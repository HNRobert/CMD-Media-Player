//
//  player-core.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#include "cmd-media-player/player-basic.hpp"
#include "cmd-media-player/render-basic.hpp"

const char *ASCII_SEQ_LONGEST = "@%#*+^=~-;:,'.` ";
const char *ASCII_SEQ_LONGER = "@%#*+=~-:,. ";
const char *ASCII_SEQ_LONG = "@%#*+=-:. ";
const char *ASCII_SEQ_SHORT = "@#*+-:. ";
const char *ASCII_SEQ_SHORTER = "@#*-. ";
const char *ASCII_SEQ_SHORTEST = "@+. ";

std::vector<std::string> ascii_char_sets = {
    ASCII_SEQ_SHORTEST,
    ASCII_SEQ_SHORTER,
    ASCII_SEQ_SHORT,
    ASCII_SEQ_LONG,
    ASCII_SEQ_LONGER,
    ASCII_SEQ_LONGEST};

int current_char_set_index = 2; // Default to ASCII_SEQ_SHORT

int volume = SDL_MIX_MAXVOLUME;
SDL_AudioSpec audio_spec;
SDL_AudioDeviceID audio_device_id = 0;

int NO_VIDEO_THRESHOLD = 20;

void adjust_volume(int change) {
    SDL_LockAudioDevice(audio_device_id);
    volume = std::clamp(volume + change, 0, SDL_MIX_MAXVOLUME);
    SDL_UnlockAudioDevice(audio_device_id);
}

void volume_up() {
    adjust_volume(SDL_MIX_MAXVOLUME / 10);
}

void volume_down() {
    adjust_volume(-SDL_MIX_MAXVOLUME / 10);
}

const std::map<std::string, std::function<std::string(const cv::Mat &, int, const char *)>> param_func_pair = {
    {"dy", image_to_ascii_dy_contrast},
    {"st", image_to_ascii}};

// Global variable to handle Ctrl+C
bool quit = false;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    quit = true;
}

void control_frame_rate(const std::chrono::high_resolution_clock::time_point &start_time, int frame_delay) {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    int remaining_delay = frame_delay - static_cast<int>(processing_time.count()) - 4;
    if (remaining_delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(remaining_delay));
    }
}

void seek_for(int seek_seconds, bool debug_mode, int64_t &current_time, int64_t total_duration,
              AVFormatContext *format_ctx, AVCodecContext *audio_codec_ctx, AVCodecContext *video_codec_ctx,
              bool has_audio, bool has_video) {
    // Calculate the target time
    int64_t target_time = current_time + seek_seconds;

    // Limit the target time to the range [0, total_duration]
    target_time = std::clamp(target_time, int64_t(0), int64_t(total_duration));

    // Convert the target time to timestamp
    int64_t target_ts = target_time * AV_TIME_BASE;

    // Set the seek flags
    int flags = 0;
    if (seek_seconds < 0) {
        flags |= AVSEEK_FLAG_BACKWARD;
    }

    // Seek to the target timestamp
    if (av_seek_frame(format_ctx, -1, target_ts, flags) >= 0) {
        // Only flush audio buffers if audio stream exists
        if (has_audio && audio_codec_ctx) {
            avcodec_flush_buffers(audio_codec_ctx);
        }

        // Only flush video buffers if video stream exists
        if (has_video && video_codec_ctx) {
            avcodec_flush_buffers(video_codec_ctx);
        }

        current_time = target_time;
    } else {
        if (debug_mode) {
            std::cerr << "Error: Seek operation failed." << std::endl;
        }
    }
}

bool initialize_video(AVFormatContext *format_ctx, VideoContext &video_ctx, bool debug_mode) {
    video_ctx = {nullptr, nullptr, -1, 0.0};

    // Find video stream
    for (int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream *stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_ctx.stream_index < 0) {
            video_ctx.stream = stream;
            video_ctx.stream_index = i;
            break;
        }
    }

    if (!video_ctx.stream) {
        if (debug_mode)
            print_error("Error: Could not find video stream.");
        return false;
    }

    // Initialize video codec
    const AVCodec *video_codec = avcodec_find_decoder(video_ctx.stream->codecpar->codec_id);
    if (!video_codec) {
        if (debug_mode)
            print_error("Error: Could not find video codec.");
        return false;
    }

    video_ctx.codec_ctx = avcodec_alloc_context3(video_codec);
    if (avcodec_parameters_to_context(video_ctx.codec_ctx, video_ctx.stream->codecpar) < 0) {
        if (debug_mode)
            print_error("Error: Could not copy video codec parameters.");
        return false;
    }

    if (avcodec_open2(video_ctx.codec_ctx, video_codec, nullptr) < 0) {
        avcodec_free_context(&video_ctx.codec_ctx);
        if (debug_mode)
            print_error("Error: Could not open video codec.");
        return false;
    }

    video_ctx.fps = av_q2d(video_ctx.stream->avg_frame_rate);
    return true;
}

bool initialize_audio(AVFormatContext *format_ctx, AudioContext &audio_ctx, bool debug_mode) {
    audio_ctx = {nullptr, nullptr, -1, nullptr, {}, {}};

    // Find audio stream
    for (int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream *stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_ctx.stream_index < 0) {
            audio_ctx.stream = stream;
            audio_ctx.stream_index = i;
            break;
        }
    }

    if (!audio_ctx.stream) {
        if (debug_mode)
            print_error("Error: Could not find audio stream.");
        return false;
    }

    // Initialize audio queue with timing information
    audio_ctx.queue.data = new uint8_t[AUDIO_QUEUE_SIZE];
    audio_ctx.queue.size = 0;
    audio_ctx.queue.mutex = SDL_CreateMutex();
    audio_ctx.queue.current_pts = 0;
    audio_ctx.queue.time_base = av_q2d(audio_ctx.stream->time_base);

    // Initialize audio codec
    const AVCodec *audio_codec = avcodec_find_decoder(audio_ctx.stream->codecpar->codec_id);
    if (!audio_codec) {
        if (debug_mode)
            print_error("Error: Could not find audio codec.");
        return false;
    }

    audio_ctx.codec_ctx = avcodec_alloc_context3(audio_codec);
    if (avcodec_parameters_to_context(audio_ctx.codec_ctx, audio_ctx.stream->codecpar) < 0) {
        if (debug_mode)
            print_error("Error: Could not copy audio codec parameters.");
        return false;
    }

    if (avcodec_open2(audio_ctx.codec_ctx, audio_codec, nullptr) < 0) {
        avcodec_free_context(&audio_ctx.codec_ctx);
        if (debug_mode)
            print_error("Error: Could not open audio codec.");
        return false;
    }

    // Initialize SDL audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        if (debug_mode)
            print_error("SDL_Init Error: ", SDL_GetError());
        return false;
    }

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audio_ctx.codec_ctx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = audio_ctx.codec_ctx->ch_layout.nb_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = &audio_ctx.queue;

    audio_device_id = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_ctx.spec, 0);
    if (audio_device_id == 0) {
        if (debug_mode)
            print_error("SDL_OpenAudioDevice Error: ", SDL_GetError());
        return false;
    }

    // Initialize resampler
    audio_ctx.swr_ctx = swr_alloc();
    if (!audio_ctx.swr_ctx) {
        if (debug_mode)
            print_error("Error: Could not allocate SwrContext.");
        return false;
    }

    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&audio_ctx.swr_ctx,
                            &out_ch_layout,
                            AV_SAMPLE_FMT_S16,
                            audio_ctx.spec.freq,
                            &audio_ctx.codec_ctx->ch_layout,
                            audio_ctx.codec_ctx->sample_fmt,
                            audio_ctx.codec_ctx->sample_rate,
                            0, nullptr) < 0) {
        if (debug_mode)
            print_error("Error: Could not set SwrContext options.");
        return false;
    }

    if (swr_init(audio_ctx.swr_ctx) < 0) {
        if (debug_mode)
            print_error("Error: Could not initialize SwrContext.");
        return false;
    }

    SDL_PauseAudioDevice(audio_device_id, 0);
    return true;
}

void play_media(const std::map<std::string, std::string> &params) {
    std::string media_path;
    const char *frame_chars;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func = nullptr;

    if (params.count("-m")) {
        media_path = params.at("-m");
    } else {
        print_error("No media but wanna play? Really? \nAdd a -m param, or type \"help\" to get more usage");
        return;
    }

    if (params.count("-ct") && param_func_pair.count(params.at("-ct"))) {
        generate_ascii_func = param_func_pair.at(params.at("-ct"));
    } else if (params.count("-dy")) {
        generate_ascii_func = image_to_ascii_dy_contrast;
    } else if (params.count("-st")) {
        generate_ascii_func = image_to_ascii;
    } else {
        generate_ascii_func = image_to_ascii;
    }

    if (params.count("-c") && params.at("-c").length() > 0) {
        std::string custom_chars = params.at("-c");

        // Find the position to insert the custom character set
        auto it = ascii_char_sets.begin();
        for (; it != ascii_char_sets.end(); ++it) {
            if (custom_chars.length() <= it->length()) {
                break;
            }
        }
        // Insert the custom character set
        it = ascii_char_sets.insert(it, custom_chars);
        // Update current_char_set_index
        current_char_set_index = std::distance(ascii_char_sets.begin(), it);
    } else if (params.count("-s")) {
        current_char_set_index = 2; // ASCII_SEQ_SHORT
    } else if (params.count("-l")) {
        current_char_set_index = 5; // ASCII_SEQ_LONGEST
    } else {
        current_char_set_index = 2; // Default: ASCII_SEQ_SHORT
    }

    bool debug_mode = false;
    if (params.count("--debug")) {
        debug_mode = true;
    }

    // Initialize FFmpeg
    avformat_network_init();

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (avformat_open_input(&format_ctx, media_path.c_str(), nullptr, nullptr) < 0) {
        print_error("Error: Could not open video file", media_path);
        return;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        avformat_close_input(&format_ctx);
        print_error("Error: Could not find stream info", media_path);
        return;
    }

    VideoContext video_ctx;
    AudioContext audio_ctx;
    bool has_visual = initialize_video(format_ctx, video_ctx, debug_mode);
    bool has_aural = initialize_audio(format_ctx, audio_ctx, debug_mode);

    if (!has_visual && !has_aural) {
        avformat_close_input(&format_ctx);
        print_error("Error: No valid streams found in the media file.");
        return;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *last_video_frame = av_frame_alloc();
    bool has_last_frame = false;

    int64_t total_duration = format_ctx->duration / AV_TIME_BASE;
    std::string total_time = format_time(total_duration);
    int64_t current_time = 0;

    double fps = has_visual ? video_ctx.fps : 30.0; // Use 30fps refresh rate if no video stream
    int frame_delay = static_cast<int>(1000.0 / fps);
    int termWidth, termHeight, frameWidth, frameHeight, prevTermWidth = 0, prevTermHeight = 0, w_space_count = 0, h_line_count = 0;

    NCursesHandler ncursesHandler;

    // Handle Ctrl+C
    signal(SIGINT, handle_sigint);
    quit = false;

    bool term_size_changed = true;
    int seek_seconds = 3; // Number of seconds to seek
    int no_video_count = 0;

    while (!quit && av_read_frame(format_ctx, packet) >= 0) {
        bool new_frame_received = false;
        auto start_time = std::chrono::high_resolution_clock::now();
        bool force_refresh = true;

        switch (ncursesHandler.handleInput()) {
            case UserAction::Quit:
                quit = true;
                break;
            case UserAction::KeyLeft:
                seek_for(-seek_seconds, debug_mode, current_time, total_duration, format_ctx, audio_ctx.codec_ctx, video_ctx.codec_ctx, has_aural, has_visual);
                break;
            case UserAction::KeyRight:
                seek_for(seek_seconds, debug_mode, current_time, total_duration, format_ctx, audio_ctx.codec_ctx, video_ctx.codec_ctx, has_aural, has_visual);
                break;
            case UserAction::KeyEqual:
                if (current_char_set_index < ascii_char_sets.size() - 1) {
                    current_char_set_index++;
                }
                break;
            case UserAction::KeyMinus:
                if (current_char_set_index > 0) {
                    current_char_set_index--;
                }
                break;
            case UserAction::KeyUp:
                if (audio_ctx.codec_ctx) {
                    volume_up();
                }
                break;
            case UserAction::KeyDown:
                if (audio_ctx.codec_ctx) {
                    volume_down();
                }
                break;
            default:
                force_refresh = false;
                break;
        }

        if (has_visual && packet->stream_index == video_ctx.stream_index &&
            avcodec_send_packet(video_ctx.codec_ctx, packet) >= 0) {
            while (avcodec_receive_frame(video_ctx.codec_ctx, frame) >= 0) {
                new_frame_received = true;
                no_video_count = 0;
                if (!has_last_frame) {
                    av_frame_unref(last_video_frame);
                    av_frame_ref(last_video_frame, frame);
                    has_last_frame = true;
                }
                const char *frame_chars = ascii_char_sets[current_char_set_index].c_str();

                render_video_frame(frame, video_ctx.stream, packet,
                                   termWidth, termHeight, prevTermWidth, prevTermHeight,
                                   term_size_changed, current_time, total_duration, total_time,
                                   frame_chars, false, ncursesHandler.is_paused, generate_ascii_func);

                control_frame_rate(start_time, frame_delay);
            }
        } else if (packet->stream_index == audio_ctx.stream_index && audio_ctx.codec_ctx &&
                   audio_ctx.swr_ctx && avcodec_send_packet(audio_ctx.codec_ctx, packet) >= 0) {
            while (avcodec_receive_frame(audio_ctx.codec_ctx, frame) >= 0) {
                process_audio_frame(frame, audio_ctx, quit);
            }
            if (!new_frame_received) {
                no_video_count += 1;
                force_refresh = true;
            }

            if (no_video_count > NO_VIDEO_THRESHOLD && has_last_frame && has_aural) {
                no_video_count -= 5;
                const char *frame_chars = ascii_char_sets[current_char_set_index].c_str();
                render_video_frame(last_video_frame, video_ctx.stream, packet,
                                   termWidth, termHeight, prevTermWidth, prevTermHeight,
                                   term_size_changed, current_time, total_duration, total_time,
                                   frame_chars, force_refresh, ncursesHandler.is_paused, generate_ascii_func);
            }

            current_time = std::max(av_rescale_q(packet->pts, audio_ctx.stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE, (int64_t)0);
            render_audio_only_display(current_time, total_duration, total_time, term_size_changed, ncursesHandler.is_paused, has_visual);
        }
        av_packet_unref(packet);
    }

    // Restore default Ctrl+C behavior
    signal(SIGINT, SIG_DFL);

    // Clean up
    av_frame_free(&frame);
    av_frame_free(&last_video_frame);
    av_packet_free(&packet);
    if (audio_device_id) {
        SDL_CloseAudioDevice(audio_device_id);
    }
    SDL_Quit();
    if (audio_ctx.swr_ctx) {
        swr_free(&audio_ctx.swr_ctx);
    }
    avcodec_free_context(&video_ctx.codec_ctx);
    if (audio_ctx.codec_ctx) {
        avcodec_free_context(&audio_ctx.codec_ctx);
    }
    avformat_close_input(&format_ctx);

    // Clean up audio queue
    SDL_DestroyMutex(audio_ctx.queue.mutex);
    delete[] audio_ctx.queue.data;


    if (!quit) {
        get_terminal_size(termWidth, termHeight);
        mvprintw(termHeight-1, 0, "\n");
        mvprintw(termHeight-1, 0, "Playback completed! Press any key to continue...");
        nodelay(stdscr, FALSE);
        getch();
        nodelay(stdscr, TRUE);
        ncursesHandler.cleanup();
        clear_screen();
    } else {
        ncursesHandler.cleanup();
        clear_screen();
        std::cout << "Playback interrupted!\n";
    }
    
}

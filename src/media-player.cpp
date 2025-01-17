//
//  media-player.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#include "cmd-media-player/media-player.hpp"
#include "cmd-media-player/basic-functions.hpp"

#define AUDIO_QUEUE_SIZE (1024 * 1024) // 1MB buffer


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

// ANSI escape sequence to move the cursor to the top-left corner and clear the screen
void move_cursor_to_top_left(bool clear_all = false) {
    if (clear_all) {
        clear();
    }
    mvprintw(0, 0, "");

    // Original code

    // printf("\033[H"); // Moves the cursor to (0, 0) and clears the screen
    // if (clear)
    //     printf("\033[2J");
}

void add_empty_lines_for(std::string &combined_output, int count) {
    for (int i = 0; i < count; i++) {
        combined_output += "\n"; // Return and clear the characters afterwords in this line
        // combined_output += "\n\033[K"; // Return and clear the characters afterwords in this line
    }
}

std::string image_to_ascii_dy_contrast(const cv::Mat &image,
                                       int pre_space = 0,
                                       const char *asciiChars = ASCII_SEQ_SHORT) {
    unsigned long asciiLength = strlen(asciiChars);
    std::string asciiImage;

    // Step 1: Find the maximum and the minimum depth of the pixels in the image
    double min_pixel_value, max_pixel_value;
    cv::minMaxLoc(image, &min_pixel_value, &max_pixel_value);

    // Step 2: Loop for all the pixels and adjust them (scaling)
    for (int i = 0; i < image.rows; ++i) {
        asciiImage += std::string(pre_space, ' '); // Add spaces beforehand
        for (int j = 0; j < image.cols; ++j) {
            const uchar pixel = image.at<uchar>(i, j);
            // Step 3: Scale the pixels within 0-255，and map them to ASCII character set
            const auto scaled_pixel = static_cast<uchar>(255.0 * (pixel - min_pixel_value) / (max_pixel_value - min_pixel_value));
            const char asciiChar = asciiChars[(scaled_pixel * asciiLength) / 256];
            asciiImage += asciiChar;
        }
        if (pre_space) {
            asciiImage += '\n'; // Return and clear the characters afterwords in this line
        } else {
            // asciiImage += "\033[K"; // Return and clear the characters afterwords in this line
        }
    }

    return asciiImage;
}

std::string image_to_ascii(const cv::Mat &image, int pre_space = 0,
                           const char *asciiChars = ASCII_SEQ_SHORT) {
    unsigned long asciiLength = strlen(asciiChars);
    std::string asciiImage;

    for (int i = 0; i < image.rows; ++i) {
        asciiImage += std::string(pre_space, ' ');
        for (int j = 0; j < image.cols; ++j) {
            uchar pixel = image.at<uchar>(i, j);
            char asciiChar = asciiChars[(pixel * asciiLength) / 256];
            asciiImage += asciiChar;
        }
        if (pre_space) {
            asciiImage += '\n';
        } else {
            // asciiImage += "\033[K";
        }
    }

    return asciiImage;
}

std::string generate_ascii_image(const cv::Mat &image,
                                 int pre_space,
                                 const char *asciiChars,
                                 std::string (*ascii_func)(const cv::Mat &, int, const char *)) {
    // Call the pointer to the function to switch between generating methods
    return ascii_func(image, pre_space, asciiChars);
}

std::string format_time(int64_t seconds) {
    int64_t hours = seconds / 3600;
    int64_t minutes = (seconds % 3600) / 60;
    int64_t secs = seconds % 60;
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << secs;
    return ss.str();
}

std::string create_progress_bar(double progress, int width) {
    int filled = static_cast<int>(progress * (width));
    std::string bar = std::string(filled, '+') + std::string(width - filled, '-');
    return bar;
}

void render_playback_overlay(int termHeight, int termWidth, int volume, int64_t total_duration, std::string total_time, int64_t current_time) {
    std::string time_played = format_time(current_time);
    int progress_width = termWidth - (int)time_played.length() - (int)total_time.length() - 2; // 2 for /
    double progress = static_cast<double>(current_time) / total_duration;
    std::string progress_bar = create_progress_bar(progress, progress_width);
    std::string progress_output = time_played + "\\" + create_progress_bar(progress, progress_width) + "/" + total_time + "\n";

    mvprintw(termHeight - 2, 0, "%s", progress_output.c_str());
    mvprintw(termHeight - 1, 0, "Press SPACE to pause/resume, ESC/Ctrl+C to quit");
    mvprintw(termHeight - 1, termWidth - 10, "🔈: %d%%", volume * 100 / SDL_MIX_MAXVOLUME);
    mvprintw(termHeight - 1, termWidth - 1, "▶");
    // printw("Frame time: %d ms, Frame delay: %d ms", frame_time, frame_delay);
    refresh();
}

void list_audio_devices() {
    int count = SDL_GetNumAudioDevices(0); // 0 for playback devices
    std::cout << "Available audio devices:" << std::endl;
    for (int i = 0; i < count; ++i) {
        std::cout << i << ": " << SDL_GetAudioDeviceName(i, 0) << std::endl;
    }
}

int select_audio_device() {
    list_audio_devices();
    int selection;
    std::cout << "Enter the number of the audio device you want to use: ";
    std::cin >> selection;
    return selection;
}

void print_audio_stream_info(AVStream *audio_stream, AVCodecContext *audio_codec_ctx) {
    std::cout << "\n====== Audio Stream Information ======\n";
    std::cout << "Codec: " << avcodec_get_name(audio_codec_ctx->codec_id) << std::endl;
    std::cout << "Bitrate: " << audio_codec_ctx->bit_rate << " bps" << std::endl;
    std::cout << "Sample Rate: " << audio_codec_ctx->sample_rate << " Hz" << std::endl;
    std::cout << "Channels: " << audio_codec_ctx->ch_layout.nb_channels << std::endl;
    std::cout << "Sample Format: " << av_get_sample_fmt_name(audio_codec_ctx->sample_fmt) << std::endl;
    std::cout << "Frame Size: " << audio_codec_ctx->frame_size << std::endl;
    std::cout << "Timebase: " << audio_stream->time_base.num << "/" << audio_stream->time_base.den << std::endl;

    // Print channel layout
    char channel_layout[64];
    av_channel_layout_describe(&audio_codec_ctx->ch_layout, channel_layout, sizeof(channel_layout));
    std::cout << "Channel Layout: " << channel_layout << std::endl;

    // Print codec parameters
    std::cout << "Codec Parameters:" << std::endl;
    std::cout << "  Format: " << audio_stream->codecpar->format << std::endl;
    std::cout << "  Codec Type: " << av_get_media_type_string(audio_stream->codecpar->codec_type) << std::endl;
    std::cout << "  Codec ID: " << audio_stream->codecpar->codec_id << std::endl;
    std::cout << "  Codec Tag: 0x" << std::hex << std::setw(8) << std::setfill('0') << audio_stream->codecpar->codec_tag << std::dec << std::endl;

    std::cout << "======================================\n\n";
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    auto audio_queue = static_cast<AudioQueue *>(userdata);
    SDL_memset(stream, 0, len);
    SDL_LockMutex(audio_queue->mutex);
    int copied = 0;
    while (copied < len && audio_queue->size > 0) {
        int to_copy = std::min(len - copied, audio_queue->size);

        // Apply volume control
        SDL_MixAudioFormat(stream + copied, audio_queue->data, AUDIO_S16SYS, to_copy, volume);

        audio_queue->size -= to_copy;
        memmove(audio_queue->data, audio_queue->data + to_copy, audio_queue->size);
        copied += to_copy;
    }
    SDL_UnlockMutex(audio_queue->mutex);
}

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
volatile bool quit = false;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    quit = true;
}

void control_frame_rate(const std::chrono::high_resolution_clock::time_point& start_time, int frame_delay) {
    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    int remaining_delay = frame_delay - static_cast<int>(processing_time.count()) - 4;
    if (remaining_delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(remaining_delay));
    }
}

void seek_for(int seek_seconds, bool debug_mode, int64_t &current_time, int64_t total_duration, AVFormatContext *format_ctx, AVCodecContext *audio_codec_ctx, AVCodecContext *video_codec_ctx) {
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
        avcodec_flush_buffers(audio_codec_ctx);
        avcodec_flush_buffers(video_codec_ctx);
        current_time = target_time;
    } else {
        if (debug_mode) {
            std::cerr << "Error: Audio seek failed." << std::endl;
        }
    }
}

bool initialize_video(AVFormatContext* format_ctx, VideoContext& video_ctx, bool debug_mode) {
    video_ctx = {nullptr, nullptr, -1, 0.0};
    
    // Find video stream
    for (int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream* stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_ctx.stream_index < 0) {
            video_ctx.stream = stream;
            video_ctx.stream_index = i;
            break;
        }
    }

    if (!video_ctx.stream) {
        if (debug_mode) print_error("Error: Could not find video stream.");
        return false;
    }

    // Initialize video codec
    const AVCodec* video_codec = avcodec_find_decoder(video_ctx.stream->codecpar->codec_id);
    if (!video_codec) {
        if (debug_mode) print_error("Error: Could not find video codec.");
        return false;
    }

    video_ctx.codec_ctx = avcodec_alloc_context3(video_codec);
    if (avcodec_parameters_to_context(video_ctx.codec_ctx, video_ctx.stream->codecpar) < 0) {
        if (debug_mode) print_error("Error: Could not copy video codec parameters.");
        return false;
    }

    if (avcodec_open2(video_ctx.codec_ctx, video_codec, nullptr) < 0) {
        avcodec_free_context(&video_ctx.codec_ctx);
        if (debug_mode) print_error("Error: Could not open video codec.");
        return false;
    }

    video_ctx.fps = av_q2d(video_ctx.stream->avg_frame_rate);
    return true;
}

bool initialize_audio(AVFormatContext* format_ctx, AudioContext& audio_ctx, bool debug_mode) {
    audio_ctx = {nullptr, nullptr, -1, nullptr, {}, {}};
    
    // Find audio stream
    for (int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream* stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_ctx.stream_index < 0) {
            audio_ctx.stream = stream;
            audio_ctx.stream_index = i;
            break;
        }
    }

    if (!audio_ctx.stream) {
        if (debug_mode) print_error("Error: Could not find audio stream.");
        return false;
    }

    // Initialize audio queue
    audio_ctx.queue.data = new uint8_t[AUDIO_QUEUE_SIZE];
    audio_ctx.queue.size = 0;
    audio_ctx.queue.mutex = SDL_CreateMutex();

    // Initialize audio codec
    const AVCodec* audio_codec = avcodec_find_decoder(audio_ctx.stream->codecpar->codec_id);
    if (!audio_codec) {
        if (debug_mode) print_error("Error: Could not find audio codec.");
        return false;
    }

    audio_ctx.codec_ctx = avcodec_alloc_context3(audio_codec);
    if (avcodec_parameters_to_context(audio_ctx.codec_ctx, audio_ctx.stream->codecpar) < 0) {
        if (debug_mode) print_error("Error: Could not copy audio codec parameters.");
        return false;
    }

    if (avcodec_open2(audio_ctx.codec_ctx, audio_codec, nullptr) < 0) {
        avcodec_free_context(&audio_ctx.codec_ctx);
        if (debug_mode) print_error("Error: Could not open audio codec.");
        return false;
    }

    // Initialize SDL audio
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        if (debug_mode) print_error("SDL_Init Error: ", SDL_GetError());
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
        if (debug_mode) print_error("SDL_OpenAudioDevice Error: ", SDL_GetError());
        return false;
    }

    // Initialize resampler
    audio_ctx.swr_ctx = swr_alloc();
    if (!audio_ctx.swr_ctx) {
        if (debug_mode) print_error("Error: Could not allocate SwrContext.");
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
        if (debug_mode) print_error("Error: Could not set SwrContext options.");
        return false;
    }

    if (swr_init(audio_ctx.swr_ctx) < 0) {
        if (debug_mode) print_error("Error: Could not initialize SwrContext.");
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
    if (!packet || !frame) {
        if (debug_mode)
            print_error("Error: Could not allocate packet or frame.");
        return;
    }
    
    int64_t total_duration = format_ctx->duration / AV_TIME_BASE;
    std::string total_time = format_time(total_duration);
    int64_t current_time = 0;

    double fps = video_ctx.fps;
    int frame_delay = static_cast<int>(1000.0 / fps);
    int termWidth, termHeight, frameWidth, frameHeight, prevTermWidth = 0, prevTermHeight = 0, w_space_count = 0, h_line_count = 0;

    NCursesHandler ncursesHandler;

    // Handle Ctrl+C
    signal(SIGINT, handle_sigint);
    quit = false;

    bool term_size_changed = true;
    int seek_seconds = 3; // Number of seconds to seek

    while (!quit && av_read_frame(format_ctx, packet) >= 0) {
        auto start_time = std::chrono::high_resolution_clock::now();

        switch (ncursesHandler.handleInput()) {
            case UserAction::Quit:
                quit = true;
                break;
            case UserAction::KeyLeft:
                seek_for(-seek_seconds, debug_mode, current_time, total_duration, format_ctx, audio_ctx.codec_ctx, video_ctx.codec_ctx);
                break;
            case UserAction::KeyRight:
                seek_for(seek_seconds, debug_mode, current_time, total_duration, format_ctx, audio_ctx.codec_ctx, video_ctx.codec_ctx);
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
                break;
        }

        if (packet->stream_index == video_ctx.stream_index && avcodec_send_packet(video_ctx.codec_ctx, packet) >= 0) {
            while (avcodec_receive_frame(video_ctx.codec_ctx, frame) >= 0) {
                // Convert to grayscale image
                cv::Mat grayFrame(frame->height, frame->width, CV_8UC1);
                for (int y = 0; y < frame->height; ++y) {
                    for (int x = 0; x < frame->width; ++x) {
                        grayFrame.at<uchar>(y, x) = frame->data[0][y * frame->linesize[0] + x];
                    }
                }
                // Get terminal size and resize frame
                get_terminal_size(termWidth, termHeight);
                if (termWidth != prevTermWidth || termHeight != prevTermHeight) {
                    prevTermWidth = termWidth;
                    prevTermHeight = termHeight;
                    term_size_changed = true;

                } else
                    term_size_changed = false;
                frameWidth = termWidth;
                frameHeight = (grayFrame.rows * frameWidth) / grayFrame.cols / 2;
                w_space_count = 0;
                h_line_count = (termHeight - frameHeight - 2) / 2;
                if (frameHeight > termHeight - 2) {
                    frameHeight = termHeight - 2;
                    frameWidth = (grayFrame.cols * frameHeight * 2) / grayFrame.rows;
                    w_space_count = (termWidth - frameWidth) / 2;
                    h_line_count = 0;
                }
                cv::resize(grayFrame, grayFrame, cv::Size(frameWidth, frameHeight));

                // Convert image to ASCII and display
                const char *frame_chars = ascii_char_sets[current_char_set_index].c_str();
                std::string asciiArt = generate_ascii_func(grayFrame,
                                                           w_space_count,
                                                           frame_chars);
                current_time = av_rescale_q(packet->pts, video_ctx.stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;

                // Combine ASCII art with progress bar
                std::string combined_output;
                add_empty_lines_for(combined_output, h_line_count);
                combined_output += asciiArt;
                add_empty_lines_for(combined_output,
                                    termHeight - frameHeight - h_line_count);

                // clear_screen();
                move_cursor_to_top_left(term_size_changed);
                printw("%s", combined_output.c_str()); // Show the Frame
                render_playback_overlay(termHeight, termWidth, volume, total_duration, total_time, current_time);

                // Frame rate control
                control_frame_rate(start_time, frame_delay);
            }
        } else if (packet->stream_index == audio_ctx.stream_index && audio_ctx.codec_ctx && audio_ctx.swr_ctx && avcodec_send_packet(audio_ctx.codec_ctx, packet) >= 0) {
            while (avcodec_receive_frame(audio_ctx.codec_ctx, frame) >= 0) {
                int out_samples = (int)av_rescale_rnd(swr_get_delay(audio_ctx.swr_ctx, audio_ctx.codec_ctx->sample_rate) + frame->nb_samples,
                                                      audio_ctx.spec.freq, audio_ctx.codec_ctx->sample_rate, AV_ROUND_UP);
                uint8_t *out_buffer;
                av_samples_alloc(&out_buffer, nullptr, audio_ctx.spec.channels, out_samples, AV_SAMPLE_FMT_S16, 0);
                int samples_out = swr_convert(audio_ctx.swr_ctx, &out_buffer, out_samples,
                                              (const uint8_t **)frame->data, frame->nb_samples);
                if (samples_out > 0) {
                    int buffer_size = av_samples_get_buffer_size(nullptr, audio_ctx.spec.channels, samples_out, AV_SAMPLE_FMT_S16, 1);
                    SDL_LockMutex(audio_ctx.queue.mutex);
                    if (audio_ctx.queue.size + buffer_size < AUDIO_QUEUE_SIZE) {
                        // Apply volume control
                        memcpy(audio_ctx.queue.data + audio_ctx.queue.size, out_buffer, buffer_size);
                        // SDL_MixAudioFormat(audio_queue.data + audio_queue.size, out_buffer, AUDIO_S16SYS, buffer_size, volume);
                        audio_ctx.queue.size += buffer_size;
                    } else {
                        // std::cout << "Audio queue full. Discarding " << buffer_size << " bytes.";
                    }
                    SDL_UnlockMutex(audio_ctx.queue.mutex);
                } else {
                    // std::cout << "No samples output from swr_convert";
                }

                if (!has_visual) {
                    current_time = av_rescale_q(packet->pts, audio_ctx.stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
                    render_playback_overlay(termHeight, termWidth, volume, total_duration, total_time, current_time);
                    // Frame rate control
                    control_frame_rate(start_time, frame_delay);
                }

                av_freep(&out_buffer);
            }
        }

        av_packet_unref(packet);
    }

    // Restore default Ctrl+C behavior
    signal(SIGINT, SIG_DFL);

    // Clean up
    av_frame_free(&frame);
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

    ncursesHandler.cleanup();

    if (!quit) {
        clear_screen();
        std::cout << "Playback completed!\n";
    } else {
        clear_screen();
        std::cout << "Playback interrupted!\n";
    }
}

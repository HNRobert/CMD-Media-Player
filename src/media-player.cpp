//
//  media-player.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#include "cmd-media-player/media-player.hpp"
#include "cmd-media-player/basic-functions.hpp"

const int AUDIO_QUEUE_SIZE = 1024 * 1024; // 1MB buffer

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

std::string image_to_ascii_dy_contrast(const cv::Mat &image,
                                       int pre_space,
                                       const char *asciiChars) {
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
            asciiImage += "\n"; // Return and clear the characters afterwords in this line
        } else {
            // asciiImage += "\033[K"; // Return and clear the characters afterwords in this line
        }
    }

    return asciiImage;
}

std::string image_to_ascii(const cv::Mat &image, int pre_space,
                           const char *asciiChars) {
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
                                 std::function<std::string(const cv::Mat &, int, const char *)> ascii_func) {
    // Call the pointer to the function to switch between generating methods
    return ascii_func(image, pre_space, asciiChars);
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

std::string create_progress_bar(double progress, int width) {
    int filled = static_cast<int>(progress * (width));
    std::string bar = std::string(filled, '+') + std::string(width - filled, '-');
    return bar;
}

void render_playback_overlay(int termHeight, int termWidth, const std::string &progress_output, int volume) {
    mvprintw(termHeight - 2, 0, "%s", progress_output.c_str());
    mvprintw(termHeight - 1, 0, "Press SPACE to pause/resume, ESC/Ctrl+C to quit");
    mvprintw(termHeight - 1, termWidth - 10, "🔈: %d%%", volume * 100 / SDL_MIX_MAXVOLUME);
    mvprintw(termHeight - 1, termWidth - 1, "▶");
    // printw("Frame time: %d ms, Frame delay: %d ms", frame_time, frame_delay);
    refresh();
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

const std::map<std::string, std::function<std::string(const cv::Mat &, int, const char *)>> param_func_pair = {
    {"dy", image_to_ascii_dy_contrast},
    {"st", image_to_ascii}};

const std::map<std::string, std::string> char_set_pairs = {
    {"s", ASCII_SEQ_SHORT},
    {"S", ASCII_SEQ_SHORT},
    {"l", ASCII_SEQ_LONG},
    {"L", ASCII_SEQ_LONG}};

// Global variable to handle Ctrl+C
volatile bool quit = false;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    quit = true;
}

void play_media(const std::map<std::string, std::string> &params) {
    std::string media_path;

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
        if (debug_mode)
            print_error("Error: Could not open media file", media_path);
        return;
    }

    if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
        avformat_close_input(&format_ctx);
        if (debug_mode)
            print_error("Error: Could not find stream info", media_path);
        return;
    }

    AudioPlayer audioPlayer;
    VideoPlayer videoPlayer;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    if (!packet || !frame) {
        if (debug_mode)
            print_error("Error: Could not allocate packet or frame.");
        return;
    }

    audioPlayer.frame = frame;
    audioPlayer.format_ctx = format_ctx;
    audioPlayer.debug_mode = &debug_mode;

    videoPlayer.frame = frame;
    videoPlayer.format_ctx = format_ctx;
    videoPlayer.debug_mode = &debug_mode;

    // Find video and audio streams
    audioPlayer.initialize();
    videoPlayer.initialize();

    if (audioPlayer.initialized) {
        videoPlayer.volume = &audioPlayer.volume;
    }

    int64_t total_duration = format_ctx->duration / AV_TIME_BASE;
    int64_t current_time = 0;
    std::string time_played;
    std::string total_time;

    double fps = 30.0;
    if (videoPlayer.initialized)
        fps = av_q2d(videoPlayer.video_stream->avg_frame_rate);
    else if (audioPlayer.initialized)
        fps = av_q2d(audioPlayer.audio_stream->avg_frame_rate);

    int frame_delay = static_cast<int>(1000.0 / fps);
    int termWidth, termHeight, frameWidth, frameHeight, prevTermWidth = 0, prevTermHeight = 0, w_space_count = 0, h_line_count = 0;
    bool term_size_changed = true;

    Uint32 frame_start = 0;
    std::string progress_output;

    videoPlayer.termWidth = &termWidth;
    videoPlayer.termHeight = &termHeight;
    videoPlayer.frame_delay = &frame_delay;
    videoPlayer.frame_start = &frame_start;
    videoPlayer.progress_output = &progress_output;
    videoPlayer.term_size_changed = &term_size_changed;
    videoPlayer.generate_ascii_func = generate_ascii_func;

    audioPlayer.termWidth = &termWidth;
    audioPlayer.termHeight = &termHeight;
    audioPlayer.frame_delay = &frame_delay;
    audioPlayer.frame_start = &frame_start;
    audioPlayer.progress_output = &progress_output;
    audioPlayer.term_size_changed = &term_size_changed;

    int seek_seconds = 3; // Number of seconds to seek
    quit = false;

    NCursesHandler ncursesHandler;
    clear();

    // Handle Ctrl+C
    signal(SIGINT, handle_sigint);

    while (!quit && av_read_frame(format_ctx, packet) >= 0) {
        frame_start = SDL_GetTicks();

        switch (ncursesHandler.handleInput()) {
            case UserAction::Quit:
                quit = true;
                break;
            case UserAction::KeyLeft:
                seek_for(-seek_seconds, debug_mode, current_time, total_duration, format_ctx, audioPlayer.audio_codec_ctx, videoPlayer.video_codec_ctx);
                break;
            case UserAction::KeyRight:
                seek_for(seek_seconds, debug_mode, current_time, total_duration, format_ctx, audioPlayer.audio_codec_ctx, videoPlayer.video_codec_ctx);
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
                if (audioPlayer.audio_codec_ctx) {
                    audioPlayer.volume_up();
                }
                break;
            case UserAction::KeyDown:
                if (audioPlayer.audio_codec_ctx) {
                    audioPlayer.volume_down();
                }
                break;
            default:
                break;
        }

        get_terminal_size(termWidth, termHeight);
        if (termWidth != prevTermWidth || termHeight != prevTermHeight) {
            prevTermWidth = termWidth;
            prevTermHeight = termHeight;
            term_size_changed = true;
        } else {
            term_size_changed = false;
        }

        time_played = format_time(current_time);
        total_time = format_time(total_duration);
        int progress_width = termWidth - (int)time_played.length() - (int)total_time.length() - 2; // 2 for \/
        double progress = static_cast<double>(current_time) / total_duration;
        progress_output = time_played + "\\" + create_progress_bar(progress, progress_width) + "/" + total_time + "\r";

        if (packet->stream_index == videoPlayer.video_stream_index) {
            current_time = av_rescale_q(packet->pts, videoPlayer.video_stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
            videoPlayer.handle_packet(packet);
        } else if (packet->stream_index == audioPlayer.audio_stream_index) {
            current_time = av_rescale_q(packet->pts, audioPlayer.audio_stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
            audioPlayer.handle_packet(packet, videoPlayer.initialized);
        } else {
            av_packet_unref(packet);
        }
    }
    av_packet_free(&packet);
    av_frame_free(&frame);

    // Restore default Ctrl+C behavior
    signal(SIGINT, SIG_DFL);

    // Clean up
    audioPlayer.cleanup();
    videoPlayer.cleanup();
    avformat_close_input(&format_ctx);

    ncursesHandler.cleanup();

    if (!quit) {
        clear_screen();
        std::cout << "Playback completed!\n";
    } else {
        clear_screen();
        std::cout << "Playback interrupted!\n";
    }
}

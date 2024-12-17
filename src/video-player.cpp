//
//  video-player.cpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#include "cmd-media-player/video-player.hpp"
#include "cmd-media-player/basic-functions.hpp"

#define AUDIO_QUEUE_SIZE (1024 * 1024) // 1MB buffer

struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
};

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
                                 std::function<std::string(const cv::Mat &, int, const char *)> ascii_func) {
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

enum class UserAction {
    None,
    Quit,
    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown
};

class NCursesHandler {
  private:
    bool is_paused = false;
    bool has_quitted = false;

  public:
    NCursesHandler() {
        initscr();
        cbreak();
        noecho();
        keypad(stdscr, TRUE);
        nodelay(stdscr, TRUE);
    }

    ~NCursesHandler() {
        cleanup();
    }

    void cleanup() {
        if (!has_quitted) {
            has_quitted = true;
            endwin();
        }
    }

    UserAction handle_space() {
        nodelay(stdscr, FALSE);
        printw("Paused. Press SPACE to resume.");
        refresh();
        UserAction res = handleInput();
        nodelay(stdscr, TRUE);
        return res;
    }

    UserAction handleInput() {
        switch (getch()) {
            case ERR:
                return UserAction::None;
            case 27: // ESC key
            case 3:  // Ctrl+C
                return UserAction::Quit;
            case ' ':
                if (is_paused) {
                    is_paused = false;
                    return UserAction::None;
                } else {
                    is_paused = true;
                    return handle_space();
                }
            case KEY_LEFT:
                return UserAction::KeyLeft;
            case KEY_RIGHT:
                return UserAction::KeyRight;
            case KEY_UP:
                return UserAction::KeyUp;
            case KEY_DOWN:
                return UserAction::KeyDown;
            default:
                return UserAction::None;
        }
    }
};

void seek_video(AVFormatContext *format_ctx, AVCodecContext *video_codec_ctx,
                int video_stream_index, int64_t &current_time, int64_t total_duration,
                int seek_seconds, bool debug_mode) {
    // Calculate the target time
    int64_t target_time = current_time + seek_seconds;

    // Limit the target time to the range [0, total_duration]
    target_time = std::clamp(target_time, int64_t(0), total_duration);

    // Convert the target time to timestamp
    int64_t target_ts = target_time * AV_TIME_BASE;

    // Set the seek flags
    int flags = 0;
    if (seek_seconds < 0) {
        flags |= AVSEEK_FLAG_BACKWARD;
    }

    // Seek to the target timestamp
    if (av_seek_frame(format_ctx, -1, target_ts, flags) >= 0) {
        avcodec_flush_buffers(video_codec_ctx);
        current_time = target_time; // Update the current time
    } else {
        if (debug_mode) {
            print_error("Error: Seek failed.");
        }
    }
}

class AudioPlayer {
  public:
    AudioPlayer() : audio_codec_ctx(nullptr), audio_device_id(0), swr_ctx(nullptr) {
        audio_queue.data = new uint8_t[AUDIO_QUEUE_SIZE];
        audio_queue.size = 0;
        audio_queue.mutex = SDL_CreateMutex();
    }

    ~AudioPlayer() {
        cleanup();
    }

    bool initialize(AVFormatContext *format_ctx, int &audio_stream_index) {
        // Initialize the audio decoder
        AVStream *audio_stream = nullptr;
        // Find the audio stream
        for (int i = 0; i < format_ctx->nb_streams; ++i) {
            AVStream *stream = format_ctx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index < 0) {
                audio_stream = stream;
                audio_stream_index = i;
                break;
            }
        }

        if (!audio_stream) {
            // Audio stream not found
            return false;
        }

        const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        if (!audio_codec) {
            // Codec not found
            return false;
        }

        audio_codec_ctx = avcodec_alloc_context3(audio_codec);
        if (avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar) < 0) {
            // Copying audio parameters failed
            return false;
        }

        if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
            // Cannot open audio codec
            return false;
        }

        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            // SDL Initialization failed
            return false;
        }

        SDL_AudioSpec wanted_spec;
        wanted_spec.freq = audio_codec_ctx->sample_rate;
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.channels = audio_codec_ctx->ch_layout.nb_channels;
        wanted_spec.silence = 0;
        wanted_spec.samples = 1024;
        wanted_spec.callback = [](void *userdata, Uint8 *stream, int len) {
            static_cast<AudioPlayer *>(userdata)->audio_callback(userdata, stream, len);
        };
        wanted_spec.userdata = this;

        audio_device_id = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);
        if (audio_device_id == 0) {
            // Cannot open audio device
            return false;
        }

        swr_ctx = swr_alloc();
        if (!swr_ctx) {
            // Cannot allocate SwrContext
            return false;
        }

        AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
        if (swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16, spec.freq,
                                &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                                0, nullptr) < 0) {
            // Cannot set SwrContext options
            return false;
        }

        if (swr_init(swr_ctx) < 0) {
            // Cannot initialize SwrContext
            return false;
        }

        return true;
    }

    void start() {
        SDL_PauseAudioDevice(audio_device_id, 0);
    }

    void handle_packet(AVPacket *packet) {
        if (avcodec_send_packet(audio_codec_ctx, packet) >= 0) {
            AVFrame *frame = av_frame_alloc();
            while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
                // Handle audio frame
                int out_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, audio_codec_ctx->sample_rate) + frame->nb_samples,
                                                      spec.freq, audio_codec_ctx->sample_rate, AV_ROUND_UP);
                uint8_t *out_buffer;
                av_samples_alloc(&out_buffer, nullptr, spec.channels, out_samples, AV_SAMPLE_FMT_S16, 0);
                int samples_out = swr_convert(swr_ctx, &out_buffer, out_samples,
                                              (const uint8_t **)frame->data, frame->nb_samples);
                if (samples_out > 0) {
                    int buffer_size = av_samples_get_buffer_size(nullptr, spec.channels, samples_out, AV_SAMPLE_FMT_S16, 1);
                    SDL_LockMutex(audio_queue.mutex);
                    if (audio_queue.size + buffer_size < AUDIO_QUEUE_SIZE) {
                        memcpy(audio_queue.data + audio_queue.size, out_buffer, buffer_size);
                        audio_queue.size += buffer_size;
                    }
                    SDL_UnlockMutex(audio_queue.mutex);
                }
                av_freep(&out_buffer);
            }
            av_frame_free(&frame);
        }
    }

    void cleanup() {
        if (audio_device_id) {
            SDL_CloseAudioDevice(audio_device_id);
            audio_device_id = 0;
        }
        if (swr_ctx) {
            swr_free(&swr_ctx);
            swr_ctx = nullptr;
        }
        if (audio_codec_ctx) {
            avcodec_free_context(&audio_codec_ctx);
            audio_codec_ctx = nullptr;
        }
        SDL_DestroyMutex(audio_queue.mutex);
        delete[] audio_queue.data;
        SDL_Quit();
    }

  private:
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

    AudioQueue audio_queue;
    SDL_AudioSpec spec;
    AVCodecContext *audio_codec_ctx;
    SDL_AudioDeviceID audio_device_id;
    SwrContext *swr_ctx;
};

class VideoPlayer {
  public:
    VideoPlayer() : video_codec_ctx(nullptr), prevTermWidth(0), prevTermHeight(0), term_size_changed(true), generate_ascii_func(image_to_ascii) {}

    ~VideoPlayer() {
        cleanup();
    }

    int frame_delay;
    AVCodecContext *video_codec_ctx;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func;

    bool initialize(AVFormatContext *format_ctx, int &video_stream_index) {
        // Initialize the video decoder
        AVStream *video_stream = nullptr;
        for (int i = 0; i < format_ctx->nb_streams; ++i) {
            AVStream *stream = format_ctx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0) {
                video_stream = stream;
                video_stream_index = i;
                break;
            }
        }

        if (!video_stream) {
            // Video stream not found
            return false;
        }

        const AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!video_codec) {
            // Video codec not found
            return false;
        }

        video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar) < 0) {
            // Failed to copy video parameters
            return false;
        }

        if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
            // Failed to open video codec
            return false;
        }

        return true;
    }

    void handle_packet(AVPacket *packet) {
        if (avcodec_send_packet(video_codec_ctx, packet) >= 0) {
            AVFrame *frame = av_frame_alloc();
            while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                // Handle video frame
                render_frame(frame, current_time, total_duration);
            }
            av_frame_free(&frame);
        }
    }

    void render_frame(AVFrame *frame, int64_t &current_time, int64_t total_duration) {
        // Convert AVFrame to grayscale image
        cv::Mat grayFrame(frame->height, frame->width, CV_8UC1);
        for (int y = 0; y < frame->height; ++y) {
            memcpy(grayFrame.ptr(y), frame->data[0] + y * frame->linesize[0], frame->width);
        }

        // Adjust terminal size and frame dimensions
        get_terminal_size(termWidth, termHeight);
        termHeight -= 2;
        if (termWidth != prevTermWidth || termHeight != prevTermHeight) {
            prevTermWidth = termWidth;
            prevTermHeight = termHeight;
            term_size_changed = true;
        } else {
            term_size_changed = false;
        }

        // ...existing code for resizing, ASCII conversion, and display...
        // Resize the frame to fit the terminal dimensions
        if (term_size_changed) {
            double aspect_ratio = static_cast<double>(frame->width) / frame->height;
            frameWidth = termWidth;
            frameHeight = static_cast<int>(frameWidth / aspect_ratio);
            if (frameHeight > termHeight) {
                frameHeight = termHeight;
                frameWidth = static_cast<int>(frameHeight * aspect_ratio);
            }
            w_space_count = (termWidth - frameWidth) / 2;
            h_line_count = (termHeight - frameHeight) / 2;
        }

        // Resize the frame to fit the terminal dimensions
        cv::Mat resizedFrame;
        cv::resize(grayFrame, resizedFrame, cv::Size(frameWidth, frameHeight));

        // Convert the resized frame to ASCII
        std::string asciiImage = generate_ascii_image(resizedFrame, w_space_count, ascii_char_sets[current_char_set_index].c_str(), generate_ascii_func);

        // Move cursor to top-left and clear the screen if needed
        move_cursor_to_top_left(term_size_changed);

        // Print the ASCII image
        printw("%s", asciiImage.c_str());

        // Print the progress bar and time
        double progress = static_cast<double>(current_time) / total_duration;
        std::string progressBar = create_progress_bar(progress, termWidth);
        std::string timeInfo = format_time(current_time) + " / " + format_time(total_duration);
        mvprintw(termHeight, 0, "%s", progressBar.c_str());
        mvprintw(termHeight + 1, 0, "%s", timeInfo.c_str());

        // Refresh the screen
        refresh();

        // Control frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay));
    }

    void cleanup() {
        if (video_codec_ctx) {
            avcodec_free_context(&video_codec_ctx);
            video_codec_ctx = nullptr;
        }
    }

  private:
    int termWidth, termHeight;
    int prevTermWidth, prevTermHeight;
    bool term_size_changed;
    int frameWidth, frameHeight;
    int w_space_count, h_line_count;
    int64_t current_time;
    int64_t total_duration;
};

void play_video(const std::map<std::string, std::string> &params) {
    std::string media_path;
    const char *frame_chars;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func = nullptr;
    bool has_visual = false;
    bool has_aural = false;

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
        current_char_set_index = 3; // ASCII_SEQ_LONG
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

    AudioPlayer audioPlayer;
    VideoPlayer videoPlayer;

    int audio_stream_index = -1;
    int video_stream_index = -1;

    bool has_audio = audioPlayer.initialize(format_ctx, audio_stream_index);
    bool has_video = videoPlayer.initialize(format_ctx, video_stream_index);

    if (has_audio) {
        audioPlayer.start();
    }

    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        // 错误处理
        return;
    }

    int64_t total_duration = format_ctx->duration / AV_TIME_BASE;
    int64_t current_time = 0;

    double fps = av_q2d(format_ctx->streams[video_stream_index]->avg_frame_rate);
    videoPlayer.frame_delay = static_cast<int>(1000.0 / fps);
    int termWidth, termHeight, frameWidth, frameHeight, prevTermWidth = 0, prevTermHeight = 0, w_space_count = 0, h_line_count = 0;

    NCursesHandler ncursesHandler;

    // Handle Ctrl+C
    signal(SIGINT, handle_sigint);
    quit = false;

    bool term_size_changed = true;
    int volume = SDL_MIX_MAXVOLUME;
    int seek_seconds = 3; // Number of seconds to seek

    while (!quit && av_read_frame(format_ctx, packet) >= 0) {
        auto start_time = std::chrono::high_resolution_clock::now();

        switch (ncursesHandler.handleInput()) {
            case UserAction::Quit:
                quit = true;
                break;
            case UserAction::KeyLeft:
                seek_video(format_ctx, videoPlayer.video_codec_ctx, video_stream_index, current_time, total_duration, -seek_seconds, debug_mode);
                break;
            case UserAction::KeyRight:
                seek_video(format_ctx, videoPlayer.video_codec_ctx, video_stream_index, current_time, total_duration, seek_seconds, debug_mode);
                break;
            case UserAction::KeyUp:
                if (current_char_set_index < ascii_char_sets.size() - 1) {
                    current_char_set_index++;
                }
                break;
            case UserAction::KeyDown:
                if (current_char_set_index > 0) {
                    current_char_set_index--;
                }
                break;
            default:
                break;
        }

        if (packet->stream_index == video_stream_index) {
            videoPlayer.handle_packet(packet);
        } else if (packet->stream_index == audio_stream_index) {
            audioPlayer.handle_packet(packet);
        }

        av_packet_unref(packet);
    }

    // Restore default Ctrl+C behavior
    signal(SIGINT, SIG_DFL);

    // Clean up
    audioPlayer.cleanup();
    videoPlayer.cleanup();
    av_packet_free(&packet);
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

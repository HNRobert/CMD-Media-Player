//
//  video-player.cpp
//  CMD-Video-Player
//
//  Created by Robert He on 2024/9/2.
//

#include "basic-functions.hpp"
#include "video-player.hpp"

#define AUDIO_QUEUE_SIZE 1024 * 1024 // 1MB buffer

bool is_escape_key_pressed() {
#ifdef _WIN32
    // Windows-specific code to check if the ESC key is pressed
    return GetAsyncKeyState(VK_ESCAPE) & 0x8000;
#else
    // For Linux and macOS, we can implement a similar functionality if needed.
    // Here, you could use termios.h or another library to handle keypresses.
    return false; // Placeholder; you might want to implement platform-specific code here.
#endif
}

struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
};

const char *ASCII_SEQ_LONG = "@%#*+^=~-;:,'.` ";
const char *ASCII_SEQ_SHORT = "@#*+-:. ";

// ANSI escape sequence to move the cursor to the top-left corner and clear the screen
void move_cursor_to_top_left(bool clear = false) {
    printf("\033[H"); // Moves the cursor to (0, 0) and clears the screen
    if (clear)
        printf("\033[2J");
}

void add_empty_lines_for(std::string &combined_output, int count) {
    for (int i = 0; i < count; i++) {
        combined_output += "\n\033[K"; // 换行并清除该行到行末的字符
    }
}

std::string image_to_ascii_dy_contrast(const cv::Mat &image,
                                       int pre_space = 0,
                                       const char *asciiChars = ASCII_SEQ_SHORT) {
    unsigned long asciiLength = strlen(asciiChars);
    std::string asciiImage;

    // Step 1: 计算图像的最小和最大像素值
    double min_pixel_value, max_pixel_value;
    cv::minMaxLoc(image, &min_pixel_value, &max_pixel_value);

    // Step 2: 遍历图像像素，并根据灰度范围缩放像素值
    for (int i = 0; i < image.rows; ++i) {
        asciiImage += std::string(pre_space, ' '); // 添加前置空格
        for (int j = 0; j < image.cols; ++j) {
            uchar pixel = image.at<uchar>(i, j);
            // Step 3: 将像素值缩放到 0-255，并映射到 ASCII 字符集
            uchar scaled_pixel = static_cast<uchar>(255.0 * (pixel - min_pixel_value) / (max_pixel_value - min_pixel_value));
            char asciiChar = asciiChars[(scaled_pixel * asciiLength) / 256];
            asciiImage += asciiChar;
        }
        if (pre_space)
            asciiImage += "\033[K";
        asciiImage += '\n';
    }

    return asciiImage;
}

std::string image_to_ascii(const cv::Mat &image, int pre_space = 0,
                           const char *asciiChars = ASCII_SEQ_SHORT) {
    // @%#*+=-:.
    unsigned long asciiLength = strlen(asciiChars);
    std::string asciiImage;

    for (int i = 0; i < image.rows; ++i) {
        asciiImage += std::string(pre_space, ' ');
        for (int j = 0; j < image.cols; ++j) {
            uchar pixel = image.at<uchar>(i, j);
            char asciiChar = asciiChars[(pixel * asciiLength) / 256];
            asciiImage += asciiChar;
        }
        if (pre_space)
            asciiImage += "\033[K";
        asciiImage += '\n';
    }

    return asciiImage;
}

std::string generate_ascii_image(const cv::Mat &image,
                                 int pre_space,
                                 const char *asciiChars,
                                 std::string (*ascii_func)(const cv::Mat &, int, const char *)) {
    // 调用通过函数指针选择的生成方式
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

void audio_callback(void *userdata, Uint8 *stream, int len) {
    AudioQueue *audio_queue = (AudioQueue *)userdata;
    SDL_memset(stream, 0, len);
    SDL_LockMutex(audio_queue->mutex);
    int copied = 0;
    while (copied < len && audio_queue->size > 0) {
        int to_copy = std::min(len - copied, audio_queue->size);
        SDL_MixAudioFormat(stream + copied, audio_queue->data, AUDIO_S16SYS, to_copy, SDL_MIX_MAXVOLUME);
        audio_queue->size -= to_copy;
        memmove(audio_queue->data, audio_queue->data + to_copy, audio_queue->size);
        copied += to_copy;
    }
    SDL_UnlockMutex(audio_queue->mutex);
}

const std::map<std::string, std::function<std::string(const cv::Mat &, int, const char *)>> param_func_pair = {
    {"dy", image_to_ascii_dy_contrast},
    {"st", image_to_ascii}};
const std::map<std::string, std::string> char_set_pairs = {
    {"s", ASCII_SEQ_SHORT},
    {"S", ASCII_SEQ_SHORT},
    {"l", ASCII_SEQ_LONG},
    {"L", ASCII_SEQ_LONG}};

void play_video(const std::map<std::string, std::string> &params) {
    std::string video_path;
    const char *frame_chars;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func = nullptr;

    if (params_include(params, "-v")) {
        video_path = params.at("-v");
    } else {
        print_error("No video but wanna play? Really? \nAdd a -v param, or type \"help\" to get usage");
        return;
    }

    if (params_include(params, "-ct") && params_include(param_func_pair, params.at("-ct"))) {
        generate_ascii_func = param_func_pair.at(params.at("-ct"));
    } else {
        generate_ascii_func = image_to_ascii;
    }

    if (params_include(params, "-chars")) {
        frame_chars = params.at("-chars").c_str();
    } else if (params_include(params, "-c") && params_include(char_set_pairs, params.at("-c"))) {
        frame_chars = char_set_pairs.at(params.at("-c")).c_str();
    } else {
        frame_chars = ASCII_SEQ_SHORT;
    }

    // Initialize FFmpeg
    avformat_network_init();

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (avformat_open_input(&format_ctx, video_path.c_str(), NULL, NULL) < 0) {
        print_error("Error: Could not open video file", video_path);
        return;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        avformat_close_input(&format_ctx);
        print_error("Error: Could not find stream info", video_path);
        return;
    }

    AVCodecContext *video_codec_ctx = nullptr;
    AVCodecContext *audio_codec_ctx = nullptr;
    AVStream *video_stream = nullptr;
    AVStream *audio_stream = nullptr;
    int video_stream_index = -1;
    int audio_stream_index = -1;

    // Find video and audio streams
    for (int i = 0; i < format_ctx->nb_streams; ++i) {
        AVStream *stream = format_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0) {
            video_stream = stream;
            video_stream_index = i;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_index < 0) {
            audio_stream = stream;
            audio_stream_index = i;
        }
    }

    if (!video_stream) {
        avformat_close_input(&format_ctx);
        print_error("Error: Could not find video stream.");
        return;
    }

    // Initialize video codec
    const AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    if (!video_codec) {
        avformat_close_input(&format_ctx);
        print_error("Error: Could not find video codec.");
        return;
    }
    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if (avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar) < 0) {
        avformat_close_input(&format_ctx);
        print_error("Error: Could not copy video codec parameters.");
        return;
    }
    if (avcodec_open2(video_codec_ctx, video_codec, NULL) < 0) {
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&format_ctx);
        print_error("Error: Could not open video codec.");
        return;
    }

    // Initialize audio queue
    AudioQueue audio_queue;
    audio_queue.data = new uint8_t[AUDIO_QUEUE_SIZE];
    audio_queue.size = 0;
    audio_queue.mutex = SDL_CreateMutex();

    // Initialize SDL audio if audio stream exists
    SDL_AudioSpec wanted_spec, spec;
    SDL_AudioDeviceID audio_device_id = 0;
    SwrContext *swr_ctx = nullptr;
    if (audio_stream) {
        const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
        if (!audio_codec) {
            print_error("Error: Could not find audio codec.");
        } else {
            audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            if (avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar) < 0) {
                print_error("Error: Could not copy audio codec parameters.");
            } else if (avcodec_open2(audio_codec_ctx, audio_codec, NULL) < 0) {
                avcodec_free_context(&audio_codec_ctx);
                print_error("Error: Could not open audio codec.");
            } else {
                print_audio_stream_info(audio_stream, audio_codec_ctx);
                if (SDL_Init(SDL_INIT_AUDIO) < 0) {
                    print_error("SDL_Init Error: ", SDL_GetError());
                } else {
                    wanted_spec.freq = audio_codec_ctx->sample_rate;
                    wanted_spec.format = AUDIO_S16SYS;
                    wanted_spec.channels = audio_codec_ctx->ch_layout.nb_channels;
                    wanted_spec.silence = 0;
                    wanted_spec.samples = 1024;
                    wanted_spec.callback = audio_callback;
                    wanted_spec.userdata = &audio_queue;

                    // int device_index = 1; // select_audio_device();
                    list_audio_devices();
                    audio_device_id = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, 0);
                    const char *device_name = SDL_GetAudioDeviceName(audio_device_id, 0);
                    if (audio_device_id == 0) {
                        print_error("SDL_OpenAudioDevice Error: ", SDL_GetError());
                    } else {
                        std::cout << "Audio device opened successfully. Device ID: " << audio_device_id << std::endl;
                        std::cout << "Using audio device: " << device_name << std::endl;
                        std::cout << "Actual audio spec - freq: " << spec.freq
                                  << ", format: " << SDL_AUDIO_BITSIZE(spec.format) << " bit"
                                  << ", channels: " << (int)spec.channels << std::endl;
                        swr_ctx = swr_alloc();
                        if (!swr_ctx) {
                            print_error("Error: Could not allocate SwrContext.");
                        } else {
                            AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
                            if (swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16, spec.freq,
                                                    &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                                                    0, NULL) < 0) {
                                print_error("Error: Could not set SwrContext options.");
                                swr_free(&swr_ctx);
                            } else if (swr_init(swr_ctx) < 0) {
                                print_error("Error: Could not initialize SwrContext.");
                                swr_free(&swr_ctx);
                            } else {
                                std::cout << "Audio resampling context initialized successfully." << std::endl;
                            }
                        }
                        SDL_PauseAudioDevice(audio_device_id, 0);
                        std::cout << "Audio device unpaused." << std::endl;
                    }
                }
            }
        }
    } else {
        std::cout << "No audio stream found in the video." << std::endl;
    }

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    if (!packet || !frame) {
        print_error("Error: Could not allocate packet or frame.");
        // Clean up and return
        // ... (cleanup code)
        return;
    }
    int64_t total_duration = format_ctx->duration / AV_TIME_BASE;
    int64_t current_time = 0;

    double fps = av_q2d(video_stream->avg_frame_rate);
    int frame_delay = static_cast<int>(1000.0 / fps);
    int termWidth, termHeight, frameWidth, frameHeight, prevTermWidth = 0, prevTermHeight = 0, w_space_count = 0, h_line_count = 0;

    bool quit = false, term_size_changed = true;
    int volume = SDL_MIX_MAXVOLUME;
    int seek_offset = 5; // 快进/快退 5 秒

    while (!quit && av_read_frame(format_ctx, packet) >= 0) {
        auto start_time = std::chrono::high_resolution_clock::now();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    case SDLK_LEFT:
                        av_seek_frame(format_ctx, video_stream_index, packet->pts - seek_offset * video_stream->time_base.den, AVSEEK_FLAG_BACKWARD);
                        break;
                    case SDLK_RIGHT:
                        av_seek_frame(format_ctx, video_stream_index, packet->pts + seek_offset * video_stream->time_base.den, AVSEEK_FLAG_ANY);
                        break;
                    case SDLK_UP:
                        volume = std::min(volume + 10, SDL_MIX_MAXVOLUME);
                        break;
                    case SDLK_DOWN:
                        volume = std::max(volume - 10, 0);
                        break;
                }
            }
        }

        if (packet->stream_index == video_stream_index) {
            if (avcodec_send_packet(video_codec_ctx, packet) >= 0) {
                while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                    // Convert to grayscale image
                    cv::Mat grayFrame(frame->height, frame->width, CV_8UC1);
                    for (int y = 0; y < frame->height; ++y) {
                        for (int x = 0; x < frame->width; ++x) {
                            grayFrame.at<uchar>(y, x) = frame->data[0][y * frame->linesize[0] + x];
                        }
                    }
                    // Get terminal size and resize frame
                    get_terminal_size(termWidth, termHeight);
                    termHeight -= 2;
                    if (termWidth != prevTermWidth || termHeight != prevTermHeight) {
                        prevTermWidth = termWidth;
                        prevTermHeight = termHeight;
                        term_size_changed = true;

                    } else
                        term_size_changed = false;
                    frameWidth = termWidth;
                    frameHeight = (grayFrame.rows * frameWidth) / grayFrame.cols / 2;
                    w_space_count = 0;
                    h_line_count = (termHeight - frameHeight) / 2;
                    if (frameHeight > termHeight) {
                        frameHeight = termHeight;
                        frameWidth = (grayFrame.cols * frameHeight * 2) / grayFrame.rows;
                        w_space_count = (termWidth - frameWidth) / 2;
                        h_line_count = 0;
                    }
                    cv::resize(grayFrame, grayFrame, cv::Size(frameWidth, frameHeight));

                    // Convert image to ASCII and display
                    std::string asciiArt = generate_ascii_func(grayFrame,
                                                               w_space_count,
                                                               frame_chars);
                    current_time = av_rescale_q(packet->pts, video_stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;

                    // Create progress bar
                    std::string time_played = format_time(current_time);
                    std::string total_time = format_time(total_duration);
                    int progress_width = termWidth - (int)time_played.length() - (int)total_time.length() - 2; // 2 for /
                    double progress = static_cast<double>(current_time) / total_duration;
                    std::string progress_bar = create_progress_bar(progress, progress_width);

                    // Combine ASCII art with progress bar
                    std::string combined_output;
                    add_empty_lines_for(combined_output, h_line_count);
                    combined_output += asciiArt;
                    add_empty_lines_for(combined_output,
                                        termHeight - frameHeight - h_line_count);
                    combined_output += time_played + "\\" + progress_bar + "/" + total_time + "\n";

                    // clear_screen();
                    move_cursor_to_top_left(term_size_changed);
                    printf("%s", combined_output.c_str()); // Show the Frame

                    // Frame rate control
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    int remaining_delay = frame_delay - static_cast<int>(processing_time.count()) - 4;
                    if (remaining_delay > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(remaining_delay));
                    }
                }
            }
        } else if (packet->stream_index == audio_stream_index && audio_codec_ctx && swr_ctx) {
            if (avcodec_send_packet(audio_codec_ctx, packet) >= 0) {
                while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
                    int out_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, audio_codec_ctx->sample_rate) + frame->nb_samples,
                                                          spec.freq, audio_codec_ctx->sample_rate, AV_ROUND_UP);
                    uint8_t *out_buffer;
                    av_samples_alloc(&out_buffer, NULL, spec.channels, out_samples, AV_SAMPLE_FMT_S16, 0);
                    int samples_out = swr_convert(swr_ctx, &out_buffer, out_samples,
                                                  (const uint8_t **)frame->data, frame->nb_samples);
                    if (samples_out > 0) {
                        int buffer_size = av_samples_get_buffer_size(NULL, spec.channels, samples_out, AV_SAMPLE_FMT_S16, 1);
                        SDL_LockMutex(audio_queue.mutex);
                        if (audio_queue.size + buffer_size < AUDIO_QUEUE_SIZE) {
                            memcpy(audio_queue.data + audio_queue.size, out_buffer, buffer_size);
                            audio_queue.size += buffer_size;
                            // std::cout << "Added " << buffer_size << " bytes to audio queue. Total size: " << audio_queue.size;
                        } else {
                            // std::cout << "Audio queue full. Discarding " << buffer_size << " bytes.";
                        }
                        SDL_UnlockMutex(audio_queue.mutex);
                    } else {
                        // std::cout << "No samples output from swr_convert";
                    }
                    av_freep(&out_buffer);
                }
            }
        }

        av_packet_unref(packet);

        if (is_escape_key_pressed()) {
            break;
        }
    }

    // Clean up
    av_frame_free(&frame);
    av_packet_free(&packet);
    if (audio_device_id) {
        SDL_CloseAudioDevice(audio_device_id);
    }
    SDL_Quit();
    if (swr_ctx) {
        swr_free(&swr_ctx);
    }
    avcodec_free_context(&video_codec_ctx);
    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
    }
    avformat_close_input(&format_ctx);

    // Clean up audio queue
    SDL_DestroyMutex(audio_queue.mutex);
    delete[] audio_queue.data;

    if (!quit) {
        std::cout << "Playback completed! Press any key to continue...";
        getchar();
    } else {
        clear_screen();
        std::cout << "Playback interrupted!\n";
    }
}

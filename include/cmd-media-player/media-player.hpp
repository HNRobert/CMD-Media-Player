//
//  media-player.hpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#ifndef video_player_hpp
#define video_player_hpp

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <SDL2/SDL.h>
#include <ncurses.h>

// Cancel the OK macro
#ifdef OK
#undef OK
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

#include <opencv2/opencv.hpp>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "basic-functions.hpp"

extern const int AUDIO_QUEUE_SIZE;

enum class UserAction {
    None,
    Quit,
    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown,
    KeyEqual,
    KeyMinus
};

struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
    int volume;
};

extern const char *ASCII_SEQ_LONGEST;
extern const char *ASCII_SEQ_LONGER;
extern const char *ASCII_SEQ_LONG;
extern const char *ASCII_SEQ_SHORT;
extern const char *ASCII_SEQ_SHORTER;
extern const char *ASCII_SEQ_SHORTEST;

extern std::vector<std::string> ascii_char_sets;

extern int current_char_set_index; // Default to ASCII_SEQ_SHORT

// ANSI escape sequence to move the cursor to the top-left corner and clear the screen

std::string image_to_ascii_dy_contrast(const cv::Mat &image,
                                       int pre_space = 0,
                                       const char *asciiChars = ASCII_SEQ_SHORT);
std::string image_to_ascii(const cv::Mat &image, int pre_space = 0,
                           const char *asciiChars = ASCII_SEQ_SHORT);

std::string generate_ascii_image(const cv::Mat &image,
                                 int pre_space,
                                 const char *asciiChars,
                                 std::function<std::string(const cv::Mat &, int, const char *)> ascii_func);

std::string format_time(int64_t seconds);
std::string create_progress_bar(double progress, int width);
void render_playback_overlay(int termHeight, int termWidth, const std::string &progress_output, int volume);

void print_audio_stream_info(AVStream *audio_stream, AVCodecContext *audio_codec_ctx);
void handle_sigint(int sig);

class NCursesHandler {
  private:
    bool is_paused = false;
    bool has_quitted = false;
    int termWidth, termHeight;

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
                get_terminal_size(termWidth, termHeight);
                mvprintw(termHeight - 1, termWidth - 1, is_paused ? "▶" : "⏸");
                if (is_paused) {
                    is_paused = false;
                    // printw("▶");
                    return UserAction::None;
                } else {
                    is_paused = true;
                    // printw("⏸");
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
            case '=':
                return UserAction::KeyEqual;
            case '-':
                return UserAction::KeyMinus;
            default:
                return UserAction::None;
        }
    }
};

#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

class AudioPlayer {
  private:
    bool cleaned_up = false;
    SwrContext *swr_ctx;
    SDL_AudioSpec wanted_spec, spec;
    SDL_AudioDeviceID audio_device_id = 0;
    AudioQueue audio_queue{};

    static void audio_callback(void *userdata, Uint8 *stream, int len) {
        auto a_queue = static_cast<AudioQueue *>(userdata);
        SDL_memset(stream, 0, len);
        SDL_LockMutex(a_queue->mutex);
        int copied = 0;
        while (copied < len && a_queue->size > 0) {
            int to_copy = std::min(len - copied, a_queue->size);

            SDL_MixAudioFormat(stream + copied, a_queue->data, AUDIO_S16SYS, to_copy, a_queue->volume);

            a_queue->size -= to_copy;
            memmove(a_queue->data, a_queue->data + to_copy, a_queue->size);
            copied += to_copy;
        }
        SDL_UnlockMutex(a_queue->mutex);
    }

  public:
    bool initialized = false;
    AVFrame *frame;
    AVCodecContext *audio_codec_ctx;
    AVFormatContext *format_ctx;
    int audio_stream_index = -1;
    int volume = SDL_MIX_MAXVOLUME;

    bool *debug_mode;
    Uint32 *frame_start;
    int *frame_delay;
    int *termWidth, *termHeight;
    bool *term_size_changed;
    int frame_time;
    std::string *progress_output;
    AVStream *audio_stream = nullptr;

    AudioPlayer() : audio_codec_ctx(nullptr), swr_ctx(nullptr) {}

    ~AudioPlayer() {
        cleanup();
    }

    bool initialize() {
        // Find audio stream
        for (int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream = format_ctx->streams[i];
                audio_stream_index = i;
                break;
            }
        }
        if (!audio_stream)
            return false;

        // Initialize audio queue
        audio_queue.data = new uint8_t[AUDIO_QUEUE_SIZE];
        audio_queue.size = 0;
        audio_queue.mutex = SDL_CreateMutex();
        if (audio_stream) {
            audio_queue.volume = volume;
            const AVCodec *audio_codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
            if (!audio_codec && debug_mode) {
                if (debug_mode)
                    print_error("Error: Could not find audio codec.");
                return false;
            }
            audio_codec_ctx = avcodec_alloc_context3(audio_codec);
            if (avcodec_parameters_to_context(audio_codec_ctx, audio_stream->codecpar) < 0) {
                if (debug_mode)
                    print_error("Error: Could not copy audio codec parameters.");
                return false;
            }
            if (avcodec_open2(audio_codec_ctx, audio_codec, nullptr) < 0) {
                avcodec_free_context(&audio_codec_ctx);
                if (debug_mode)
                    print_error("Error: Could not open audio codec.");
                return false;
            } else if (SDL_Init(SDL_INIT_AUDIO) < 0) {
                if (debug_mode)
                    print_error("SDL_Init Error: ", SDL_GetError());
                return false;
            }

            if (debug_mode)
                print_audio_stream_info(audio_stream, audio_codec_ctx);

            wanted_spec.freq = audio_codec_ctx->sample_rate;
            wanted_spec.format = AUDIO_S16SYS;
            wanted_spec.channels = audio_codec_ctx->ch_layout.nb_channels;
            wanted_spec.silence = 0;
            wanted_spec.samples = 1024;
            wanted_spec.callback = AudioPlayer::audio_callback;
            wanted_spec.userdata = &audio_queue;

            audio_device_id = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);
            if (audio_device_id == 0 && debug_mode) {
                print_error("SDL_OpenAudioDevice Error: ", SDL_GetError());
                return false;
            }

            swr_ctx = swr_alloc();
            if (!swr_ctx && debug_mode) {
                print_error("Error: Could not allocate SwrContext.");
                return false;
            }

            AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
            if (swr_alloc_set_opts2(&swr_ctx, &out_ch_layout, AV_SAMPLE_FMT_S16, spec.freq,
                                    &audio_codec_ctx->ch_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
                                    0, nullptr) < 0) {
                print_error("Error: Could not set SwrContext options.");
                swr_free(&swr_ctx);
                return false;
            }

            if (swr_init(swr_ctx) < 0) {
                print_error("Error: Could not initialize SwrContext.");
                swr_free(&swr_ctx);
                return false;
            }

            SDL_PauseAudioDevice(audio_device_id, 0);
        } else {
            if (debug_mode)
                print_error("No audio stream found.");
            return false;
        }
        initialized = true;
        return true;
    }

    void adjust_volume(int change) {
        volume = std::clamp(volume + change, 0, SDL_MIX_MAXVOLUME);
        SDL_LockAudioDevice(audio_device_id);
        audio_queue.volume = volume;
        SDL_UnlockAudioDevice(audio_device_id);
    }

    void volume_up() {
        adjust_volume(SDL_MIX_MAXVOLUME / 10);
    }

    void volume_down() {
        adjust_volume(-SDL_MIX_MAXVOLUME / 10);
    }

    void handle_packet(AVPacket *packet, bool has_video) {
        if (avcodec_send_packet(audio_codec_ctx, packet) < 0) {
            return;
        }
        while (avcodec_receive_frame(audio_codec_ctx, frame) >= 0) {
            render_audio();

            if (!has_video) {
                render_audio_frame(format_ctx);
            }
        }
    }

    void render_audio() {
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
                // Apply volume control, deprecated
                // SDL_MixAudioFormat(audio_queue.data + audio_queue.size, out_buffer, AUDIO_S16SYS, buffer_size, volume);
                audio_queue.size += buffer_size;
            } else {
                // std::cout << "Audio queue full. Discarding " << buffer_size << " bytes.";
            }
            SDL_UnlockMutex(audio_queue.mutex);
        } else {
            // std::cout << "No samples output from swr_convert";
        }
        av_freep(&out_buffer);
    }

    void render_audio_frame(AVFormatContext *format_ctx) {
        // TODO: Render visual audio frame
        render_playback_overlay(*termHeight, *termWidth, *progress_output, volume);

        // Frame rate control
        frame_time = SDL_GetTicks() - *frame_start;
        if (frame_time < *frame_delay + 4) {
            SDL_Delay(*frame_delay - frame_time - 4);
        }
    }

    void cleanup() {
        if (cleaned_up) {
            return;
        }
        SDL_CloseAudio();
        if (swr_ctx) {
            swr_free(&swr_ctx);
            swr_ctx = nullptr;
        }
        if (audio_codec_ctx) {
            avcodec_free_context(&audio_codec_ctx);
            audio_codec_ctx = nullptr;
        }
        if (SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        if (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
        if (SDL_WasInit(SDL_INIT_TIMER) & SDL_INIT_TIMER) {
            SDL_QuitSubSystem(SDL_INIT_TIMER);
        }
        if (SDL_WasInit(SDL_INIT_EVENTS) & SDL_INIT_EVENTS) {
            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        }
        if (SDL_WasInit(SDL_INIT_EVERYTHING) & SDL_INIT_EVERYTHING) {
            SDL_Quit();
        }
        if (SDL_WasInit(SDL_INIT_AUDIO)) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        cleaned_up = true;
    }
};

class VideoPlayer {
  private:
    bool cleaned_up = false;

  public:
    VideoPlayer() : video_codec_ctx(nullptr), generate_ascii_func(image_to_ascii) {}

    ~VideoPlayer() {
        cleanup();
    }

    bool initialized = false;
    int volume_value = SDL_MIX_MAXVOLUME;
    int *volume = &volume_value;
    bool *debug_mode;
    Uint32 *frame_start;
    int frame_time, *frame_delay;
    bool *term_size_changed;
    int *termWidth, *termHeight;
    int frameWidth, frameHeight;
    int w_space_count, h_line_count;
    std::string *progress_output;

    AVFrame *frame;
    AVStream *video_stream = nullptr;
    AVCodecContext *video_codec_ctx;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func;

    // Add member variables
    AVFormatContext *format_ctx;
    int video_stream_index = -1;

    // Modify initialize() method to store format_ctx and video_stream_index
    bool initialize() {
        // Initialize the video decoder
        for (int i = 0; i < format_ctx->nb_streams; ++i) {
            AVStream *stream = format_ctx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_index < 0) {
                video_stream = stream;
                video_stream_index = i;
                break;
            }
        }

        if (!video_stream) {
            if (debug_mode) {
                print_error("Error: No video stream found.");
            }
            return false;
        }

        const AVCodec *video_codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
        if (!video_codec) {
            // Video codec not found
            if (debug_mode) {
                print_error("Error: Could not find video codec.");
            }
            return false;
        }

        video_codec_ctx = avcodec_alloc_context3(video_codec);
        if (avcodec_parameters_to_context(video_codec_ctx, video_stream->codecpar) < 0) {
            // Failed to copy video parameters
            if (debug_mode) {
                print_error("Error: Could not copy video codec parameters.");
            }
            return false;
        } else if (avcodec_open2(video_codec_ctx, video_codec, nullptr) < 0) {
            // Failed to open video codec
            if (debug_mode) {
                print_error("Error: Could not open video codec.");
            }
            return false;
        }
        initialized = true;
        return true;
    }

    void handle_packet(AVPacket *packet) {
        if (avcodec_send_packet(video_codec_ctx, packet) < 0) {
            return;
        }
        while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
            // Handle video frame
            render_frame(frame);
        }
    }

    void render_frame(AVFrame *frame) {
        // Convert to grayscale image
        cv::Mat grayFrame(frame->height, frame->width, CV_8UC1);
        for (int y = 0; y < frame->height; ++y) {
            for (int x = 0; x < frame->width; ++x) {
                grayFrame.at<uchar>(y, x) = frame->data[0][y * frame->linesize[0] + x];
            }
        }
        // Get terminal size and resize frame

        frameWidth = *termWidth;
        frameHeight = (grayFrame.rows * frameWidth) / grayFrame.cols / 2;
        w_space_count = 0;
        h_line_count = (*termHeight - 2 - frameHeight) / 2;
        if (frameHeight > *termHeight - 2) {
            frameHeight = *termHeight - 2;
            frameWidth = (grayFrame.cols * frameHeight * 2) / grayFrame.rows;
            w_space_count = (*termWidth - frameWidth) / 2;
            h_line_count = 0;
        }
        cv::resize(grayFrame, grayFrame, cv::Size(frameWidth, frameHeight));

        // Convert image to ASCII and display
        const char *frame_chars = ascii_char_sets[current_char_set_index].c_str();
        std::string asciiArt = generate_ascii_func(grayFrame,
                                                   w_space_count,
                                                   frame_chars);

        if (term_size_changed) {
            clear();
        }
        mvprintw(h_line_count, 0, "%s", asciiArt.c_str()); // Show the Frame

        render_playback_overlay(*termHeight, *termWidth, *progress_output, *volume);

        // Frame rate control
        frame_time = SDL_GetTicks() - *frame_start;
        if (frame_time < *frame_delay + 4) {
            SDL_Delay(*frame_delay - frame_time - 4);
        }
    }

    void cleanup() {
        if (cleaned_up) {
            return;
        }
        if (video_codec_ctx) {
            avcodec_free_context(&video_codec_ctx);
            video_codec_ctx = nullptr;
        }
        cleaned_up = true;
    }
};

void play_media(const std::map<std::string, std::string> &params);

#endif /* media_player_hpp */

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

extern const int VOLUME;
extern const int AUDIO_QUEUE_SIZE;
enum class UserAction {
    None,
    Quit,
    KeyLeft,
    KeyRight,
    KeyUp,
    KeyDown
};
struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
};

extern const char *ASCII_SEQ_LONGEST;
extern const char *ASCII_SEQ_LONGER;
extern const char *ASCII_SEQ_LONG;
extern const char *ASCII_SEQ_SHORT;
extern const char *ASCII_SEQ_SHORTER;
extern const char *ASCII_SEQ_SHORTEST;

extern std::vector<std::string> ascii_char_sets;

extern int current_char_set_index; // Default to ASCII_SEQ_SHORT

extern SDL_AudioSpec audio_spec;

// ANSI escape sequence to move the cursor to the top-left corner and clear the screen

void move_cursor_to_top_left(bool clear_all = false);
void add_empty_lines_for(std::string &combined_output, int count);

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

void list_audio_devices();
int select_audio_device();
void print_audio_stream_info(AVStream *audio_stream, AVCodecContext *audio_codec_ctx);

void handle_sigint(int sig);

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

class AudioPlayer {
  public:
    Uint32 frame_start;

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

    void handle_packet(AVPacket *packet, bool has_video) {
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

                if (!has_video) {
                    render_audio(frame->pts);
                }

                av_freep(&out_buffer);
            }
            av_frame_free(&frame);
        }
    }

    void render_audio(int64_t current_time) {
        get_terminal_size(termWidth, termHeight);

        std::string time_played = format_time(current_time);
        std::string total_time = format_time(total_duration);
        int progress_width = termWidth - (int)time_played.length() - (int)total_time.length() - 2; // 2 for \/
        double progress = static_cast<double>(current_time) / total_duration;
        std::string progress_bar = create_progress_bar(progress, progress_width);

        move_cursor_to_top_left();
        std::string progress_output = std::string(termHeight - 2, '\n') + time_played + "\\" + progress_bar + "/" + total_time + "\n";
        printw("%s", progress_output.c_str());
        printw("Press SPACE to pause/resume, ESC to quit\n");
        refresh();

        // Frame rate control
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < frame_delay + 4) {
            SDL_Delay(frame_delay - frame_time - 4);
        }
    }

    void cleanup() {
        if (audio_device_id) {
            SDL_PauseAudioDevice(audio_device_id, 1); // Pause the audio device before closing
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
        if (audio_queue.mutex) {
            SDL_DestroyMutex(audio_queue.mutex);
            audio_queue.mutex = nullptr;
        }
        delete[] audio_queue.data;
        audio_queue.data = nullptr;
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
    }

  private:
    void audio_callback(void *userdata, Uint8 *stream, int len) {
        auto audio_queue = static_cast<AudioQueue *>(userdata);
        SDL_memset(stream, 0, len);
        SDL_LockMutex(audio_queue->mutex);
        int copied = 0;
        while (copied < len && audio_queue->size > 0) {
            int to_copy = std::min(len - copied, audio_queue->size);

            // Apply VOLUME control
            SDL_MixAudioFormat(stream + copied, audio_queue->data, AUDIO_S16SYS, to_copy, VOLUME);

            audio_queue->size -= to_copy;
            memmove(audio_queue->data, audio_queue->data + to_copy, audio_queue->size);
            copied += to_copy;
        }
        SDL_UnlockMutex(audio_queue->mutex);
    }

    int termWidth, termHeight;
    int frame_time;
    int frame_delay;
    int64_t current_time, total_duration;
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
    int64_t total_duration;
    int64_t current_time;
    Uint32 frame_start;

    AVCodecContext *video_codec_ctx;
    NCursesHandler *ncurses_handler;
    std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func;

    // Add member variables
    AVFormatContext *format_ctx;
    int video_stream_index;

    // Modify initialize() method to store format_ctx and video_stream_index
    bool initialize(AVFormatContext *format_ctx, int &video_stream_index) {
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

        this->format_ctx = format_ctx;
        this->video_stream_index = video_stream_index;

        total_duration = format_ctx->duration / AV_TIME_BASE;
        // total_duration = av_rescale_q(video_stream->duration, video_stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
        current_time = 0;
        double fps = av_q2d(format_ctx->streams[video_stream_index]->avg_frame_rate);
        frame_delay = static_cast<int>(1000.0 / fps);

        return true;
    }

    void handle_packet(AVPacket *packet) {
        if (avcodec_send_packet(video_codec_ctx, packet) >= 0) {
            AVFrame *frame = av_frame_alloc();
            current_time = av_rescale_q(packet->pts, video_stream->time_base, AV_TIME_BASE_Q) / AV_TIME_BASE;
            while (avcodec_receive_frame(video_codec_ctx, frame) >= 0) {
                // Handle video frame
                render_frame(frame, current_time, total_duration);
            }
            av_frame_free(&frame);
        }
    }

    void render_frame(AVFrame *frame, int64_t &current_time, int64_t total_duration) {
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
        } else {
            term_size_changed = false;
        }

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
        const char *frame_chars = ascii_char_sets[current_char_set_index].c_str();
        std::string asciiArt = generate_ascii_func(grayFrame,
                                                   w_space_count,
                                                   frame_chars);

        // Create progress bar
        std::string time_played = format_time(current_time);
        std::string total_time = format_time(total_duration);
        int progress_width = termWidth - (int)time_played.length() - (int)total_time.length() - 2; // 2 for \/
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
        printw("%s", combined_output.c_str()); // Show the Frame
        printw("Press SPACE to pause/resume, ESC/Ctrl+C to quit\n");
        // printw("Frame time: %d ms, Frame delay: %d ms", frame_time, frame_delay);
        refresh();

        // Frame rate control
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < frame_delay + 4) {
            SDL_Delay(frame_delay - frame_time - 4);
        }
    }

    // Add seek method
    void seek(int seek_seconds, bool debug_mode) {
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

    void cleanup() {
        if (video_codec_ctx) {
            avcodec_free_context(&video_codec_ctx);
            video_codec_ctx = nullptr;
        }
    }

  private:
    AVStream *video_stream = nullptr;
    int frame_time;
    int frame_delay;
    int termWidth, termHeight;
    int prevTermWidth, prevTermHeight;
    bool term_size_changed;
    int frameWidth, frameHeight;
    int w_space_count, h_line_count;
};

void play_media(const std::map<std::string, std::string> &params);

#endif /* media_player_hpp */

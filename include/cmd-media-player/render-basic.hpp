#ifndef render_basic_hpp
#define render_basic_hpp

#include <SDL2/SDL.h>
#include <ncurses.h>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "basic-functions.hpp"
#include "media-player.hpp"

// Forward declarations
struct AudioQueue;
extern const char *ASCII_SEQ_SHORT;
extern int volume;

// Basic rendering functions
void move_cursor_to_top_left(bool clear_all = false);
void add_empty_lines_for(std::string &combined_output, int count);

// ASCII art generation
std::string image_to_ascii_dy_contrast(const cv::Mat &image,
                                    int pre_space = 0,
                                    const char *asciiChars = ASCII_SEQ_SHORT);

std::string image_to_ascii(const cv::Mat &image, 
                        int pre_space = 0,
                        const char *asciiChars = ASCII_SEQ_SHORT);

std::string generate_ascii_image(const cv::Mat &image,
                              int pre_space,
                              const char *asciiChars,
                              std::string (*ascii_func)(const cv::Mat &, int, const char *));

// Playback UI elements
std::string create_progress_bar(double progress, int width);
void render_playback_overlay(int termHeight, int termWidth, int volume, 
                          int64_t total_duration, std::string total_time, 
                          int64_t current_time);

// Audio device management
void list_audio_devices();
int select_audio_device();
void print_audio_stream_info(AVStream *audio_stream, AVCodecContext *audio_codec_ctx);
void audio_callback(void *userdata, Uint8 *stream, int len);

// Implementation details remain unchanged

// ANSI escape sequence to move the cursor to the top-left corner and clear the screen
void move_cursor_to_top_left(bool clear_all) {
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
            asciiImage += '\n'; // Return and clear the characters afterwords in this line
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
                                 std::string (*ascii_func)(const cv::Mat &, int, const char *)) {
    // Call the pointer to the function to switch between generating methods
    return ascii_func(image, pre_space, asciiChars);
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
    std::string progress_output = time_played + "\\" + progress_bar + "/" + total_time + "\n";

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

        // Apply volume control with timing check
        SDL_MixAudioFormat(stream + copied,
                           audio_queue->data,
                           AUDIO_S16SYS,
                           to_copy,
                           volume);

        audio_queue->size -= to_copy;
        memmove(audio_queue->data, audio_queue->data + to_copy, audio_queue->size);
        copied += to_copy;
    }

    SDL_UnlockMutex(audio_queue->mutex);
}

#endif /* render_basic_hpp */
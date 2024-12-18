//
//  media-player.hpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#ifndef video_player_hpp
#define video_player_hpp

#include <algorithm>
#include <chrono>
#include <csignal>
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

void play_media(const std::map<std::string, std::string> &params);

// class AudioPlayer {
//   public:
//     AudioPlayer();
//     ~AudioPlayer();

//     bool initialize(AVFormatContext *format_ctx, int &audio_stream_index);
//     void start();
//     void handle_packet(AVPacket *packet);
//     void cleanup();

//     // Add the audio callback as a member function
//     void audio_callback(void *userdata, Uint8 *stream, int len);

//   private:
//     AVCodecContext *audio_codec_ctx;
//     SDL_AudioSpec spec;
//     SDL_AudioDeviceID audio_device_id;
//     SwrContext *swr_ctx;
//     struct AudioQueue {
//         uint8_t *data;
//         int size;
//         SDL_mutex *mutex;
//     } audio_queue;

//     // Volume control
//     int volume;
//     // ...existing code...
// };

// class VideoPlayer {
//   public:
//     VideoPlayer();
//     ~VideoPlayer();

//     bool initialize(AVFormatContext *format_ctx, int &video_stream_index);
//     void handle_packet(AVPacket *packet);
//     void render_frame(AVFrame *frame, int64_t &current_time, int64_t total_duration);
//     void cleanup();
//     AVCodecContext *video_codec_ctx;

//   private:

//     int termWidth, termHeight;
//     int frameWidth, frameHeight;
//     int prevTermWidth, prevTermHeight;
//     int w_space_count, h_line_count;
//     bool term_size_changed;
//     // Frame rate control
//     double fps;
//     int frame_delay;

//     // ASCII conversion function and character set
//     std::function<std::string(const cv::Mat &, int, const char *)> generate_ascii_func;
//     const char *frame_chars;

//     // ...existing code...
// };

#endif /* media_player_hpp */

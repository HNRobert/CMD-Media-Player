//
//  video-player.hpp
//  CMD-Media-Player
//
//  Created by Robert He on 2024/9/2.
//

#ifndef video_player_hpp
#define video_player_hpp

#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>

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

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1 : 0)
void play_video(const std::map<std::string, std::string> &params);

#endif /* video_player_hpp */


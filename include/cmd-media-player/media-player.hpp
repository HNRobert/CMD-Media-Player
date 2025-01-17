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

#include "basic-functions.hpp"

struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
    int64_t current_pts;  // Add PTS tracking
    double time_base;     // Add time base for accurate timing
};

struct VideoContext {
    AVCodecContext *codec_ctx;
    AVStream *stream;
    int stream_index;
    double fps;
};

struct AudioContext {
    AVCodecContext *codec_ctx;
    AVStream *stream;
    int stream_index;
    SwrContext *swr_ctx;
    AudioQueue queue;
    SDL_AudioSpec spec;
};
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
// #define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1 : 0)
void play_media(const std::map<std::string, std::string> &params);

#endif /* video_player_hpp */

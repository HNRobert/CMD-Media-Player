//
//  player-core.hpp
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

#include "player-basic.hpp"

extern SDL_AudioDeviceID audio_device_id;

struct AudioQueue {
    uint8_t *data;
    int size;
    SDL_mutex *mutex;
    int64_t current_pts; // Add PTS tracking
    double time_base;    // Add time base for accurate timing
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
    KeyMinus,
    KeySpace
};

class NCursesHandler {
  private:
    bool has_quitted = false;
    int termWidth, termHeight;

  public:
    bool is_paused = false;
    void *userdata;

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
        get_terminal_size(termWidth, termHeight);
        if (!is_paused) {
            mvprintw(termHeight - 1, termWidth - 2, "||");
            refresh();
            is_paused = true;
            nodelay(stdscr, FALSE);
            SDL_PauseAudioDevice(audio_device_id, 1); // Pause audio playback
        }
        UserAction res = handleInput(true);
        switch (res) {
            case UserAction::Quit:
                is_paused = false;
                nodelay(stdscr, TRUE);
                SDL_PauseAudioDevice(audio_device_id, 0); // Resume audio before quitting
                return UserAction::Quit;
            case UserAction::KeySpace:
                is_paused = false;
                nodelay(stdscr, TRUE);
                SDL_PauseAudioDevice(audio_device_id, 0); // Resume audio playback
                return UserAction::KeySpace;
            default:
                return handle_space();
        }
    }

    UserAction handleInput(bool last_space = false) {
        switch (getch()) {
            case ERR:
                return UserAction::None;
            case 27: // ESC key
            case 3:  // Ctrl+C
                return UserAction::Quit;
            case ' ':
                return last_space ? UserAction::KeySpace : handle_space();
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
void play_media(const std::map<std::string, std::string> &params);

#endif /* video_player_hpp */

cmake_minimum_required(VERSION 3.29)
project(CMD_Media_Player)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/opencv/4.10.0_14/include/opencv4
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/ffmpeg/7.1/include
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/sdl2/2.30.8/include
)

link_directories(
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/opencv/4.10.0_14/lib
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/ffmpeg/7.1/lib
        /opt/homebrew/Cellar/cmd-media-player/1.0.0/sdl2/2.30.8/lib
)

add_executable(CMD_Media_Player
        CMD-Media-Player/basic-functions.cpp
        CMD-Media-Player/basic-functions.hpp
        CMD-Media-Player/video-player.cpp
        CMD-Media-Player/video-player.hpp
        CMD-Media-Player/main.cpp
)

target_link_libraries(CMD_Media_Player
        avcodec
        avformat
        avutil
        swresample
        opencv_core
        opencv_highgui
        opencv_imgproc
        opencv_videoio
        sdl2
)
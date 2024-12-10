cmake_minimum_required(VERSION 3.29)
project(CMD_Media_Player)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        /opt/homebrew/opt/opencv/include/opencv4
        /opt/homebrew/opt/ffmpeg/include
        /opt/homebrew/opt/sdl2/include
)

link_directories(
        /opt/homebrew/opt/opencv/lib
        /opt/homebrew/opt/ffmpeg/lib
        /opt/homebrew/opt/sdl2/lib
)

add_executable(CMD_Media_Player
        CMD-Media-Player/basic-functions.cpp
        CMD-Media-Player/basic-functions.hpp
        CMD-Media-Player/main.cpp
        CMD-Media-Player/video-player.cpp
        CMD-Media-Player/video-player.hpp
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
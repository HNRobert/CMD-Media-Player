cmake_minimum_required(VERSION 3.29)
project(CMD-Media-Player)

set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    set(HOMEBREW_PREFIX "/opt/homebrew")
else()
    set(HOMEBREW_PREFIX "/usr/local")
endif()

include_directories(
    ${HOMEBREW_PREFIX}/opt/opencv/include/opencv4
    ${HOMEBREW_PREFIX}/opt/ffmpeg/include
    ${HOMEBREW_PREFIX}/opt/sdl2/include
)

link_directories(
    ${HOMEBREW_PREFIX}/opt/opencv/lib
    ${HOMEBREW_PREFIX}/opt/ffmpeg/lib
    ${HOMEBREW_PREFIX}/opt/sdl2/lib
)

add_executable(CMD-Media-Player
    CMD-Media-Player/basic-functions.cpp
    CMD-Media-Player/basic-functions.hpp
    CMD-Media-Player/main.cpp
    CMD-Media-Player/video-player.cpp
    CMD-Media-Player/video-player.hpp
)

target_link_libraries(CMD-Media-Player
    avcodec
    avformat
    avutil
    swresample
    opencv_core
    opencv_highgui
    opencv_imgproc
    opencv_videoio
    sdl2
    readline
)

# Install the executable
install(TARGETS CMD-Media-Player DESTINATION bin)

# Install header files
install(FILES
    CMD-Media-Player/basic-functions.hpp
    CMD-Media-Player/video-player.hpp
    DESTINATION include/CMD-Media-Player
)

# Add support for building both x86_64 and arm64 architectures
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
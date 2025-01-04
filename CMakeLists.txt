cmake_minimum_required(VERSION 3.29)
project(CMD-Media-Player)

set(CMAKE_CXX_STANDARD 20)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    set(HOMEBREW_PREFIX "/opt/homebrew")
else()
    set(HOMEBREW_PREFIX "/usr/local")
endif()

include_directories(
    ${HOMEBREW_PREFIX}/opt/opencv/include/opencv4
    ${HOMEBREW_PREFIX}/opt/ffmpeg/include
    ${HOMEBREW_PREFIX}/opt/sdl2/include
    ${HOMEBREW_PREFIX}/opt/ncurses/include
)

link_directories(
    ${HOMEBREW_PREFIX}/opt/opencv/lib
    ${HOMEBREW_PREFIX}/opt/ffmpeg/lib
    ${HOMEBREW_PREFIX}/opt/sdl2/lib
    ${HOMEBREW_PREFIX}/opt/ncurses/lib
)

add_executable(CMD-Media-Player
    src/basic-functions.cpp
    src/media-player.cpp
    src/main.cpp
)

target_include_directories(CMD-Media-Player PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

message(${PROJECT_SOURCE_DIR}/include) 
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
    ncurses
)

# Install the executable
install(TARGETS CMD-Media-Player DESTINATION bin)

# Install header files
install(FILES
    src/basic-functions.hpp
    src/media-player.hpp
    DESTINATION include/CMD-Media-Player
)

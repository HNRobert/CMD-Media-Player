cmake_minimum_required(VERSION 3.29)
project(CMD-Media-Player)

set(CMAKE_CXX_STANDARD 20)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Flags Debug: ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}")

# Determine Homebrew prefix based on platform and architecture
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(HOMEBREW_PREFIX "/opt/homebrew")
    else()
        set(HOMEBREW_PREFIX "/usr/local")
    endif()
else() # Linux
    set(HOMEBREW_PREFIX "/home/linuxbrew/.linuxbrew")
endif()

message(STATUS "Using Homebrew prefix: ${HOMEBREW_PREFIX}")

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
    src/player-basic.cpp
    src/player-core.cpp
    src/main.cpp
)

target_include_directories(CMD-Media-Player PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# target_link_libraries expects library names or paths
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
    include/cmd-media-player/player-basic.hpp
    include/cmd-media-player/player-core.hpp
    include/cmd-media-player/render-basic.hpp
    DESTINATION include/CMD-Media-Player
)

cmake_minimum_required(VERSION 3.10)
project(PipelinePlayer)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

add_executable(player
    src/main.cpp
    src/ffmpeg_demuxer.cpp
    src/ffmpeg_decoder.cpp
    src/sdl_renderer.cpp
)

target_link_libraries(player
    ${FFMPEG_LIBRARIES}
    ${SDL2_LIBRARIES}
)

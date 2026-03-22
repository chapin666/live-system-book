cmake_minimum_required(VERSION 3.10)
project(Project02_NetworkPlayer)

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
    src/network_player.cpp
    # ... 复用 project-01 的代码
)

target_link_libraries(player
    ${FFMPEG_LIBRARIES}
    ${SDL2_LIBRARIES}
    pthread
)

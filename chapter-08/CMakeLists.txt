cmake_minimum_required(VERSION 3.10)
project(rtmp-player VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libswscale
)
find_package(SDL2 REQUIRED)

# ========== 1. 简单RTMP播放器（最小可运行代码）==========
add_executable(simple_rtmp_player src/simple_rtmp_player.cpp)

target_include_directories(simple_rtmp_player PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(simple_rtmp_player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
    pthread
)

# ========== 2. 模块化RTMP播放器（工程化版本）==========
add_executable(rtmp_player
    src/main.cpp
    src/rtmp_connection.cpp
    src/flv_parser.cpp
)

target_include_directories(rtmp_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(rtmp_player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
    pthread
)

# 编译选项
target_compile_options(simple_rtmp_player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

target_compile_options(rtmp_player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS simple_rtmp_player rtmp_player DESTINATION bin)

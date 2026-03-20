cmake_minimum_required(VERSION 3.10)
project(live-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
    libavformat
    libavcodec
    libavutil
    libswscale
)
find_package(SDL2 REQUIRED)

# 头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

# ========== 1. 简单播放器（100行核心代码）==========
# 对应 README 第1节和第6节，适合初学者理解基础概念
add_executable(simple_player src/simple_player.cpp)

target_link_libraries(simple_player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

target_compile_options(simple_player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# ========== 2. Pipeline 播放器（工程化版本）==========
# 对应 README 第7节，展示模块化架构设计
set(PIPELINE_SOURCES
    src/main.cpp
    src/core/simple_pipeline.cpp
    src/core/demuxer.cpp
    src/core/decoder.cpp
    src/core/renderer.cpp
)

add_executable(live-player ${PIPELINE_SOURCES})

target_link_libraries(live-player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

target_compile_options(live-player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS simple_player live-player DESTINATION bin)

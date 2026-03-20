cmake_minimum_required(VERSION 3.10)
project(async-player VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
)
find_package(SDL2 REQUIRED)

# 可执行文件
add_executable(async-player
    src/main.cpp
    src/decoder_thread.cpp
    src/frame_queue.cpp
)

target_include_directories(async-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(async-player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
    pthread
)

target_compile_options(async-player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS async-player DESTINATION bin)

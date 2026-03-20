cmake_minimum_required(VERSION 3.10)
project(network-player VERSION 3.0.0 LANGUAGES CXX)

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
pkg_check_modules(CURL REQUIRED libcurl)
find_package(SDL2 REQUIRED)

# 可执行文件
add_executable(network-player
    src/main.cpp
    src/ring_buffer.cpp
    src/download_thread.cpp
)

target_include_directories(network-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(network-player
    ${FFMPEG_LIBRARIES}
    ${CURL_LIBRARIES}
    SDL2::SDL2
    pthread
)

target_compile_options(network-player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS network-player DESTINATION bin)

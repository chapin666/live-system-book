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

# 源文件
set(SOURCES
    src/main.cpp
    src/demuxer.cpp
    src/decoder.cpp
    src/renderer.cpp
)

# 可执行文件
add_executable(live-player ${SOURCES})

# 链接库
target_link_libraries(live-player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

# 编译选项
target_compile_options(live-player PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS live-player DESTINATION bin)

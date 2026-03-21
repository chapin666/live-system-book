cmake_minimum_required(VERSION 3.10)
project(hw-decode-player VERSION 7.0.0 LANGUAGES CXX)

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

# ========== 1. 硬件解码播放器 ==========
add_executable(hw_decode_player src/hw_decode_player.cpp)

target_include_directories(hw_decode_player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(hw_decode_player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
    pthread
)

# ========== 2. 解码性能对比工具 ==========
add_executable(decode_bench src/decode_bench.cpp)

target_include_directories(decode_bench PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(decode_bench
    ${FFMPEG_LIBRARIES}
    pthread
)

# 编译选项
set(COMMON_FLAGS ${FFMPEG_CFLAGS_OTHER} -Wall -Wextra)
target_compile_options(hw_decode_player PRIVATE ${COMMON_FLAGS})
target_compile_options(decode_bench PRIVATE ${COMMON_FLAGS})

# 安装
install(TARGETS hw_decode_player decode_bench DESTINATION bin)

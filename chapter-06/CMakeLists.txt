cmake_minimum_required(VERSION 3.10)
project(rtmp-streamer VERSION 6.0.0 LANGUAGES CXX)

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

# ========== 1. 简单RTMP推流器（最小可运行代码）==========
add_executable(simple_streamer src/simple_streamer.cpp)

target_include_directories(simple_streamer PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(simple_streamer
    ${FFMPEG_LIBRARIES}
    pthread
)

# ========== 2. 模块化推流器（工程化版本）==========
# add_executable(rtmp_streamer
#     src/main.cpp
#     src/encoder.cpp
#     src/flv_muxer.cpp
#     src/rtmp_sender.cpp
# )
# 
# target_include_directories(rtmp_streamer PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${FFMPEG_INCLUDE_DIRS}
# )
# 
# target_link_libraries(rtmp_streamer
#     ${FFMPEG_LIBRARIES}
#     pthread
# )

# 编译选项
target_compile_options(simple_streamer PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
    -Wall
    -Wextra
)

# 安装
install(TARGETS simple_streamer DESTINATION bin)

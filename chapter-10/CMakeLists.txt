cmake_minimum_required(VERSION 3.10)
project(capture-demo VERSION 5.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat
    libavcodec
    libavutil
    libavdevice
    libswscale
)
find_package(SDL2 REQUIRED)

# ========== 1. 视频采集演示 ==========
add_executable(video_capture_demo src/video_capture_demo.cpp)

target_include_directories(video_capture_demo PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(video_capture_demo
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

# ========== 2. 音频采集演示 ==========
add_executable(audio_capture_demo src/audio_capture_demo.cpp)

target_include_directories(audio_capture_demo PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(audio_capture_demo
    ${FFMPEG_LIBRARIES}
)

# ========== 3. 音视频同步采集 ==========
add_executable(av_capture_demo src/av_capture_demo.cpp)

target_include_directories(av_capture_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(av_capture_demo
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

# 编译选项
set(COMMON_FLAGS ${FFMPEG_CFLAGS_OTHER} -Wall -Wextra)
target_compile_options(video_capture_demo PRIVATE ${COMMON_FLAGS})
target_compile_options(audio_capture_demo PRIVATE ${COMMON_FLAGS})
target_compile_options(av_capture_demo PRIVATE ${COMMON_FLAGS})

# 安装
install(TARGETS video_capture_demo audio_capture_demo av_capture_demo DESTINATION bin)

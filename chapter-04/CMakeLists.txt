cmake_minimum_required(VERSION 3.10)
project(Ch4Performance)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)
pkg_check_modules(SDL2 REQUIRED sdl2)

include_directories(${FFMPEG_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

# 解码耗时测量工具
add_executable(timing_demo src/timing_demo.cpp)
target_link_libraries(timing_demo ${FFMPEG_LIBRARIES})

# 卡顿实验工具
add_executable(lag_demo src/artificial_lag_demo.cpp)
target_link_libraries(lag_demo ${FFMPEG_LIBRARIES} ${SDL2_LIBRARIES})

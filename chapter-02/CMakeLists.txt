cmake_minimum_required(VERSION 3.10)
project(SimplePlayer)

set(CMAKE_CXX_STANDARD 14)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)
pkg_check_modules(SDL2 REQUIRED sdl2)

# 可执行文件
add_executable(player src/simple_player.cpp)

# 包含路径
target_include_directories(player PRIVATE 
    ${FFMPEG_INCLUDE_DIRS} 
    ${SDL2_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(player 
    ${FFMPEG_LIBRARIES} 
    ${SDL2_LIBRARIES}
)

# 编译选项
target_compile_options(player PRIVATE 
    ${FFMPEG_CFLAGS_OTHER} 
    ${SDL2_CFLAGS_OTHER}
)

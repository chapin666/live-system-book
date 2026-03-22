cmake_minimum_required(VERSION 3.14)
project(chapter03-player VERSION 3.0.0 LANGUAGES CXX)

# ============================================
# 1. 基本设置
# ============================================
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra)
else()
    add_compile_options(-O3 -DNDEBUG -Wall)
endif()

# ============================================
# 2. 查找依赖
# ============================================
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat>=58.0
    libavcodec>=58.0
    libavutil>=56.0
)
find_package(SDL2 REQUIRED)

# ============================================
# 3. 库目标：player_core
# ============================================
add_library(player_core STATIC
    src/ffmpeg_demuxer.cpp
    src/ffmpeg_decoder.cpp
    src/sdl_renderer.cpp
    src/player_pipeline.cpp
)

target_include_directories(player_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(player_core PUBLIC
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)

target_compile_options(player_core PRIVATE
    ${FFMPEG_CFLAGS_OTHER}
)

# ============================================
# 4. 可执行文件：player
# ============================================
add_executable(player src/main.cpp)

target_link_libraries(player PRIVATE
    player_core
)

# ============================================
# 5. 安装规则
# ============================================
install(TARGETS player
    RUNTIME DESTINATION bin
)

install(TARGETS player_core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)

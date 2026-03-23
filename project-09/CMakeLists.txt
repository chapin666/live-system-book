cmake_minimum_required(VERSION 3.10)
project(mcu_recorder VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(FFMPEG REQUIRED 
    libavcodec
    libavformat
    libavutil
    libavfilter
    libswscale
    libswresample
)

set(SOURCES
    src/main.cpp
    src/recorder.cpp
    src/mcu_mixer.cpp
    src/hls_segmenter.cpp
    src/playlist_manager.cpp
    src/timeshift_buffer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${FFMPEG_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

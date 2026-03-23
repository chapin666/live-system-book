cmake_minimum_required(VERSION 3.10)
project(sfu_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

pkg_check_modules(FFMPEG REQUIRED 
    libavcodec>=58.0
    libavformat>=58.0
    libavutil>=56.0
)

set(SOURCES
    src/main.cpp
    src/sfu_server.cpp
    src/room_manager.cpp
    src/participant.cpp
    src/rtp_router.cpp
    src/video_forwarder.cpp
    src/audio_mixer.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    OpenSSL::SSL
    ${FFMPEG_LIBRARIES}
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

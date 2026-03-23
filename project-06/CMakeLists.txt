cmake_minimum_required(VERSION 3.10)
project(p2p_call VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(FFMPEG REQUIRED 
    libavcodec>=58.0
    libavformat>=58.0
    libavutil>=56.0
    libswscale>=5.0
)

pkg_check_modules(OPUS REQUIRED opus)

# 可选：PortAudio
find_package(PortAudio QUIET)
if(NOT PortAudio_FOUND)
    message(WARNING "PortAudio not found, audio capture disabled")
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/p2p_call.cpp
    src/rtp_packet.cpp
    src/stun_client.cpp
    src/udp_socket.cpp
    src/audio_capturer.cpp
    src/video_capturer.cpp
    src/codec/h264_encoder.cpp
    src/codec/opus_encoder.cpp
)

set(HEADERS
    include/common.h
    src/p2p_call.h
    src/rtp_packet.h
    src/stun_client.h
    src/udp_socket.h
    src/audio_capturer.h
    src/video_capturer.h
    src/codec/h264_encoder.h
    src/codec/opus_encoder.h
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${OPUS_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    ${FFMPEG_LIBRARIES}
    ${OPUS_LIBRARIES}
)

if(PortAudio_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${PortAudio_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PortAudio_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_PORTAUDIO)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
    ${FFMPEG_CFLAGS_OTHER}
    ${OPUS_CFLAGS_OTHER}
)

# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/${PROJECT_NAME}
)

# 测试
enable_testing()
add_test(NAME rtp_packet_test COMMAND ${PROJECT_NAME} --test rtp)

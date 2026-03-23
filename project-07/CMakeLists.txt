cmake_minimum_required(VERSION 3.10)
project(webrtc_client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# WebRTC (需要预先编译或下载)
set(WEBRTC_ROOT $ENV{WEBRTC_ROOT} CACHE PATH "WebRTC root directory")
if(NOT WEBRTC_ROOT)
    message(FATAL_ERROR "WEBRTC_ROOT not set")
endif()

pkg_check_modules(JSONCPP jsoncpp)

set(SOURCES
    src/main.cpp
    src/webrtc_client.cpp
    src/signaling_client.cpp
    src/peer_connection.cpp
    src/sdp_parser.cpp
    src/ice_candidate.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${WEBRTC_ROOT}/include
    ${WEBRTC_ROOT}/include/third_party/abseil-cpp
    ${WEBRTC_ROOT}/include/third_party/libyuv
    ${JSONCPP_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${WEBRTC_ROOT}/lib/libwebrtc.a
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${JSONCPP_LIBRARIES}
    X11
    dl
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

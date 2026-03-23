cmake_minimum_required(VERSION 3.10)
project(unified_server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system filesystem)

pkg_check_modules(FFMPEG REQUIRED 
    libavcodec
    libavformat
    libavutil
)

set(SOURCES
    src/main.cpp
    src/unified_server.cpp
    src/signaling/signaling_service.cpp
    src/signaling/room_manager.cpp
    src/signaling/websocket_handler.cpp
    src/sfu/sfu_module.cpp
    src/sfu/rtp_router.cpp
    src/sfu/bandwidth_controller.cpp
    src/mcu/mcu_module.cpp
    src/mcu/mixer.cpp
    src/mcu/hls_output.cpp
    src/common/config.cpp
    src/common/logger.cpp
    src/common/metrics.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${FFMPEG_LIBRARIES}
    ${Boost_LIBRARIES}
    yaml-cpp
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

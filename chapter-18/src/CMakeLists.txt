cmake_minimum_required(VERSION 3.10)
project(chapter17_udp_rtp)

set(CMAKE_CXX_STANDARD 14)

# 源文件
set(SOURCES
    rtp_rtcp_demo.cpp
)

# 头文件
include_directories(src)

# 可执行文件
add_executable(rtp_rtcp_demo ${SOURCES})

# 链接线程库
target_link_libraries(rtp_rtcp_demo pthread)

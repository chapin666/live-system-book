# 第五章：C++11 多线程基础

cmake_minimum_required(VERSION 3.16)
project(chapter05_threading)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 示例程序
add_executable(01_thread_basics src/01_thread_basics.cpp)
add_executable(02_mutex_demo src/02_mutex_demo.cpp)
add_executable(03_deadlock_demo src/03_deadlock_demo.cpp)
add_executable(04_condition_variable src/04_condition_variable.cpp)
add_executable(05_atomic_demo src/05_atomic_demo.cpp)
add_executable(06_threadsafe_queue src/06_threadsafe_queue.cpp)
add_executable(07_producer_consumer src/07_producer_consumer.cpp)

# 线程库
find_package(Threads REQUIRED)
target_link_libraries(01_thread_basics Threads::Threads)
target_link_libraries(02_mutex_demo Threads::Threads)
target_link_libraries(03_deadlock_demo Threads::Threads)
target_link_libraries(04_condition_variable Threads::Threads)
target_link_libraries(05_atomic_demo Threads::Threads)
target_link_libraries(06_threadsafe_queue Threads::Threads)
target_link_libraries(07_producer_consumer Threads::Threads)

# 添加头文件路径
include_directories(include)

# 打印编译信息
message(STATUS "Chapter 5: C++11 Threading")
message(STATUS "Build examples:")
message(STATUS "  01_thread_basics")
message(STATUS "  02_mutex_demo")
message(STATUS "  03_deadlock_demo")
message(STATUS "  04_condition_variable")
message(STATUS "  05_atomic_demo")
message(STATUS "  06_threadsafe_queue")
message(STATUS "  07_producer_consumer")

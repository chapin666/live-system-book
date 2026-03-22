/**
 * @file 01_thread_basics.cpp
 * @brief 线程基础示例
 * 
 * 演示内容：
 * - 创建线程
 * - join vs detach
 * - 线程 ID
 * - Lambda 在线程中的使用
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>

// 获取当前线程的字符串ID
std::string get_thread_id() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

// 简单函数在线程中运行
void hello() {
    std::cout << "[T" << get_thread_id() << "] Hello from thread!\n";
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "[T" << get_thread_id() << "] Thread finishing\n";
}

// 带参数的线程函数
void worker(int id, int sleep_ms) {
    std::cout << "[T" << get_thread_id() << "] Worker " << id 
              << " started, will sleep " << sleep_ms << "ms\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    std::cout << "[T" << get_thread_id() << "] Worker " << id << " finished\n";
}

int main() {
    std::cout << "=== 线程基础示例 ===\n\n";
    
    // 示例 1: 基本线程创建
    std::cout << "--- 示例 1: 基本线程创建 ---\n";
    {
        std::thread t(hello);
        std::cout << "[Main] 主线程等待子线程...\n";
        t.join();  // 等待线程完成
        std::cout << "[Main] 子线程已完成\n\n";
    }
    
    // 示例 2: 使用 Lambda
    std::cout << "--- 示例 2: 使用 Lambda ---\n";
    {
        int value = 42;
        std::thread t([value]() {
            std::cout << "[T" << get_thread_id() 
                      << "] Lambda captured value: " << value << "\n";
        });
        t.join();
        std::cout << "\n";
    }
    
    // 示例 3: 多个线程
    std::cout << "--- 示例 3: 多个线程并行执行 ---\n";
    {
        std::thread t1(worker, 1, 100);
        std::thread t2(worker, 2, 200);
        std::thread t3(worker, 3, 150);
        
        std::cout << "[Main] 等待所有 worker 完成...\n";
        t1.join();
        t2.join();
        t3.join();
        std::cout << "[Main] 所有 worker 已完成\n\n";
    }
    
    // 示例 4: detach（分离线程）
    std::cout << "--- 示例 4: detach ---\n";
    {
        std::thread t([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::cout << "[T" << get_thread_id() 
                      << "] 分离线程完成（可能在 main 结束后）\n";
        });
        t.detach();  // 分离线程，主线程不等待
        std::cout << "[Main] 线程已分离，主线程继续...\n";
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n=== 所有示例完成 ===\n";
    return 0;
}

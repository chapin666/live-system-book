/**
 * @file 02_mutex_demo.cpp
 * @brief 互斥锁示例
 * 
 * 演示内容：
 * - std::mutex 基本使用
 * - std::lock_guard (RAII)
 * - 数据竞争问题
 * - 原子计数器对比
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <chrono>
#include <atomic>

int unsafe_counter = 0;
int safe_counter = 0;
std::mutex mtx;
std::atomic<int> atomic_counter{0};

const int ITERATIONS = 100000;
const int NUM_THREADS = 4;

// 无锁递增（数据竞争）
void unsafe_increment() {
    for (int i = 0; i < ITERATIONS / NUM_THREADS; ++i) {
        ++unsafe_counter;  // 数据竞争！
    }
}

// 互斥锁保护
void safe_increment() {
    for (int i = 0; i < ITERATIONS / NUM_THREADS; ++i) {
        std::lock_guard<std::mutex> lock(mtx);
        ++safe_counter;
    }
}

// 原子操作
void atomic_increment() {
    for (int i = 0; i < ITERATIONS / NUM_THREADS; ++i) {
        ++atomic_counter;
    }
}

int main() {
    std::cout << "=== 互斥锁示例 ===\n\n";
    
    // 测试 1: 无锁（数据竞争）
    std::cout << "--- 测试 1: 无锁递增（数据竞争）---\n";
    {
        unsafe_counter = 0;
        std::vector<std::thread> threads;
        
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(unsafe_increment);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::steady_clock::now();
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "结果: " << unsafe_counter 
                  << " (预期: " << ITERATIONS << ")\n";
        std::cout << "耗时: " << ms << "ms\n";
        std::cout << (unsafe_counter == ITERATIONS ? "✓ 正确" : "✗ 错误（数据竞争）") << "\n\n";
    }
    
    // 测试 2: 互斥锁
    std::cout << "--- 测试 2: 互斥锁保护 ---\n";
    {
        safe_counter = 0;
        std::vector<std::thread> threads;
        
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(safe_increment);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::steady_clock::now();
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "结果: " << safe_counter << "\n";
        std::cout << "耗时: " << ms << "ms\n";
        std::cout << "✓ 正确（但较慢）\n\n";
    }
    
    // 测试 3: 原子操作
    std::cout << "--- 测试 3: 原子操作 ---\n";
    {
        atomic_counter = 0;
        std::vector<std::thread> threads;
        
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < NUM_THREADS; ++i) {
            threads.emplace_back(atomic_increment);
        }
        for (auto& t : threads) {
            t.join();
        }
        auto end = std::chrono::steady_clock::now();
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "结果: " << atomic_counter.load() << "\n";
        std::cout << "耗时: " << ms << "ms\n";
        std::cout << "✓ 正确且较快\n\n";
    }
    
    std::cout << "=== 结论 ===\n";
    std::cout << "- 无锁：快但错误（数据竞争）\n";
    std::cout << "- 互斥锁：正确但较慢\n";
    std::cout << "- 原子操作：正确且快（计数器场景）\n";
    
    return 0;
}

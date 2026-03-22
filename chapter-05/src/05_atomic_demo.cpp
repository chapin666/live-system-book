/**
 * @file 05_atomic_demo.cpp
 * @brief 原子操作示例
 * 
 * 演示内容：
 * - atomic 基本操作
 * - compare_exchange
 * - 内存序简介
 */

#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

std::atomic<int> counter{0};
std::atomic<bool> ready{false};

void worker(int id) {
    // 等待主线程信号
    while (!ready.load()) {
        std::this_thread::yield();
    }
    
    // 原子递增
    for (int i = 0; i < 1000; ++i) {
        counter.fetch_add(1);
    }
    
    std::cout << "Worker " << id << " 完成\n";
}

int main() {
    std::cout << "=== 原子操作示例 ===\n\n";
    
    // 示例 1: 原子计数器
    std::cout << "--- 原子计数器 ---\n";
    {
        counter = 0;
        ready = false;
        
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(worker, i);
        }
        
        // 同时启动所有线程
        ready.store(true);
        
        for (auto& t : threads) {
            t.join();
        }
        
        std::cout << "最终计数: " << counter.load() << " (预期: 4000)\n\n";
    }
    
    // 示例 2: CAS 操作
    std::cout << "--- CAS (Compare-And-Swap) ---\n";
    {
        std::atomic<int> value{10};
        
        int expected = 10;
        bool success = value.compare_exchange_strong(expected, 20);
        
        std::cout << "CAS 结果: " << (success ? "成功" : "失败") << "\n";
        std::cout << "当前值: " << value.load() << " (预期: 20)\n\n";
    }
    
    // 示例 3: CAS 失败场景
    std::cout << "--- CAS 失败场景 ---\n";
    {
        std::atomic<int> value{30};
        
        int expected = 10;  // 错误预期
        bool success = value.compare_exchange_strong(expected, 20);
        
        std::cout << "CAS 结果: " << (success ? "成功" : "失败") << "\n";
        std::cout << "expected 更新为: " << expected << " (实际值)\n";
        std::cout << "value 仍为: " << value.load() << "\n";
    }
    
    std::cout << "\n=== 完成 ===\n";
    return 0;
}

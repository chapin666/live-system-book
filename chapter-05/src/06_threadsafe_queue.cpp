/**
 * @file 06_threadsafe_queue.cpp
 * @brief 线程安全队列测试
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "live/threadsafe_queue.h"

using namespace live;

int main() {
    std::cout << "=== 线程安全队列测试 ===\n\n";
    
    // 测试 1: 基本操作
    std::cout << "--- 测试 1: 基本操作 ---\n";
    {
        ThreadSafeQueue<int> queue;
        
        queue.push(1);
        queue.push(2);
        queue.push(3);
        
        std::cout << "队列大小: " << queue.size() << "\n";
        
        int value;
        while (queue.try_pop(value)) {
            std::cout << "弹出: " << value << "\n";
        }
        std::cout << "\n";
    }
    
    // 测试 2: 单生产者单消费者
    std::cout << "--- 测试 2: 单生产者单消费者 ---\n";
    {
        ThreadSafeQueue<int> queue;
        const int COUNT = 100;
        
        std::thread producer([&queue, COUNT]() {
            for (int i = 0; i < COUNT; ++i) {
                queue.push(i);
            }
            queue.stop();
        });
        
        std::thread consumer([&queue, COUNT]() {
            int value;
            int sum = 0;
            while (queue.pop(value)) {
                sum += value;
            }
            std::cout << "消费者总和: " << sum << "\n";
        });
        
        producer.join();
        consumer.join();
        std::cout << "\n";
    }
    
    std::cout << "=== 测试完成 ===\n";
    return 0;
}

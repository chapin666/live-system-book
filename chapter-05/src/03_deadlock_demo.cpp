/**
 * @file 03_deadlock_demo.cpp
 * @brief 死锁演示与避免
 * 
 * 演示内容：
 * - 什么是死锁
 * - 双向死锁示例
 * - 使用 std::lock 避免死锁
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

std::mutex m1, m2;

// 错误示范：必然死锁
void deadlock_thread_a() {
    std::cout << "[A] 尝试获取 m1...\n";
    std::lock_guard<std::mutex> l1(m1);
    std::cout << "[A] 获取 m1 成功，休眠...\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "[A] 尝试获取 m2...\n";
    std::lock_guard<std::mutex> l2(m2);  // 等待 m2
    std::cout << "[A] 获取 m2 成功\n";
}

void deadlock_thread_b() {
    std::cout << "[B] 尝试获取 m2...\n";
    std::lock_guard<std::mutex> l2(m2);
    std::cout << "[B] 获取 m2 成功，休眠...\n";
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "[B] 尝试获取 m1...\n";
    std::lock_guard<std::mutex> l1(m1);  // 等待 m1
    std::cout << "[B] 获取 m1 成功\n";
}

// 正确示范：固定顺序
void safe_thread_a() {
    std::cout << "[A] 按顺序获取 m1, m2...\n";
    std::lock_guard<std::mutex> l1(m1);  // 总是先锁 m1
    std::lock_guard<std::mutex> l2(m2);  // 再锁 m2
    std::cout << "[A] 获取锁成功\n";
}

void safe_thread_b() {
    std::cout << "[B] 按顺序获取 m1, m2...\n";
    std::lock_guard<std::mutex> l1(m1);  // 总是先锁 m1
    std::lock_guard<std::mutex> l2(m2);  // 再锁 m2
    std::cout << "[B] 获取锁成功\n";
}

// 使用 std::lock 同时锁定
void stdlock_thread() {
    std::cout << "[Thread] 使用 std::lock 同时锁定...\n";
    std::lock(m1, m2);  // 同时锁定，避免死锁
    
    std::lock_guard<std::mutex> l1(m1, std::adopt_lock);
    std::lock_guard<std::mutex> l2(m2, std::adopt_lock);
    
    std::cout << "[Thread] 获取锁成功\n";
}

int main() {
    std::cout << "=== 死锁演示 ===\n\n";
    
    // 注意：实际死锁会卡住程序，这里只做演示说明
    std::cout << "--- 死锁场景说明 ---\n";
    std::cout << "线程 A: lock(m1) -> sleep -> lock(m2)\n";
    std::cout << "线程 B: lock(m2) -> sleep -> lock(m1)\n";
    std::cout << "结果: A 等待 m2，B 等待 m1，死锁！\n\n";
    
    std::cout << "--- 解决方案 1: 固定加锁顺序 ---\n";
    {
        // 重新初始化 mutex
        new (&m1) std::mutex();
        new (&m2) std::mutex();
        
        std::thread t1(safe_thread_a);
        std::thread t2(safe_thread_b);
        
        t1.join();
        t2.join();
        std::cout << "✓ 成功完成\n\n";
    }
    
    std::cout << "--- 解决方案 2: 使用 std::lock ---\n";
    {
        new (&m1) std::mutex();
        new (&m2) std::mutex();
        
        std::thread t1(stdlock_thread);
        std::thread t2(stdlock_thread);
        
        t1.join();
        t2.join();
        std::cout << "✓ 成功完成\n\n";
    }
    
    std::cout << "=== 死锁避免黄金法则 ===\n";
    std::cout << "1. 固定加锁顺序\n";
    std::cout << "2. 使用 std::lock 或 std::scoped_lock\n";
    std::cout << "3. 避免在持有锁时调用未知代码\n";
    std::cout << "4. 使用 try_lock 实现超时机制\n";
    
    return 0;
}

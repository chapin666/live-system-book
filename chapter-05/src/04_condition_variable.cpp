/**
 * @file 04_condition_variable.cpp
 * @brief 条件变量示例
 * 
 * 演示内容：
 * - wait / notify_one
 * - wait_for 超时
 * - 虚假唤醒防护
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

std::mutex mtx;
std::condition_variable cv;
std::queue<int> queue;
bool finished = false;

void producer() {
    for (int i = 0; i < 5; ++i) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(i);
            std::cout << "[Producer] 生产: " << i << "\n";
        }
        cv.notify_one();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
    }
    cv.notify_all();
    std::cout << "[Producer] 完成\n";
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 使用谓词防止虚假唤醒
        cv.wait(lock, []() {
            return !queue.empty() || finished;
        });
        
        if (queue.empty() && finished) {
            std::cout << "[Consumer] 退出\n";
            break;
        }
        
        int value = queue.front();
        queue.pop();
        lock.unlock();
        
        std::cout << "[Consumer] 消费: " << value << "\n";
    }
}

int main() {
    std::cout << "=== 条件变量示例 ===\n\n";
    
    std::thread prod(producer);
    std::thread cons(consumer);
    
    prod.join();
    cons.join();
    
    std::cout << "\n=== 完成 ===\n";
    return 0;
}

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <chrono>

namespace live {

/**
 * @brief 线程安全队列
 * 
 * 特点：
 * - 支持多生产者/多消费者
 * - 阻塞式 pop（队列为空时等待）
 * - 非阻塞式 try_pop
 * - 优雅停止（支持中断等待）
 * 
 * @tparam T 元素类型
 */
template<typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() : stopped_(false) {}
    
    // 禁止拷贝
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    
    // 允许移动
    ThreadSafeQueue(ThreadSafeQueue&&) = default;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = default;
    
    /**
     * @brief 推入元素
     */
    void push(T value) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_not_empty_.notify_one();
    }
    
    /**
     * @brief 阻塞式弹出
     * @param value 输出参数
     * @return true 成功获取元素，false 队列已停止
     */
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        cv_not_empty_.wait(lock, [this]() {
            return !queue_.empty() || stopped_.load();
        });
        
        if (queue_.empty()) {
            return false;  // 已停止
        }
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief 非阻塞式弹出
     * @return true 成功获取元素，false 队列为空或已停止
     */
    bool try_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (queue_.empty() || stopped_.load()) {
            return false;
        }
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief 带超时的弹出
     * @param timeout 超时时间
     * @return true 成功获取元素，false 超时或已停止
     */
    template<typename Rep, typename Period>
    bool try_pop_for(T& value, 
                     const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        bool ready = cv_not_empty_.wait_for(lock, timeout, [this]() {
            return !queue_.empty() || stopped_.load();
        });
        
        if (!ready || queue_.empty()) {
            return false;
        }
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    /**
     * @brief 停止队列，唤醒所有等待的线程
     */
    void stop() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stopped_.store(true);
        }
        cv_not_empty_.notify_all();
    }
    
    /**
     * @brief 检查队列是否为空
     */
    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    /**
     * @brief 获取队列大小
     */
    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    /**
     * @brief 检查队列是否已停止
     */
    bool stopped() const {
        return stopped_.load();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_not_empty_;
    std::queue<T> queue_;
    std::atomic<bool> stopped_;
};

} // namespace live

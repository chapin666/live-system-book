#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

extern "C" {
#include <libavutil/frame.h>
}

namespace live {

// 队列状态
enum class QueueState {
    OK,           // 正常
    Full,         // 队列满
    Empty,        // 队列为空
    Stopped       // 已停止
};

// 线程安全的帧队列
class FrameQueue {
public:
    explicit FrameQueue(size_t max_size = 3);
    ~FrameQueue();

    // 禁止拷贝（mutex 不可拷贝）
    FrameQueue(const FrameQueue&) = delete;
    FrameQueue& operator=(const FrameQueue&) = delete;

    // 生产者接口
    QueueState Push(AVFrame* frame, bool block = false);
    
    // 消费者接口
    AVFrame* Pop(bool block = true);
    
    // 查询状态
    size_t Size() const;
    bool Empty() const;
    bool Full() const;
    
    // 控制
    void Clear();
    void Stop();

private:
    std::queue<AVFrame*> queue_;
    const size_t max_size_;
    
    mutable std::mutex mutex_;
    std::condition_variable not_full_;   // 队列不满
    std::condition_variable not_empty_;  // 队列非空
    
    std::atomic<bool> stopped_{false};
};

} // namespace live

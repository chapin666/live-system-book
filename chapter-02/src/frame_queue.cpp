#include "live/frame_queue.h"
#include <iostream>

namespace live {

FrameQueue::FrameQueue(size_t max_size) : max_size_(max_size) {
    std::cout << "[FrameQueue] Created with max_size=" << max_size << std::endl;
}

FrameQueue::~FrameQueue() {
    Clear();
}

QueueState FrameQueue::Push(AVFrame* frame, bool block) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 如果非阻塞且队列满，直接返回
    if (!block && queue_.size() >= max_size_) {
        return QueueState::Full;
    }
    
    // 等待队列不满（或停止）
    not_full_.wait(lock, [this]() {
        return queue_.size() < max_size_ || stopped_.load();
    });
    
    if (stopped_) {
        return QueueState::Stopped;
    }
    
    // 入队
    queue_.push(frame);
    size_t size = queue_.size();
    lock.unlock();
    
    // 通知等待的消费者
    not_empty_.notify_one();
    
    if (size >= max_size_) {
        return QueueState::Full;
    }
    return QueueState::OK;
}

AVFrame* FrameQueue::Pop(bool block) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 如果非阻塞且队列为空，直接返回
    if (!block && queue_.empty()) {
        return nullptr;
    }
    
    // 等待队列非空（或停止）
    not_empty_.wait(lock, [this]() {
        return !queue_.empty() || stopped_.load();
    });
    
    if (stopped_ && queue_.empty()) {
        return nullptr;
    }
    
    // 出队
    AVFrame* frame = queue_.front();
    queue_.pop();
    lock.unlock();
    
    // 通知等待的生产者
    not_full_.notify_one();
    
    return frame;
}

size_t FrameQueue::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool FrameQueue::Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

bool FrameQueue::Full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size() >= max_size_;
}

void FrameQueue::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        AVFrame* frame = queue_.front();
        queue_.pop();
        av_frame_free(&frame);
    }
}

void FrameQueue::Stop() {
    stopped_.store(true);
    not_full_.notify_all();
    not_empty_.notify_all();
}

} // namespace live

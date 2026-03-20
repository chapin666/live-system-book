#include "live/ring_buffer.h"
#include <cstring>
#include <algorithm>
#include <chrono>

namespace live {

RingBuffer::RingBuffer(size_t capacity)
    : capacity_(capacity) {
    buffer_ = new uint8_t[capacity];
}

RingBuffer::~RingBuffer() {
    delete[] buffer_;
}

size_t RingBuffer::Write(const void* data, size_t len) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (stopped_.load()) {
        return 0;
    }
    
    const uint8_t* src = static_cast<const uint8_t*>(data);
    size_t written = 0;
    
    while (len > 0 && size_ < capacity_) {
        // 计算本次可写入的长度
        size_t write_end = (write_pos_ >= read_pos_) ? capacity_ : read_pos_;
        size_t can_write = std::min(len, write_end - write_pos_);
        can_write = std::min(can_write, capacity_ - size_);
        
        if (can_write == 0) break;
        
        // 写入数据
        memcpy(buffer_ + write_pos_, src + written, can_write);
        write_pos_ = (write_pos_ + can_write) % capacity_;
        size_ += can_write;
        written += can_write;
        len -= can_write;
    }
    
    lock.unlock();
    if (written > 0) {
        not_empty_.notify_all();
    }
    
    return written;
}

size_t RingBuffer::Read(void* out, size_t len) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    uint8_t* dst = static_cast<uint8_t*>(out);
    size_t read = 0;
    
    while (len > 0 && size_ > 0) {
        // 计算本次可读取的长度
        size_t read_end = (read_pos_ >= write_pos_) ? capacity_ : write_pos_;
        size_t can_read = std::min(len, read_end - read_pos_);
        can_read = std::min(can_read, size_);
        
        if (can_read == 0) break;
        
        // 读取数据
        memcpy(dst + read, buffer_ + read_pos_, can_read);
        read_pos_ = (read_pos_ + can_read) % capacity_;
        size_ -= can_read;
        read += can_read;
        len -= can_read;
    }
    
    lock.unlock();
    if (read > 0) {
        not_full_.notify_all();
    }
    
    return read;
}

size_t RingBuffer::ReadBlocking(void* out, size_t len) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // 等待数据可用
    not_empty_.wait(lock, [this] { return size_ > 0 || stopped_.load(); });
    
    if (stopped_.load() && size_ == 0) {
        return 0;
    }
    
    lock.unlock();
    return Read(out, len);
}

size_t RingBuffer::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

bool RingBuffer::Empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ == 0;
}

bool RingBuffer::Full() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ >= capacity_;
}

void RingBuffer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    read_pos_ = 0;
    write_pos_ = 0;
    size_ = 0;
    not_full_.notify_all();
}

void RingBuffer::Stop() {
    stopped_.store(true);
    not_empty_.notify_all();
    not_full_.notify_all();
}

// FFmpeg 回调接口
int RingBuffer::ReadCallback(void* opaque, uint8_t* buf, int buf_size) {
    auto* ring = static_cast<RingBuffer*>(opaque);
    if (!ring) return -1;
    
    size_t read = ring->ReadBlocking(buf, buf_size);
    if (read == 0 && ring->IsStopped()) {
        return AVERROR_EOF;
    }
    return static_cast<int>(read);
}

} // namespace live

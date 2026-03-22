#pragma once
#include <cstdint.h>
#include <stddef.h>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace live {

// 环形缓冲区 - 用于网络播放器
// 平滑下载速度和播放速度的差异
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);
    ~RingBuffer();

    // 禁止拷贝
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // 写入数据（下载线程调用）
    // 返回实际写入的字节数
    size_t Write(const void* data, size_t len);
    
    // 读取数据（解码线程调用）
    // 返回实际读取的字节数
    size_t Read(void* out, size_t len);
    
    // 阻塞式读取（等待直到有足够数据或停止）
    size_t ReadBlocking(void* out, size_t len);
    
    // 查询状态
    size_t Size() const;
    size_t Capacity() const { return capacity_; }
    bool Empty() const;
    bool Full() const;
    
    // 控制
    void Clear();
    void Stop();  // 通知等待的线程退出
    bool IsStopped() const { return stopped_.load(); }

    // 用于 FFmpeg 的回调接口
    static int ReadCallback(void* opaque, uint8_t* buf, int buf_size);

private:
    uint8_t* buffer_;         // 底层缓冲区
    const size_t capacity_;   // 容量
    
    size_t read_pos_ = 0;     // 读位置
    size_t write_pos_ = 0;    // 写位置
    size_t size_ = 0;         // 当前数据量
    
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::atomic<bool> stopped_{false};
};

} // namespace live

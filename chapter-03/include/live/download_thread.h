#pragma once
#include "live/ring_buffer.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>

namespace live {

struct DownloadConfig {
    std::string url;
    int64_t start_pos = 0;      // 起始位置（用于 seek）
    int connect_timeout = 10;   // 连接超时（秒）
    int speed_limit = 0;        // 限速（KB/s，0 表示不限）
};

// 下载线程 - 负责 HTTP 下载
class DownloadThread {
public:
    explicit DownloadThread(RingBuffer* buffer, const DownloadConfig& config);
    ~DownloadThread();

    DownloadThread(const DownloadThread&) = delete;
    DownloadThread& operator=(const DownloadThread&) = delete;

    bool Start();
    void Stop();
    bool IsRunning() const { return running_.load(); }
    
    // 获取下载统计
    int64_t GetDownloadedBytes() const { return downloaded_bytes_.load(); }

private:
    void Run();
    
    RingBuffer* ring_buffer_;
    DownloadConfig config_;
    
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<int64_t> downloaded_bytes_{0};
};

} // namespace live

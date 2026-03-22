#pragma once
#include <string>
#include <chrono>

namespace live {

// 网络播放器配置
struct NetworkConfig {
    // 超时设置（微秒）
    int64_t connect_timeout_us = 10000000;   // 10秒
    int64_t read_timeout_us = 5000000;       // 5秒
    
    // 重连设置
    int max_retries = 3;
    int retry_delay_ms = 2000;
    
    // 缓冲设置
    int buffer_duration_ms = 2000;  // 缓冲2秒数据再播放
    
    // HTTP 设置
    bool tcp_nodelay = true;
    int http_persistent = 1;  // 保持连接
};

class NetworkPlayer {
public:
    explicit NetworkPlayer(const NetworkConfig& config = {});
    
    // 播放网络或本地视频
    bool Play(const char* url);
    void Stop();
    
    // 网络状态
    bool IsNetworkSource() const { return is_network_; }
    float GetBufferProgress() const;  // 0.0 - 100.0
    int64_t GetDownloadSpeed() const; // bytes/s
    
private:
    bool IsNetworkUrl(const char* url);
    bool TryOpen(const char* url);
    void HandleError(int error_code);
    bool RetryLoop(const char* url);
    
    NetworkConfig config_;
    bool is_network_ = false;
    int retry_count_ = 0;
    int64_t bytes_downloaded_ = 0;
    std::chrono::steady_clock::time_point download_start_;
};

} // namespace live

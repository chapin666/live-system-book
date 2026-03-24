#pragma once
#include "live/player.h"
#include <string>
#include <functional>

namespace live {

// 带网络支持的播放器
class NetworkPlayer : public Player {
public:
    NetworkPlayer();
    ~NetworkPlayer();
    
    // 带重试的播放
    bool PlayWithRetry(const char* url, int max_retries = 3);
    
    // 设置缓冲参数
    void SetBufferSize(int min_buffer_ms, int max_buffer_ms);
    
    // 获取缓冲进度 (0-100)
    int GetBufferProgress() const;
    
    // 网络状态
    bool IsNetworkUrl(const char* url) const;
    bool IsConnected() const;
    
    // 回调
    std::function<void(int progress)> on_buffer_progress;
    std::function<void(const char* error)> on_network_error;
    
private:
    bool TryPlay(const char* url);
    void HandleNetworkError(int error_code);
    void UpdateBufferProgress();
    
    int max_retries_ = 3;
    int min_buffer_ms_ = 2000;
    int max_buffer_ms_ = 10000;
    int buffer_progress_ = 0;
    bool is_connected_ = false;
};

} // namespace live
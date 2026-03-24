#pragma once
#include <memory>
#include <string>
#include <functional>

namespace live {

// 主播端配置
struct StreamerConfig {
    char server_url[512];
    char stream_key[256];
    int video_width = 1280;
    int video_height = 720;
    int video_fps = 30;
    int video_bitrate = 2000000;  // 2Mbps
    int audio_sample_rate = 48000;
    int audio_channels = 2;
};

// 主播端
class Streamer {
public:
    Streamer();
    ~Streamer();
    
    // 初始化
    bool Init(const StreamerConfig& config);
    void Shutdown();
    
    // 开始/停止推流
    bool StartStream();
    void StopStream();
    
    // 状态
    bool IsStreaming() const;
    int64_t GetBytesSent() const;
    float GetNetworkBitrate() const;  // kbps
    
    // 回调
    std::function<void(const char* status)> on_status;
    std::function<void(float bitrate, int fps)> on_stats;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
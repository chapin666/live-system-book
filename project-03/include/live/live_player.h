#pragma once
#include <cstdint>
#include <string>

namespace live {

// 直播播放器配置
struct LiveConfig {
    // 延迟控制
    int max_delay_ms = 5000;      // 最大容忍延迟 5秒
    int target_delay_ms = 2000;   // 目标延迟 2秒
    
    // 追帧策略
    bool enable_drop_frame = true;   // 允许丢帧
    bool enable_fast_play = true;    // 允许加速播放
    
    // 重连设置
    int reconnect_delay_ms = 5000;   // 断流后 5秒重连
    int max_reconnects = 10;          // 最大重连次数
};

class LivePlayer {
public:
    explicit LivePlayer(const LiveConfig& config = {});
    
    // 播放直播流
    bool Play(const char* url);
    void Stop();
    
    // 状态查询
    bool IsLive() const { return is_live_; }
    int GetCurrentDelayMs() const { return current_delay_ms_; }
    int GetDroppedFrames() const { return dropped_frames_; }
    float GetBitrateKbps() const;  // 当前码率
    
private:
    bool OpenStream(const char* url);
    void HandleFrameTiming();
    bool IsLiveStream();  // 检测是否为直播流
    void Run();
    
    LiveConfig config_;
    bool is_live_ = false;
    bool connected_ = false;
    int current_delay_ms_ = 0;
    int dropped_frames_ = 0;
    int64_t bytes_received_ = 0;
    std::string url_;
};

} // namespace live

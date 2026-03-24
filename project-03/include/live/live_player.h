#pragma once
#include "live/network_player.h"

namespace live {

// 直播播放器（低延迟模式）
class LivePlayer : public NetworkPlayer {
public:
    LivePlayer();
    
    // 直播模式设置
    void SetLiveMode(bool live);
    
    // 追帧策略
    void SetCatchUpStrategy(bool enabled);
    
    // 获取延迟统计
    float GetCurrentLatency() const;
    
private:
    void ApplyLiveOptions();
    void AdjustPlaybackSpeed();
    
    bool is_live_mode_ = false;
    bool catch_up_enabled_ = true;
    float current_latency_ = 0.0f;
    float playback_speed_ = 1.0f;
};

} // namespace live
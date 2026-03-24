#include "live/live_player.h"
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

LivePlayer::LivePlayer() = default;

void LivePlayer::SetLiveMode(bool live) {
    is_live_mode_ = live;
    if (live) {
        ApplyLiveOptions();
    }
}

void LivePlayer::ApplyLiveOptions() {
    // 直播模式优化选项
    // - 减少缓冲
    // - 启用追帧
    // - 优先低延迟
    std::cout << "启用直播模式（低延迟）" << std::endl;
    SetBufferSize(500, 3000);  // 500ms最小缓冲，3秒最大
}

void LivePlayer::SetCatchUpStrategy(bool enabled) {
    catch_up_enabled_ = enabled;
}

float LivePlayer::GetCurrentLatency() const {
    return current_latency_;
}

void LivePlayer::AdjustPlaybackSpeed() {
    if (!catch_up_enabled_) return;
    
    // 如果延迟过大，加速播放追赶
    if (current_latency_ > 500.0f) {
        playback_speed_ = 1.2f;  // 1.2倍速
    } else if (current_latency_ < 200.0f) {
        playback_speed_ = 1.0f;  // 正常速度
    }
}

} // namespace live
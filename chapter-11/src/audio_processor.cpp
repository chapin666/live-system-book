// src/audio_processor.cpp
#include "live/audio_processor.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace live {

class AudioProcessor::Impl {
public:
    Impl(const AudioConfig& cfg) : config_(cfg) {
        samples_per_frame_ = config_.sample_rate * config_.frame_duration_ms / 1000;
        total_samples_ = samples_per_frame_ * config_.channels;
    }
    
    bool Init() {
        std::cout << "[AudioProcessor] Initialized: " 
                  << config_.sample_rate << "Hz, " 
                  << config_.channels << " channels,"
                  << samples_per_frame_ << " samples/frame" << std::endl;
        return true;
    }
    
    void Process(const int16_t* mic, const int16_t* speaker, int16_t* out) {
        // 1. AEC：简单实现（实际使用 WebRTC AEC3）
        if (aec_enabled_ && speaker) {
            SimpleAEC(mic, speaker, out);
        } else {
            std::memcpy(out, mic, total_samples_ * sizeof(int16_t));
        }
        
        // 2. NS：简单降噪
        if (ns_enabled_) {
            SimpleNS(out, out);
        }
        
        // 3. AGC：简单增益控制
        if (agc_enabled_) {
            SimpleAGC(out, out);
        }
    }

    void EnableAEC(bool enable) { aec_enabled_ = enable; }
    void EnableNS(bool enable) { ns_enabled_ = enable; }
    void EnableAGC(bool enable) { agc_enabled_ = enable; }

private:
    // 简化 AEC：减去参考信号的一部分
    void SimpleAEC(const int16_t* mic, const int16_t* ref, int16_t* out) {
        for (int i = 0; i < total_samples_; i++) {
            // 减去参考信号的 20%（简化模型）
            int32_t val = static_cast<int32_t>(mic[i]) - (ref[i] >> 2);
            out[i] = static_cast<int16_t>(
                std::max(-32768, std::min(32767, val))
            );
        }
    }
    
    // 简化 NS：门限降噪
    void SimpleNS(const int16_t* in, int16_t* out) {
        const int16_t threshold = 500;  // 噪声门限
        
        for (int i = 0; i < total_samples_; i++) {
            if (std::abs(in[i]) < threshold) {
                out[i] = 0;  // 认为是噪声，置零
            } else {
                out[i] = in[i];
            }
        }
    }
    
    // 简化 AGC：自动调整增益
    void SimpleAGC(const int16_t* in, int16_t* out) {
        // 计算 RMS
        int64_t sum = 0;
        for (int i = 0; i < total_samples_; i++) {
            sum += static_cast<int64_t>(in[i]) * in[i];
        }
        int rms = static_cast<int>(std::sqrt(sum / total_samples_));
        
        // 目标 RMS：3000（约 -20 dBFS）
        const int target_rms = 3000;
        const int max_gain = 10;
        
        if (rms > 0 && rms < target_rms) {
            int gain = std::min(target_rms / rms, max_gain);
            
            for (int i = 0; i < total_samples_; i++) {
                int32_t val = static_cast<int32_t>(in[i]) * gain;
                out[i] = static_cast<int16_t>(
                    std::max(-32768, std::min(32767, val))
                );
            }
        } else {
            std::memcpy(out, in, total_samples_ * sizeof(int16_t));
        }
    }

    bool aec_enabled_ = true;
    bool ns_enabled_ = true;
    bool agc_enabled_ = true;
    AudioConfig config_;
    int samples_per_frame_ = 0;
    int total_samples_ = 0;
};

// 公共接口实现
AudioProcessor::AudioProcessor(const AudioConfig& config)
    : impl_(std::make_unique<Impl>(config))
    , config_(config) {
}

AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::Init() {
    return impl_->Init();
}

void AudioProcessor::Process(const int16_t* mic, const int16_t* speaker, int16_t* out) {
    impl_->Process(mic, speaker, out);
}

void AudioProcessor::EnableAEC(bool enable) {
    impl_->EnableAEC(enable);
}

void AudioProcessor::EnableNS(bool enable) {
    impl_->EnableNS(enable);
}

void AudioProcessor::EnableAGC(bool enable) {
    impl_->EnableAGC(enable);
}

} // namespace live

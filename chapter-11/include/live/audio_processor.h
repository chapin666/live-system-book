#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>

namespace live {

struct AudioConfig {
    int sample_rate = 48000;      // 采样率：16k, 32k, 48k
    int channels = 2;              // 声道数：1 或 2
    int frame_duration_ms = 10;    // 帧长：10ms
};

class AudioProcessor {
public:
    explicit AudioProcessor(const AudioConfig& config);
    ~AudioProcessor();

    // 初始化 APM
    bool Init();
    
    // 处理音频帧
    // mic: 麦克风输入（interleaved PCM）
    // speaker: 扬声器参考信号（用于 AEC，可为 nullptr）
    // out: 处理后输出（与 mic 相同大小）
    void Process(const int16_t* mic, const int16_t* speaker, int16_t* out);
    
    // 设置处理开关
    void EnableAEC(bool enable);
    void EnableNS(bool enable);
    void EnableAGC(bool enable);
    
    // 设置 AGC 目标电平
    void SetAGCTargetLevel(int target_level_dbfs);  // -31 ~ 0
    
    // 设置 NS 等级
    void SetNSLevel(int level);  // 0-3

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    AudioConfig config_;
};

} // namespace live

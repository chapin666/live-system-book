#pragma once

#include "live/idemuxer.h"
#include "live/idecoder.h"
#include "live/irenderer.h"

namespace live {

// Pipeline配置
struct PipelineConfig {
    std::string url;           // 播放地址
    int decode_threads = 0;    // 解码线程数（0=自动）
    int video_width = 0;       // 强制窗口宽度（0=按视频）
    int video_height = 0;      // 强制窗口高度（0=按视频）
};

// Pipeline接口
class IPipeline {
public:
    virtual ~IPipeline() = default;
    
    // 初始化Pipeline
    virtual ErrorCode Init(const PipelineConfig& config) = 0;
    
    // 开始播放（阻塞直到播放结束）
    virtual ErrorCode Run() = 0;
    
    // 请求停止播放
    virtual void RequestStop() = 0;
    
    // 获取播放统计
    virtual int64_t GetPlayedFrames() const = 0;
    virtual int64_t GetDroppedFrames() const = 0;
};

using IPipelinePtr = std::unique_ptr<IPipeline>;

} // namespace live

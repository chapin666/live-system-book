#pragma once

#include "live/ipipeline.h"
#include "live/idemuxer.h"
#include "live/idecoder.h"
#include "live/irenderer.h"

namespace live {

// 简单的Pipeline实现
class PlayerPipeline : public IPipeline {
public:
    PlayerPipeline();
    ~PlayerPipeline() override;
    
    // 可以注入自定义模块（用于测试）
    PlayerPipeline(IDemuxerPtr demuxer, IDecoderPtr decoder, 
                   IRendererPtr renderer);

    ErrorCode Init(const PipelineConfig& config) override;
    ErrorCode Run() override;
    void RequestStop() override;
    int64_t GetPlayedFrames() const override;
    int64_t GetDroppedFrames() const override;

private:
    IDemuxerPtr demuxer_;
    IDecoderPtr decoder_;
    IRendererPtr renderer_;
    
    PipelineConfig config_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<int64_t> played_frames_{0};
    std::atomic<int64_t> dropped_frames_{0};
    bool initialized_ = false;
};

} // namespace live

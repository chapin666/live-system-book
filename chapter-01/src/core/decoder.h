#pragma once

#include "base/pipeline.h"
#include "base/ffmpeg_utils.h"

namespace live {

// 解码器：H.264/H.265 -> YUV
class Decoder {
public:
    Decoder();
    ~Decoder();
    
    // 禁止拷贝
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    
    // 初始化（根据流信息创建解码器）
    ErrorCode Init(const AVCodecParameters* codecpar);
    
    // 发送压缩数据包
    // 返回 true 表示可以接收帧（不一定立即有帧输出）
    bool SendPacket(const AVPacket* packet);
    
    // 接收解码后的帧
    // 返回 true 表示成功接收到一帧
    bool ReceiveFrame(AVFrame* frame);
    
    // 转换为 VideoFrame 结构
    bool ConvertToVideoFrame(const AVFrame* av_frame, 
                             int64_t frame_number,
                             VideoFrame* out_frame);
    
    // 获取视频宽高
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    CodecContextPtr dec_ctx_;
    const AVCodec* codec_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    AVRational time_base_;  // 时间基，用于计算 pts
};

} // namespace live

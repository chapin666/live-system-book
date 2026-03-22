#pragma once

#include "live/idemuxer.h"

struct AVPacket;
struct AVFrame;
struct AVCodecParameters;

namespace live {

// 解码器接口
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    // 初始化解码器
    virtual ErrorCode Init(const AVCodecParameters* params, 
                           int thread_count = 0) = 0;
    
    // 发送压缩数据到解码器
    virtual ErrorCode SendPacket(const AVPacket* packet) = 0;
    
    // 接收解码后的帧
    virtual ErrorCode ReceiveFrame(AVFrame* frame) = 0;
    
    // 获取视频宽高
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
    
    // 关闭解码器
    virtual void Close() = 0;
};

using IDecoderPtr = std::unique_ptr<IDecoder>;

} // namespace live

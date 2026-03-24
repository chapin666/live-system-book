#pragma once
#include <cstdint>

namespace live {

// 解码器接口
class IDecoder {
public:
    virtual ~IDecoder() = default;
    
    // 初始化和释放
    virtual bool Init(int codec_id, int width, int height) = 0;
    virtual void Close() = 0;
    
    // 发送packet解码
    virtual bool SendPacket(void* packet) = 0;
    
    // 接收解码后的帧
    virtual bool ReceiveFrame(void* frame) = 0;
    
    // 刷新解码器
    virtual void Flush() = 0;
};

// 创建FFmpeg解码器
std::unique_ptr<IDecoder> CreateFFmpegDecoder();

} // namespace live
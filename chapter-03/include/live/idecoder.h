#pragma once
#include <cstdint>

struct AVPacket;
struct AVFrame;

namespace live {

class IDecoder {
public:
    virtual ~IDecoder() = default;
    virtual bool Init(int codec_id, int width, int height) = 0;
    virtual bool SendPacket(const AVPacket* packet) = 0;
    virtual bool ReceiveFrame(AVFrame* frame) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
};

std::unique_ptr<IDecoder> CreateFFmpegDecoder();

} // namespace live

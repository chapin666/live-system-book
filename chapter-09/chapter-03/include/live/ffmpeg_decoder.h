#pragma once

#include "live/idecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace live {

class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;
    
    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    ErrorCode Init(const AVCodecParameters* params, 
                   int thread_count = 0) override;
    ErrorCode SendPacket(const AVPacket* packet) override;
    ErrorCode ReceiveFrame(AVFrame* frame) override;
    int GetWidth() const override;
    int GetHeight() const override;
    void Close() override;

private:
    AVCodecContext* codec_ctx_ = nullptr;
    const AVCodec* codec_ = nullptr;
    bool initialized_ = false;
};

} // namespace live

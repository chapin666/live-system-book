#include "live/idecoder.h"
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace live {

class FFmpegDecoder : public IDecoder {
public:
    FFmpegDecoder() = default;
    ~FFmpegDecoder() { Close(); }
    
    bool Init(int codec_id, int width, int height) override {
        const AVCodec* codec = avcodec_find_decoder(
            static_cast<AVCodecID>(codec_id));
        if (!codec) {
            std::cerr << "未找到解码器" << std::endl;
            return false;
        }
        
        codec_ctx_ = avcodec_alloc_context3(codec);
        codec_ctx_>width = width;
        codec_ctx_>height = height;
        
        int ret = avcodec_open2(codec_ctx_, codec, nullptr);
        if (ret < 0) {
            std::cerr << "无法打开解码器" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool SendPacket(const AVPacket* packet) override {
        int ret = avcodec_send_packet(codec_ctx_, packet);
        return ret >= 0 || ret == AVERROR(EAGAIN);
    }
    
    bool ReceiveFrame(AVFrame* frame) override {
        int ret = avcodec_receive_frame(codec_ctx_, frame);
        return ret >= 0;
    }
    
    void Flush() override {
        avcodec_send_packet(codec_ctx_, nullptr);
    }
    
    void Close() override {
        if (codec_ctx_) {
            avcodec_free_context(&codec_ctx_);
        }
    }

private:
    AVCodecContext* codec_ctx_ = nullptr;
};

std::unique_ptr<IDecoder> CreateFFmpegDecoder() {
    return std::make_unique<FFmpegDecoder>();
}

} // namespace live

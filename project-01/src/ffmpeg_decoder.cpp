#include "live/idecoder.h"
#include "live/raii_utils.h"
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
        const AVCodec* codec = avcodec_find_decoder((AVCodecID)codec_id);
        if (!codec) {
            std::cerr << "Codec not found" << std::endl;
            return false;
        }
        
        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_) {
            return false;
        }
        
        codec_ctx_->width = width;
        codec_ctx_->height = height;
        
        if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
            avcodec_free_context(&codec_ctx_);
            return false;
        }
        
        return true;
    }
    
    void Close() override {
        if (codec_ctx_) {
            avcodec_free_context(&codec_ctx_);
            codec_ctx_ = nullptr;
        }
    }
    
    bool SendPacket(void* packet) override {
        AVPacket* pkt = static_cast<AVPacket*>(packet);
        int ret = avcodec_send_packet(codec_ctx_, pkt);
        return ret >= 0 || ret == AVERROR(EAGAIN);
    }
    
    bool ReceiveFrame(void* frame) override {
        AVFrame* frm = static_cast<AVFrame*>(frame);
        int ret = avcodec_receive_frame(codec_ctx_, frm);
        return ret >= 0;
    }
    
    void Flush() override {
        avcodec_flush_buffers(codec_ctx_);
    }
    
private:
    AVCodecContext* codec_ctx_ = nullptr;
};

std::unique_ptr<IDecoder> CreateFFmpegDecoder() {
    return std::make_unique<FFmpegDecoder>();
}

} // namespace live
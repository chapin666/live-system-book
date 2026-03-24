#include "live/streamer.h"
#include <iostream>
#include <string>
#include <math>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

namespace live {

class Streamer::Impl {
public:
    bool Init(const StreamerConfig& cfg) {
        config = cfg;
        
        // 初始化输出
        char full_url[1024];
        snprintf(full_url, sizeof(full_url), "%s/%s", 
                 cfg.server_url, cfg.stream_key);
        
        if (avformat_alloc_output_context2(&fmt_ctx, nullptr, "flv", full_url) < 0) {
            return false;
        }
        
        // 添加视频流
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) return false;
        
        video_st = avformat_new_stream(fmt_ctx, codec);
        if (!video_st) return false;
        
        video_ctx = avcodec_alloc_context3(codec);
        if (!video_ctx) return false;
        
        video_ctx->width = cfg.video_width;
        video_ctx->height = cfg.video_height;
        video_ctx->time_base = {1, cfg.video_fps};
        video_ctx->framerate = {cfg.video_fps, 1};
        video_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        video_ctx->bit_rate = cfg.video_bitrate;
        
        // 设置编码器参数
        av_opt_set(video_ctx->priv_data, "preset", "fast", 0);
        av_opt_set(video_ctx->priv_data, "tune", "zerolatency", 0);
        
        if (avcodec_open2(video_ctx, codec, nullptr) < 0) {
            return false;
        }
        
        avcodec_parameters_from_context(video_st->codecpar, video_ctx);
        
        // 打开输出
        if (avio_open(&fmt_ctx->pb, full_url, AVIO_FLAG_WRITE) < 0) {
            return false;
        }
        
        if (avformat_write_header(fmt_ctx, nullptr) < 0) {
            return false;
        }
        
        return true;
    }
    
    void Shutdown() {
        if (fmt_ctx) {
            av_write_trailer(fmt_ctx);
            if (fmt_ctx->pb) {
                avio_closep(&fmt_ctx->pb);
            }
            avformat_free_context(fmt_ctx);
            fmt_ctx = nullptr;
        }
        if (video_ctx) {
            avcodec_free_context(&video_ctx);
        }
    }
    
    bool StartStream() {
        is_streaming = true;
        start_time = av_gettime();
        return true;
    }
    
    void StopStream() {
        is_streaming = false;
    }
    
    StreamerConfig config;
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* video_ctx = nullptr;
    AVStream* video_st = nullptr;
    bool is_streaming = false;
    int64_t bytes_sent = 0;
    int64_t start_time = 0;
};

Streamer::Streamer() : impl_(std::make_unique<Impl>()) {}
Streamer::~Streamer() = default;

bool Streamer::Init(const StreamerConfig& config) {
    return impl_->Init(config);
}

void Streamer::Shutdown() {
    impl_->Shutdown();
}

bool Streamer::StartStream() {
    return impl_->StartStream();
}

void Streamer::StopStream() {
    impl_->StopStream();
}

bool Streamer::IsStreaming() const {
    return impl_->is_streaming;
}

int64_t Streamer::GetBytesSent() const {
    return impl_->bytes_sent;
}

float Streamer::GetNetworkBitrate() const {
    if (!impl_->is_streaming || impl_->start_time == 0) return 0.0f;
    
    int64_t elapsed = av_gettime() - impl_->start_time;
    if (elapsed <= 0) return 0.0f;
    
    return impl_->bytes_sent * 8.0f / (elapsed / 1000000.0f) / 1000.0f;  // kbps
}

} // namespace live
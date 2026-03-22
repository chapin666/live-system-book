#include "live/ffmpeg_decoder.h"
#include <cstdio>

namespace live {

FFmpegDecoder::FFmpegDecoder() = default;

FFmpegDecoder::~FFmpegDecoder() {
    Close();
}

ErrorCode FFmpegDecoder::Init(const AVCodecParameters* params, 
                               int thread_count) {
    if (!params) {
        return ErrorCode::OpenFailed;
    }
    
    // 1. 查找解码器
    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) {
        printf("[FFmpegDecoder] Codec not found for id=%d\n", 
               params->codec_id);
        return ErrorCode::OpenFailed;
    }
    
    printf("[FFmpegDecoder] Using codec: %s\n", codec_->name);
    
    // 2. 分配解码器上下文
    codec_ctx_ = avcodec_alloc_context3(codec_);
    if (!codec_ctx_) {
        return ErrorCode::OpenFailed;
    }
    
    // 3. 复制参数到上下文
    int ret = avcodec_parameters_to_context(codec_ctx_, params);
    if (ret < 0) {
        printf("[FFmpegDecoder] Failed to copy codec params\n");
        Close();
        return ErrorCode::OpenFailed;
    }
    
    // 4. 配置多线程解码
    if (thread_count > 0) {
        codec_ctx_->thread_count = thread_count;
        codec_ctx_->thread_type = FF_THREAD_FRAME;
    } else {
        codec_ctx_->thread_count = 0;  // 自动
    }
    
    // 5. 打开解码器
    ret = avcodec_open2(codec_ctx_, codec_, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[FFmpegDecoder] Failed to open codec: %s\n", errbuf);
        Close();
        return ErrorCode::OpenFailed;
    }
    
    printf("[FFmpegDecoder] Initialized: %dx%d, threads=%d\n",
           GetWidth(), GetHeight(), codec_ctx_->thread_count);
    
    initialized_ = true;
    return ErrorCode::OK;
}

ErrorCode FFmpegDecoder::SendPacket(const AVPacket* packet) {
    if (!initialized_ || !codec_ctx_) {
        return ErrorCode::ReadError;
    }
    
    int ret = avcodec_send_packet(codec_ctx_, packet);
    
    if (ret == 0 || ret == AVERROR(EAGAIN)) {
        return ErrorCode::OK;
    } else if (ret == AVERROR_EOF) {
        return ErrorCode::EndOfFile;
    } else {
        return ErrorCode::ReadError;
    }
}

ErrorCode FFmpegDecoder::ReceiveFrame(AVFrame* frame) {
    if (!initialized_ || !codec_ctx_) {
        return ErrorCode::ReadError;
    }
    
    int ret = avcodec_receive_frame(codec_ctx_, frame);
    
    if (ret == 0) {
        return ErrorCode::OK;
    } else if (ret == AVERROR(EAGAIN)) {
        return ErrorCode::OK;  // 需要更多数据
    } else if (ret == AVERROR_EOF) {
        return ErrorCode::EndOfFile;
    } else {
        return ErrorCode::ReadError;
    }
}

int FFmpegDecoder::GetWidth() const {
    return codec_ctx_ ? codec_ctx_->width : 0;
}

int FFmpegDecoder::GetHeight() const {
    return codec_ctx_ ? codec_ctx_->height : 0;
}

void FFmpegDecoder::Close() {
    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
    codec_ = nullptr;
    initialized_ = false;
}

} // namespace live

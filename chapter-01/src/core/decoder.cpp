#include "decoder.h"
#include <cstdio>

namespace live {

Decoder::Decoder() = default;
Decoder::~Decoder() = default;

ErrorCode Decoder::Init(const AVCodecParameters* codecpar) {
    // 1. 找到解码器
    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        printf("[Decoder] Codec not found\n");
        return ErrorCode::InitFailed;
    }
    
    printf("[Decoder] Using codec: %s\n", codec_->name);
    
    // 2. 分配解码器上下文
    AVCodecContext* raw_ctx = avcodec_alloc_context3(codec_);
    if (!raw_ctx) {
        return ErrorCode::InitFailed;
    }
    dec_ctx_.reset(raw_ctx);
    
    // 3. 复制参数到上下文
    int ret = avcodec_parameters_to_context(dec_ctx_.get(), codecpar);
    if (ret < 0) {
        return ErrorCode::InitFailed;
    }
    
    // 4. 打开解码器
    ret = avcodec_open2(dec_ctx_.get(), codec_, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[Decoder] Failed to open codec: %s\n", errbuf);
        return ErrorCode::InitFailed;
    }
    
    // 保存信息
    width_ = dec_ctx_->width;
    height_ = dec_ctx_->height;
    time_base_ = dec_ctx_->time_base;
    
    printf("[Decoder] Initialized: %dx%d, time_base=%d/%d\n",
           width_, height_, time_base_.num, time_base_.den);
    
    return ErrorCode::OK;
}

bool Decoder::SendPacket(const AVPacket* packet) {
    // packet 可以是 nullptr（表示刷新解码器）
    int ret = avcodec_send_packet(dec_ctx_.get(), packet);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // 需要 ReceiveFrame 后再发送，不算错误
            return true;
        }
        return false;
    }
    return true;
}

bool Decoder::ReceiveFrame(AVFrame* frame) {
    int ret = avcodec_receive_frame(dec_ctx_.get(), frame);
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            // 需要更多数据或已结束
            return false;
        }
        return false;
    }
    return true;
}

bool Decoder::ConvertToVideoFrame(const AVFrame* av_frame, 
                                   int64_t frame_number,
                                   VideoFrame* out_frame) {
    // 检查格式（我们只处理 YUV420P）
    if (av_frame->format != AV_PIX_FMT_YUV420P) {
        printf("[Decoder] Unsupported pixel format: %d\n", av_frame->format);
        return false;
    }
    
    // 填充 VideoFrame
    for (int i = 0; i < 3; i++) {
        out_frame->data[i] = av_frame->data[i];
        out_frame->linesize[i] = av_frame->linesize[i];
    }
    
    out_frame->width = av_frame->width;
    out_frame->height = av_frame->height;
    
    // 计算 pts（微秒）
    // pts 在 AVFrame 中是 time_base 为单位，需要转换
    if (av_frame->pts != AV_NOPTS_VALUE) {
        out_frame->pts = av_rescale_q(av_frame->pts, time_base_, {1, 1000000});
    } else {
        out_frame->pts = 0;
    }
    
    // 估算帧持续时间（根据帧率）
    if (dec_ctx_->frame_rate.num > 0) {
        int64_t frame_duration = 1000000LL * dec_ctx_->frame_rate.den / dec_ctx_->frame_rate.num;
        out_frame->duration = frame_duration;
    } else {
        out_frame->duration = 33333;  // 默认 30fps = 33.3ms
    }
    
    out_frame->frame_number = frame_number;
    
    // 注意：我们没有复制数据，只是引用了指针
    // 上层需要确保在 AVFrame 被释放前使用完 VideoFrame
    out_frame->release_callback = nullptr;
    
    return true;
}

} // namespace live

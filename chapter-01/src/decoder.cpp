#include "decoder.h"
#include <cstring>

namespace live {

Decoder::Decoder() = default;

Decoder::~Decoder() {
    Close();
}

bool Decoder::Init(const AVCodecParameters* codecpar) {
    Close();

    // 1. 找到解码器
    // codec_id 可能是 AV_CODEC_ID_H264, AV_CODEC_ID_HEVC, AV_CODEC_ID_VP9 等
    codec_ = avcodec_find_decoder(codecpar->codec_id);
    if (!codec_) {
        error_ = "找不到解码器";
        return false;
    }
    printf("[Decoder] 使用解码器: %s\n", codec_->name);

    // 2. 分配解码器上下文
    dec_ctx_ = avcodec_alloc_context3(codec_);
    if (!dec_ctx_) {
        error_ = "分配解码器上下文失败";
        return false;
    }

    // 3. 复制编解码器参数到上下文
    // 这会把分辨率、码率、extradata（SPS/PPS）等复制过去
    int ret = avcodec_parameters_to_context(dec_ctx_, codecpar);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "复制编解码器参数失败: " + std::string(errbuf);
        Close();
        return false;
    }

    // 4. 打开解码器
    // 参数3: options，可以设置线程数等
    ret = avcodec_open2(dec_ctx_, codec_, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "打开解码器失败: " + std::string(errbuf);
        Close();
        return false;
    }

    // 保存一些常用参数
    width_ = dec_ctx_->width;
    height_ = dec_ctx_->height;
    pix_fmt_ = dec_ctx_->pix_fmt;

    printf("[Decoder] 初始化完成: %dx%d, format=%d\n", width_, height_, pix_fmt_);
    initialized_ = true;
    return true;
}

void Decoder::Close() {
    if (dec_ctx_) {
        avcodec_free_context(&dec_ctx_);
        dec_ctx_ = nullptr;
    }
    codec_ = nullptr;
    width_ = 0;
    height_ = 0;
    pix_fmt_ = AV_PIX_FMT_NONE;
    initialized_ = false;
    error_.clear();
}

bool Decoder::SendPacket(const AVPacket* packet) {
    if (!initialized_ || !dec_ctx_) {
        error_ = "解码器未初始化";
        return false;
    }

    // 发送压缩数据到解码器
    // 注意：packet 可以是 nullptr，表示刷新（EOF）
    int ret = avcodec_send_packet(dec_ctx_, packet);
    
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // 需要 ReceiveFrame 后再发送，不算错误
            return true;
        }
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "发送 packet 失败: " + std::string(errbuf);
        return false;
    }

    return true;
}

bool Decoder::ReceiveFrame(AVFrame* frame) {
    if (!initialized_ || !dec_ctx_) {
        error_ = "解码器未初始化";
        return false;
    }

    // 接收解码后的帧
    int ret = avcodec_receive_frame(dec_ctx_, frame);
    
    if (ret < 0) {
        if (ret == AVERROR(EAGAIN)) {
            // 需要更多输入数据，不算错误
            return false;
        }
        if (ret == AVERROR_EOF) {
            // 解码完成
            return false;
        }
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "接收 frame 失败: " + std::string(errbuf);
        return false;
    }

    return true;
}

void Decoder::Flush() {
    if (!initialized_ || !dec_ctx_) {
        return;
    }
    // 送空 packet 触发解码器刷新内部缓冲
    avcodec_send_packet(dec_ctx_, nullptr);
}

} // namespace live

#include "decoder_thread.h"
#include <iostream>

namespace live {

DecoderThread::DecoderThread(const DecoderConfig& config)
    : config_(config)
    , queue_(std::make_unique<FrameQueue>(config.queue_max_size)) {
}

DecoderThread::~DecoderThread() {
    Stop();
}

bool DecoderThread::Start() {
    // 打开文件（在主线程做，方便错误处理）
    int ret = avformat_open_input(&fmt_ctx_, config_.filename.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法打开文件: " << errbuf << std::endl;
        return false;
    }

    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        std::cerr << "无法获取流信息" << std::endl;
        avformat_close_input(&fmt_ctx_);
        return false;
    }

    // 查找视频流
    video_stream_idx_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx_ < 0) {
        std::cerr << "未找到视频流" << std::endl;
        avformat_close_input(&fmt_ctx_);
        return false;
    }

    AVStream* stream = fmt_ctx_->streams[video_stream_idx_];
    time_base_ = stream->time_base;

    // 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        std::cerr << "未找到解码器" << std::endl;
        avformat_close_input(&fmt_ctx_);
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx_, stream->codecpar);
    
    // 启用多线程解码
    codec_ctx_->thread_count = 4;
    codec_ctx_->thread_type = FF_THREAD_FRAME;

    ret = avcodec_open2(codec_ctx_, codec, nullptr);
    if (ret < 0) {
        std::cerr << "无法打开解码器" << std::endl;
        avcodec_free_context(&codec_ctx_);
        avformat_close_input(&fmt_ctx_);
        return false;
    }

    width_ = codec_ctx_->width;
    height_ = codec_ctx_->height;

    std::cout << "[Decoder] " << width_ << "x" << height_ 
              << ", queue_size=" << config_.queue_max_size << std::endl;

    // 启动线程
    running_.store(true);
    should_stop_.store(false);
    thread_ = std::thread(&DecoderThread::Run, this);

    return true;
}

void DecoderThread::Stop() {
    if (!running_.load()) return;

    std::cout << "[Decoder] Stopping..." << std::endl;
    
    should_stop_.store(true);
    queue_->Stop();  // 唤醒等待的线程

    if (thread_.joinable()) {
        thread_.join();
    }

    // 清理资源
    avcodec_free_context(&codec_ctx_);
    avformat_close_input(&fmt_ctx_);
    
    running_.store(false);
    std::cout << "[Decoder] Stopped" << std::endl;
}

void DecoderThread::Run() {
    std::cout << "[Decoder] Thread started" << std::endl;

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int64_t frame_count = 0;

    // 解码循环
    while (!should_stop_.load()) {
        // 读取 packet
        int ret = av_read_frame(fmt_ctx_, packet);
        if (ret < 0) {
            break;  // 文件结束或错误
        }

        if (packet->stream_index != video_stream_idx_) {
            av_packet_unref(packet);
            continue;
        }

        // 发送 packet
        ret = avcodec_send_packet(codec_ctx_, packet);
        av_packet_unref(packet);

        if (ret < 0) {
            continue;
        }

        // 接收 frame
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx_, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                break;
            }

            // 复制 frame（解码器会重用内部缓冲区）
            AVFrame* cloned = av_frame_clone(frame);
            
            // 送入队列（阻塞模式）
            auto state = queue_->Push(cloned, true);
            if (state == QueueState::Stopped) {
                av_frame_free(&cloned);
                break;
            }

            frame_count++;
            if (frame_count % 30 == 0) {
                std::cout << "[Decoder] Decoded " << frame_count 
                          << " frames, queue=" << queue_->Size() << std::endl;
            }
        }

        if (should_stop_.load()) break;
    }

    // 刷新解码器
    avcodec_send_packet(codec_ctx_, nullptr);
    while (avcodec_receive_frame(codec_ctx_, frame) == 0) {
        AVFrame* cloned = av_frame_clone(frame);
        if (queue_->Push(cloned, true) == QueueState::Stopped) {
            av_frame_free(&cloned);
            break;
        }
    }

    std::cout << "[Decoder] Total decoded: " << frame_count << std::endl;

    av_frame_free(&frame);
    av_packet_free(&packet);
}

} // namespace live

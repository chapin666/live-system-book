#pragma once
#include "live/frame_queue.h"
#include <thread>
#include <atomic>
#include <string>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace live {

struct DecoderConfig {
    std::string filename;
    size_t queue_max_size = 3;  // 默认3帧缓冲
};

class DecoderThread {
public:
    explicit DecoderThread(const DecoderConfig& config);
    ~DecoderThread();

    // 禁止拷贝
    DecoderThread(const DecoderThread&) = delete;
    DecoderThread& operator=(const DecoderThread&) = delete;

    // 启动和停止
    bool Start();
    void Stop();
    bool IsRunning() const { return running_.load(); }

    // 获取队列（供主线程消费）
    FrameQueue* GetFrameQueue() { return queue_.get(); }
    
    // 获取视频信息
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    AVRational GetTimeBase() const { return time_base_; }

private:
    void Run();  // 线程主函数

    DecoderConfig config_;
    std::unique_ptr<FrameQueue> queue_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};

    // FFmpeg 上下文（线程内部使用）
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    int video_stream_idx_ = -1;
    
    // 视频信息
    int width_ = 0;
    int height_ = 0;
    AVRational time_base_{0, 1};
};

} // namespace live

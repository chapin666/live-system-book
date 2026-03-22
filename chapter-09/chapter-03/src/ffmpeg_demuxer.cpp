#include "live/ffmpeg_demuxer.h"
#include <cstdio>

namespace live {

FFmpegDemuxer::FFmpegDemuxer() = default;

FFmpegDemuxer::~FFmpegDemuxer() {
    Close();
}

ErrorCode FFmpegDemuxer::Open(const std::string& url) {
    // 1. 打开输入文件
    int ret = avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[FFmpegDemuxer] Failed to open '%s': %s\n", url.c_str(), errbuf);
        return ErrorCode::FileNotFound;
    }
    
    // 2. 获取流信息
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        printf("[FFmpegDemuxer] Failed to find stream info\n");
        Close();
        return ErrorCode::OpenFailed;
    }
    
    // 3. 查找视频流
    video_stream_index_ = av_find_best_stream(
        fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    
    if (video_stream_index_ < 0) {
        printf("[FFmpegDemuxer] No video stream found\n");
        Close();
        return ErrorCode::OpenFailed;
    }
    
    // 4. 打印信息
    AVStream* stream = fmt_ctx_->streams[video_stream_index_];
    printf("[FFmpegDemuxer] Opened: %s\n", url.c_str());
    printf("[FFmpegDemuxer] Video: %dx%d @ %.2f fps, "
           "duration=%.2fs\n",
           stream->codecpar->width,
           stream->codecpar->height,
           av_q2d(stream->avg_frame_rate),
           GetDurationMs() / 1000.0);
    
    opened_ = true;
    return ErrorCode::OK;
}

ErrorCode FFmpegDemuxer::ReadPacket(AVPacket* packet) {
    if (!opened_ || !fmt_ctx_) {
        return ErrorCode::ReadError;
    }
    
    // 循环读取，直到读到视频包或文件结束
    while (true) {
        int ret = av_read_frame(fmt_ctx_, packet);
        
        if (ret == AVERROR_EOF) {
            return ErrorCode::EndOfFile;
        }
        if (ret < 0) {
            return ErrorCode::ReadError;
        }
        
        // 只返回视频流的数据包
        if (packet->stream_index == video_stream_index_) {
            return ErrorCode::OK;
        }
        
        // 跳过非视频包
        av_packet_unref(packet);
    }
}

const AVCodecParameters* FFmpegDemuxer::GetVideoParams() const {
    if (!opened_ || video_stream_index_ < 0 || !fmt_ctx_) {
        return nullptr;
    }
    return fmt_ctx_->streams[video_stream_index_]->codecpar;
}

int64_t FFmpegDemuxer::GetDurationMs() const {
    if (!fmt_ctx_) return 0;
    return fmt_ctx_->duration * 1000 / AV_TIME_BASE;
}

void FFmpegDemuxer::Close() {
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    video_stream_index_ = -1;
    opened_ = false;
}

} // namespace live

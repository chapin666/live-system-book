#include "demuxer.h"
#include <cstdio>

namespace live {

Demuxer::Demuxer() = default;
Demuxer::~Demuxer() = default;

ErrorCode Demuxer::Init(const std::string& url) {
    // 打开输入文件
    AVFormatContext* raw_ctx = nullptr;
    int ret = avformat_open_input(&raw_ctx, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[Demuxer] Failed to open %s: %s\n", url.c_str(), errbuf);
        return ErrorCode::ResourceNotFound;
    }
    
    // 使用智能指针接管所有权
    format_ctx_.reset(raw_ctx);
    
    // 读取流信息（解析 moov box 等）
    ret = avformat_find_stream_info(format_ctx_.get(), nullptr);
    if (ret < 0) {
        return ErrorCode::InitFailed;
    }
    
    // 找到视频流
    video_stream_index_ = av_find_best_stream(
        format_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    
    if (video_stream_index_ < 0) {
        printf("[Demuxer] No video stream found\n");
        return ErrorCode::ResourceNotFound;
    }
    
    // 打印信息
    AVStream* stream = format_ctx_->streams[video_stream_index_];
    printf("[Demuxer] Video stream #%d: %dx%d @ %.2f fps\n",
           video_stream_index_,
           stream->codecpar->width,
           stream->codecpar->height,
           av_q2d(stream->avg_frame_rate));
    
    initialized_ = true;
    return ErrorCode::OK;
}

PacketPtr Demuxer::ReadPacket() {
    if (!initialized_) {
        return nullptr;
    }
    
    PacketPtr packet = MakePacket();
    int ret = av_read_frame(format_ctx_.get(), packet.get());
    
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            // 正常结束
            return nullptr;
        }
        // 错误
        return nullptr;
    }
    
    // 只返回视频包（跳过音频）
    if (packet->stream_index != video_stream_index_) {
        av_packet_unref(packet.get());
        return ReadPacket();  // 递归读取下一个
    }
    
    return packet;
}

const AVCodecParameters* Demuxer::GetVideoStreamInfo() const {
    if (!initialized_ || video_stream_index_ < 0) {
        return nullptr;
    }
    return format_ctx_->streams[video_stream_index_]->codecpar;
}

int64_t Demuxer::GetDurationMs() const {
    if (!format_ctx_) return 0;
    // duration 是 AV_TIME_BASE 为单位（1/1000000 秒）
    return format_ctx_->duration * 1000 / AV_TIME_BASE;
}

} // namespace live

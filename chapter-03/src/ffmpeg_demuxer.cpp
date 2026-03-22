#include "live/idemuxer.h"
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

class FFmpegDemuxer : public IDemuxer {
public:
    FFmpegDemuxer() = default;
    ~FFmpegDemuxer() { Close(); }
    
    bool Open(const char* url) override {
        int ret = avformat_open_input(&fmt_ctx_, url, nullptr, nullptr);
        if (ret < 0) {
            std::cerr << "无法打开输入: " << url << std::endl;
            return false;
        }
        
        ret = avformat_find_stream_info(fmt_ctx_, nullptr);
        if (ret < 0) {
            std::cerr << "无法获取流信息" << std::endl;
            return false;
        }
        
        video_stream_idx_ = av_find_best_stream(
            fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (video_stream_idx_ < 0) {
            std::cerr << "未找到视频流" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool GetVideoStreamInfo(StreamInfo& info) override {
        if (video_stream_idx_ < 0) return false;
        
        AVStream* st = fmt_ctx_->streams[video_stream_idx_];
        info.index = video_stream_idx_;
        info.codec_id = st->ccodecpar->codec_id;
        info.width = st->ccodecpar->width;
        info.height = st->ccodecpar->height;
        return true;
    }
    
    bool ReadPacket(AVPacket* packet) override {
        while (av_read_frame(fmt_ctx_, packet) >= 0) {
            if (packet->stream_index == video_stream_idx_) {
                return true;
            }
            av_packet_unref(packet);
        }
        return false;
    }
    
    void Close() override {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
            fmt_ctx_ = nullptr;
        }
    }

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_idx_ = -1;
};

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer() {
    return std::make_unique<FFmpegDemuxer>();
}

} // namespace live

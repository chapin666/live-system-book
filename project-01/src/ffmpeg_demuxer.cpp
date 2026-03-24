#include "live/idemuxer.h"
#include "live/raii_utils.h"
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
        if (avformat_open_input(&fmt_ctx_, url, nullptr, nullptr) < 0) {
            return false;
        }
        
        if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
            avformat_close_input(&fmt_ctx_);
            return false;
        }
        
        // 找到视频流
        video_stream_idx_ = -1;
        for (unsigned i = 0; i < fmt_ctx_->nb_streams; i++) {
            if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx_ = i;
                break;
            }
        }
        
        if (video_stream_idx_ == -1) {
            avformat_close_input(&fmt_ctx_);
            return false;
        }
        
        return true;
    }
    
    void Close() override {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
            fmt_ctx_ = nullptr;
        }
    }
    
    bool ReadPacket(void* packet) override {
        AVPacket* pkt = static_cast<AVPacket*>(packet);
        while (av_read_frame(fmt_ctx_, pkt) >= 0) {
            if (pkt->stream_index == video_stream_idx_) {
                return true;
            }
            av_packet_unref(pkt);
        }
        return false;
    }
    
    bool GetVideoStreamInfo(StreamInfo& info) override {
        if (video_stream_idx_ == -1 || !fmt_ctx_) return false;
        
        AVCodecParameters* codecpar = fmt_ctx_->streams[video_stream_idx_]->codecpar;
        info.width = codecpar->width;
        info.height = codecpar->height;
        info.codec_id = codecpar->codec_id;
        info.bitrate = codecpar->bit_rate;
        
        AVRational fps = fmt_ctx_->streams[video_stream_idx_]->avg_frame_rate;
        info.fps = av_q2d(fps);
        return true;
    }
    
    int64_t GetDuration() const override {
        if (!fmt_ctx_) return 0;
        return fmt_ctx_->duration;
    }
    
private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_idx_ = -1;
};

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer() {
    return std::make_unique<FFmpegDemuxer>();
}

} // namespace live
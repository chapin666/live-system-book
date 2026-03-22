#pragma once

#include "live/idemuxer.h"

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

class FFmpegDemuxer : public IDemuxer {
public:
    FFmpegDemuxer();
    ~FFmpegDemuxer() override;
    
    FFmpegDemuxer(const FFmpegDemuxer&) = delete;
    FFmpegDemuxer& operator=(const FFmpegDemuxer&) = delete;

    ErrorCode Open(const std::string& url) override;
    ErrorCode ReadPacket(AVPacket* packet) override;
    const AVCodecParameters* GetVideoParams() const override;
    int64_t GetDurationMs() const override;
    void Close() override;

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
    bool opened_ = false;
};

} // namespace live

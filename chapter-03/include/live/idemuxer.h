#pragma once
#include <cstdint>
#include <vector>
#include <memory>

struct AVPacket;

namespace live {

struct StreamInfo {
    int index;
    int codec_id;
    int width, height;
    int sample_rate;
    int channels;
};

class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    virtual bool Open(const char* url) = 0;
    virtual bool GetVideoStreamInfo(StreamInfo& info) = 0;
    virtual bool ReadPacket(AVPacket* packet) = 0;
    virtual void Close() = 0;
};

std::unique_ptr<IDemuxer> CreateFFmpegDemuxer();

} // namespace live

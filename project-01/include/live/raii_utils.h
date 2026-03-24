#pragma once
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace live {

// RAII 包装器
struct AVFrameDeleter {
    void operator()(AVFrame* frame) {
        if (frame) av_frame_free(&frame);
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* packet) {
        if (packet) av_packet_free(&packet);
    }
};

using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

} // namespace live
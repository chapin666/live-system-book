#pragma once
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <memory>

namespace live {

struct AVFrameDeleter {
    void operator()(AVFrame* p) { 
        if (p) av_frame_free(&p); 
    }
};
using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;

struct AVPacketDeleter {
    void operator()(AVPacket* p) { 
        if (p) av_packet_free(&p); 
    }
};
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* p) { 
        if (p) avcodec_free_context(&p); 
    }
};
using CodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* p) { 
        if (p) avformat_close_input(&p); 
    }
};
using FormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;

} // namespace live

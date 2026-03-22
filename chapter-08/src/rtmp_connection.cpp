#include <live/rtmp_connection.h>
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

namespace live {

RtmpConnection::RtmpConnection() = default;

RtmpConnection::~RtmpConnection() {
    Disconnect();
}

bool RtmpConnection::Connect(const std::string& url) {
    state_ = RtmpState::Connecting;
    
    // 初始化网络库
    avformat_network_init();
    
    if (!OpenInput(url)) {
        state_ = RtmpState::Error;
        return false;
    }
    
    state_ = RtmpState::Connected;
    return true;
}

bool RtmpConnection::OpenInput(const std::string& url) {
    AVFormatContext* fmt_ctx = nullptr;
    
    // 设置选项
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "buffer_size", "65536", 0);
    av_dict_set(&opts, "max_delay", "500000", 0); // 500ms
    
    // 打开输入
    int ret = avformat_open_input(&fmt_ctx, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "Failed to open input: " << errbuf << std::endl;
        return false;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Failed to find stream info" << std::endl;
        avformat_close_input(&fmt_ctx);
        return false;
    }
    
    // 打印信息
    std::cout << "RTMP Stream Info:" << std::endl;
    av_dump_format(fmt_ctx, 0, url.c_str(), 0);
    
    // 查找视频流
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream* stream = fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index_ = i;
            std::cout << "Video stream index: " << i << std::endl;
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index_ = i;
            std::cout << "Audio stream index: " << i << std::endl;
        }
    }
    
    format_ctx_ = fmt_ctx;
    return true;
}

bool RtmpConnection::Play(const std::string& stream_name) {
    if (state_ != RtmpState::Connected) {
        return false;
    }
    
    state_ = RtmpState::Playing;
    std::cout << "Start playing stream: " << stream_name << std::endl;
    return true;
}

void RtmpConnection::Disconnect() {
    if (format_ctx_) {
        avformat_close_input((AVFormatContext**)&format_ctx_);
        format_ctx_ = nullptr;
    }
    state_ = RtmpState::Disconnected;
    video_stream_index_ = -1;
    audio_stream_index_ = -1;
}

int RtmpConnection::ReadData(uint8_t* buffer, int size) {
    if (!format_ctx_ || state_ != RtmpState::Playing) {
        return -1;
    }
    
    AVFormatContext* fmt_ctx = (AVFormatContext*)format_ctx_;
    AVPacket* pkt = av_packet_alloc();
    
    int ret = av_read_frame(fmt_ctx, pkt);
    if (ret < 0) {
        av_packet_free(&pkt);
        return ret;
    }
    
    // 复制数据到缓冲区
    int copy_size = pkt->size < size ? pkt->size : size;
    memcpy(buffer, pkt->data, copy_size);
    
    av_packet_free(&pkt);
    return copy_size;
}

} // namespace live

#pragma once

#include <string>
#include <vector>
#include <functional>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

// RTMP连接状态
enum class RtmpState {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    Playing,
    Error
};

// RTMP连接类（简化版，使用FFmpeg实现）
class RtmpConnection {
public:
    RtmpConnection();
    ~RtmpConnection();

    // 禁止拷贝
    RtmpConnection(const RtmpConnection&) = delete;
    RtmpConnection& operator=(const RtmpConnection&) = delete;

    // 连接到RTMP服务器
    bool Connect(const std::string& url);

    // 断开连接
    void Disconnect();

    // 开始播放流
    bool Play(const std::string& stream_name);

    // 获取当前状态
    RtmpState GetState() const { return state_; }

    // 读取数据（阻塞）
    int ReadData(uint8_t* buffer, int size);

private:
    RtmpState state_ = RtmpState::Disconnected;
    AVFormatContext* format_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    int video_stream_index_ = -1;
    int audio_stream_index_ = -1;

    bool OpenInput(const std::string& url);
};

} // namespace live

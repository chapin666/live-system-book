#pragma once

#include "base/pipeline.h"
#include "base/ffmpeg_utils.h"

namespace live {

// 解封装器：从文件/URL读取压缩数据
class Demuxer {
public:
    Demuxer();
    ~Demuxer();
    
    // 禁止拷贝
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;
    
    // 初始化（打开文件）
    ErrorCode Init(const std::string& url);
    
    // 读取一个压缩数据包
    // 返回 nullptr 表示文件结束
    PacketPtr ReadPacket();
    
    // 获取视频流信息（用于初始化解码器）
    const AVCodecParameters* GetVideoStreamInfo() const;
    
    // 获取时长（毫秒）
    int64_t GetDurationMs() const;

private:
    FormatContextPtr format_ctx_;
    int video_stream_index_ = -1;
    bool initialized_ = false;
};

} // namespace live

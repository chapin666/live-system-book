#pragma once
#include <cstdint>

namespace live {

// 视频流信息
struct StreamInfo {
    int width = 0;
    int height = 0;
    int codec_id = 0;
    int64_t bitrate = 0;
    double fps = 0.0;
};

// 解封装器接口
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    
    // 打开输入
    virtual bool Open(const char* url) = 0;
    virtual void Close() = 0;
    
    // 读取数据包
    virtual bool ReadPacket(void* packet) = 0;
    
    // 获取流信息
    virtual bool GetVideoStreamInfo(StreamInfo& info) = 0;
    virtual int64_t GetDuration() const = 0;
};

// 创建FFmpeg解封装器
std::unique_ptr<IDemuxer> CreateFFmpegDemuxer();

} // namespace live
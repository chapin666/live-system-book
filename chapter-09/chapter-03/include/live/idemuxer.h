#pragma once

#include <string>
#include <memory>

// FFmpeg前向声明
struct AVPacket;
struct AVCodecParameters;

namespace live {

// 错误码
enum class ErrorCode {
    OK = 0,
    FileNotFound,
    OpenFailed,
    ReadError,
    EndOfFile,
    Unknown
};

// 解封装器接口
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    
    // 打开文件/URL
    virtual ErrorCode Open(const std::string& url) = 0;
    
    // 读取一个压缩数据包
    virtual ErrorCode ReadPacket(AVPacket* packet) = 0;
    
    // 获取视频流参数
    virtual const AVCodecParameters* GetVideoParams() const = 0;
    
    // 获取视频时长（毫秒）
    virtual int64_t GetDurationMs() const = 0;
    
    // 关闭并释放资源
    virtual void Close() = 0;
};

using IDemuxerPtr = std::unique_ptr<IDemuxer>;

} // namespace live

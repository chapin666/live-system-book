#ifndef DEMUXER_H
#define DEMUXER_H

#include <string>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

// 解封装器：从文件/URL读取压缩数据包
class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    // 禁止拷贝
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    // 打开输入文件
    // @param url: 文件路径或网络URL (如 "file:///tmp/video.mp4" 或 "rtmp://...")
    // @return: 成功返回true
    bool Open(const std::string& url);

    // 关闭并释放资源
    void Close();

    // 读取一个数据包
    // @param packet: 输出参数，存储读取到的压缩数据
    // @return: 成功返回true，EOF返回false，错误返回false并设置error_
    bool ReadPacket(AVPacket* packet);

    // 获取视频流索引（方便后面过滤）
    int video_stream_index() const { return video_stream_index_; }

    // 获取视频流信息
    AVStream* video_stream() const;

    // 获取时长（秒）
    double duration() const;

    // 获取错误信息
    const std::string& error() const { return error_; }

private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_index_ = -1;
    std::string error_;
    bool opened_ = false;
};

} // namespace live

#endif // DEMUXER_H

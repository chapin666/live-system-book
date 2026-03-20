#ifndef DECODER_H
#define DECODER_H

#include <string>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

namespace live {

// 视频解码器：H.264/H.265 -> YUV
class Decoder {
public:
    Decoder();
    ~Decoder();

    // 禁止拷贝
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;

    // 初始化解码器
    // @param codecpar: 从 AVStream->codecpar 获取的编解码器参数
    // @return: 成功返回true
    bool Init(const AVCodecParameters* codecpar);

    // 关闭解码器
    void Close();

    // 发送压缩数据包
    // @param packet: 压缩数据（H.264 NAL单元等）
    // @return: 成功返回true，需要更多数据返回true（EAGAIN），错误返回false
    bool SendPacket(const AVPacket* packet);

    // 接收解码后的帧
    // @param frame: 输出参数，存储解码后的YUV帧
    // @return: 成功返回true，需要更多输入返回false（EAGAIN），EOF返回false
    bool ReceiveFrame(AVFrame* frame);

    // 刷新解码器（送空packet，取出缓冲中的帧）
    void Flush();

    // 获取宽高
    int width() const { return width_; }
    int height() const { return height_; }
    AVPixelFormat pix_fmt() const { return pix_fmt_; }

    // 获取错误信息
    const std::string& error() const { return error_; }

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* dec_ctx_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    AVPixelFormat pix_fmt_ = AV_PIX_FMT_NONE;
    std::string error_;
    bool initialized_ = false;
};

} // namespace live

#endif // DECODER_H

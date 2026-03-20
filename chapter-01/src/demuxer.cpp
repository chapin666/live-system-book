#include "demuxer.h"
#include <cstring>

namespace live {

Demuxer::Demuxer() {
    // FFmpeg 全局初始化（线程安全，可以多次调用）
    avformat_network_init();
}

Demuxer::~Demuxer() {
    Close();
}

bool Demuxer::Open(const std::string& url) {
    Close();  // 先关闭之前的

    // 打开输入文件
    // 参数1: &fmt_ctx_ - 输出参数，分配并初始化的格式上下文
    // 参数2: url.c_str() - 文件路径或URL
    // 参数3: nullptr - 自动探测格式
    // 参数4: nullptr - 额外选项
    int ret = avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "打开文件失败: " + std::string(errbuf);
        return false;
    }

    // 读取流信息（解析 moov/mdat 等）
    // 这个步骤会探测流类型、分辨率、时长等元数据
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "读取流信息失败: " + std::string(errbuf);
        Close();
        return false;
    }

    // 找到第一个视频流
    video_stream_index_ = av_find_best_stream(
        fmt_ctx_, 
        AVMEDIA_TYPE_VIDEO,  // 媒体类型：视频
        -1,                  //  wanted_stream_nb: -1 表示自动选择
        -1,                  //  related_stream: 相关的流（用于音频选择关联的视频）
        nullptr,             //  decoder_ret: 返回找到的解码器
        0                    //  flags
    );

    if (video_stream_index_ < 0) {
        error_ = "没有找到视频流";
        Close();
        return false;
    }

    // 打印一些信息
    AVStream* stream = fmt_ctx_->streams[video_stream_index_];
    printf("[Demuxer] 视频流 #%d: %dx%d, %.2f fps\n",
           video_stream_index_,
           stream->codecpar->width,
           stream->codecpar->height,
           av_q2d(stream->avg_frame_rate));

    opened_ = true;
    return true;
}

void Demuxer::Close() {
    if (fmt_ctx_) {
        avformat_close_input(&fmt_ctx_);
        fmt_ctx_ = nullptr;
    }
    video_stream_index_ = -1;
    opened_ = false;
    error_.clear();
}

bool Demuxer::ReadPacket(AVPacket* packet) {
    if (!opened_ || !fmt_ctx_) {
        error_ = "解封装器未打开";
        return false;
    }

    // 读取一个数据包
    // 内部会从文件中读取原始数据，解析成独立的 packet
    int ret = av_read_frame(fmt_ctx_, packet);
    
    if (ret < 0) {
        if (ret == AVERROR_EOF) {
            // 正常结束
            return false;
        }
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        error_ = "读取数据包失败: " + std::string(errbuf);
        return false;
    }

    return true;
}

AVStream* Demuxer::video_stream() const {
    if (!fmt_ctx_ || video_stream_index_ < 0) {
        return nullptr;
    }
    return fmt_ctx_->streams[video_stream_index_];
}

double Demuxer::duration() const {
    if (!fmt_ctx_) {
        return 0.0;
    }
    // duration 是 AV_TIME_BASE 为单位的（1/1000000 秒）
    return static_cast<double>(fmt_ctx_->duration) / AV_TIME_BASE;
}

} // namespace live

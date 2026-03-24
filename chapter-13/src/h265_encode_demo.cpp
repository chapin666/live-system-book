/**
 * Chapter 13: H.265/HEVC 编码示例
 * 使用 x265 编码器
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

class H265Encoder {
public:
    H265Encoder() = default;
    ~H265Encoder() { Cleanup(); }

    bool Initialize(int width, int height, int fps, int bitrate) {
        // 查找 H.265 编码器
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
        if (!codec) {
            fprintf(stderr, "H.265 编码器未找到\n");
            return false;
        }

        codec_ctx_ = avcodec_alloc_context3(codec);
        if (!codec_ctx_) return false;

        // 配置编码参数
        codec_ctx_>width = width;
        codec_ctx_>height = height;
        codec_ctx_>time_base = {1, fps};
        codec_ctx_>framerate = {fps, 1};
        codec_ctx_>pix_fmt = AV_PIX_FMT_YUV420P;
        codec_ctx_>bit_rate = bitrate;
        
        // H.265 特定参数
        codec_ctx_>gop_size = fps;  // 1秒一个GOP
        codec_ctx_>max_b_frames = 4;  // B帧数量

        // x265 参数设置
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "preset", "medium", 0);    // 编码速度
        av_dict_set(&opts, "tune", "zerolatency", 0); // 低延迟
        av_dict_set(&opts, "x265-params", "crf=23", 0); // 质量模式

        if (avcodec_open2(codec_ctx_, codec, &opts) < 0) {
            fprintf(stderr, "无法打开编码器\n");
            av_dict_free(&opts);
            return false;
        }
        av_dict_free(&opts);

        printf("H.265 编码器初始化成功: %dx%d @ %d fps, %d kbps\n",
               width, height, fps, bitrate / 1000);
        return true;
    }

    void Cleanup() {
        if (codec_ctx_) {
            avcodec_free_context(&codec_ctx_);
        }
    }

    // 编码一帧
    bool EncodeFrame(AVFrame* frame, AVPacket* packet) {
        int ret = avcodec_send_frame(codec_ctx_, frame);
        if (ret < 0) return false;

        ret = avcodec_receive_packet(codec_ctx_, packet);
        return ret >= 0;
    }

    AVCodecContext* GetContext() { return codec_ctx_; }

private:
    AVCodecContext* codec_ctx_ = nullptr;
};

// SVC 分层编码示例 - 模拟时间可伸缩性
void SvcEncodingDemo() {
    printf("\n=== SVC 分层编码示例 ===\n");
    
    // 基础层: 15fps
    // 增强层: 30fps (基础层 + 15fps)
    // 增强层2: 60fps (增强层 + 30fps)
    
    struct LayerConfig {
        int tid;           // 时间层ID
        int fps;           // 帧率
        int bitrate;       // 码率
        const char* name;  // 层名称
    };
    
    LayerConfig layers[] = {
        {0, 15, 500000,  "基础层 (T0)"},
        {1, 30, 800000,  "增强层1 (T0+T1)"},
        {2, 60, 1500000, "增强层2 (T0+T1+T2)"}
    };
    
    printf("分层配置:\n");
    for (const auto& layer : layers) {
        printf("  %s: %d fps, %d kbps\n", layer.name, layer.fps, layer.bitrate / 1000);
    }
    
    printf("\n网络适配策略:\n");
    printf("  良好网络 -> 接收全部层 (60fps)\n");
    printf("  一般网络 -> 只接收 T0+T1 (30fps)\n");
    printf("  差网络   -> 只接收 T0 (15fps)\n");
}

int main(int argc, char* argv[]) {
    printf("Chapter 13: H.265/HEVC 编码示例\n");
    printf("================================\n\n");

    // H.265 编码演示
    H265Encoder encoder;
    if (!encoder.Initialize(1920, 1080, 30, 4000000)) {
        return 1;
    }

    // SVC 演示
    SvcEncodingDemo();

    // 码率控制模式对比
    printf("\n=== 码率控制模式对比 ===\n");
    printf("CBR (恒定码率):   码率稳定，适合直播\n");
    printf("VBR (动态码率):   质量优先，适合点播\n");
    printf("CRF (恒定质量):   质量稳定，码率波动\n");
    printf("ABR (平均码率):   平衡方案\n");

    printf("\n建议:\n");
    printf("  直播场景: CBR 或 CRF 23\n");
    printf("  录制场景: CRF 18-20\n");
    printf("  带宽受限: CBR + 码率自适应\n");

    return 0;
}
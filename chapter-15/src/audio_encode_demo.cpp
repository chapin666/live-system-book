/**
 * Chapter 15: 音频编码示例
 * AAC vs Opus 对比
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
}

// AAC 编码器封装
class AacEncoder {
public:
    bool Initialize(int sample_rate, int channels, int bitrate) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec) {
            fprintf(stderr, "AAC 编码器未找到\n");
            return false;
        }

        ctx_ = avcodec_alloc_context3(codec);
        if (!ctx_) return false;

        ctx_>sample_rate = sample_rate;
        ctx_>ch_layout.nb_channels = channels;
        ctx_>ch_layout = channels == 1 ? AV_CHANNEL_LAYOUT_MONO : AV_CHANNEL_LAYOUT_STEREO;
        ctx_>sample_fmt = AV_SAMPLE_FMT_FLTP;
        ctx_>bit_rate = bitrate;

        // AAC-LC 低复杂度配置
        ctx_>profile = FF_PROFILE_AAC_LOW;

        if (avcodec_open2(ctx_, codec, nullptr) < 0) {
            fprintf(stderr, "无法打开 AAC 编码器\n");
            return false;
        }

        printf("AAC 编码器初始化: %d Hz, %d ch, %d kbps\n",
               sample_rate, channels, bitrate / 1000);
        return true;
    }

    void Cleanup() {
        if (ctx_) avcodec_free_context(&ctx_);
    }

private:
    AVCodecContext* ctx_ = nullptr;
};

// Opus 编码器封装
class OpusEncoder {
public:
    bool Initialize(int sample_rate, int channels, int bitrate, bool low_latency) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_OPUS);
        if (!codec) {
            fprintf(stderr, "Opus 编码器未找到\n");
            return false;
        }

        ctx_ = avcodec_alloc_context3(codec);
        if (!ctx_) return false;

        ctx_>sample_rate = sample_rate;
        ctx_>ch_layout.nb_channels = channels;
        ctx_>sample_fmt = AV_SAMPLE_FMT_FLTP;
        ctx_>bit_rate = bitrate;

        // Opus 特定参数
        AVDictionary* opts = nullptr;
        
        if (low_latency) {
            // 低延迟模式: 适合实时通信
            av_dict_set(&opts, "application", "voip", 0);
            av_dict_set(&opts, "frame_duration", "20", 0); // 20ms帧
            printf("Opus 模式: VOIP (低延迟)\n");
        } else {
            // 音乐模式: 适合音乐直播
            av_dict_set(&opts, "application", "audio", 0);
            printf("Opus 模式: AUDIO (高音质)\n");
        }

        if (avcodec_open2(ctx_, codec, &opts) < 0) {
            fprintf(stderr, "无法打开 Opus 编码器\n");
            av_dict_free(&opts);
            return false;
        }
        av_dict_free(&opts);

        printf("Opus 编码器初始化: %d Hz, %d ch, %d kbps\n",
               sample_rate, channels, bitrate / 1000);
        return true;
    }

    void Cleanup() {
        if (ctx_) avcodec_free_context(&ctx_);
    }

private:
    AVCodecContext* ctx_ = nullptr;
};

// 音频编码选型指南
void AudioCodecComparison() {
    printf("\n=== 音频编码对比 ===\n");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "编码器", "延迟", "码率范围", "适用场景", "专利");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "--------", "------", "----------", "------------", "--------");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "AAC-LC", "40ms", "64-256k", "通用直播", "有");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "AAC-HE", "60ms", "32-96k", "语音直播", "有");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "Opus", "5-60ms", "6-510k", "实时通信", "无");
    printf("%-10s %-8s %-10s %-12s %-12s\n", 
           "G.711", "0.125ms", "64k", "传统电话", "无");
    
    printf("\n=== 推荐配置 ===\n");
    printf("音乐直播:  AAC 128-256kbps 或 Opus 128kbps\n");
    printf("语音直播:  Opus 24-32kbps (语音优化)\n");
    printf("游戏直播:  Opus 64kbps (低延迟)\n");
    printf("视频会议:  Opus 16-32kbps\n");
}

int main(int argc, char* argv[]) {
    printf("Chapter 15: 音频编码示例\n");
    printf("========================\n\n");

    // AAC 示例
    printf("--- AAC 编码 ---\n");
    AacEncoder aac;
    aac.Initialize(48000, 2, 128000);
    aac.Cleanup();

    printf("\n");

    // Opus 低延迟示例
    printf("--- Opus 低延迟模式 ---\n");
    OpusEncoder opus_voip;
    opus_voip.Initialize(48000, 2, 32000, true);
    opus_voip.Cleanup();

    printf("\n");

    // Opus 音乐示例
    printf("--- Opus 音乐模式 ---\n");
    OpusEncoder opus_music;
    opus_music.Initialize(48000, 2, 128000, false);
    opus_music.Cleanup();

    // 对比
    AudioCodecComparison();

    return 0;
}
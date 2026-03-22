// timing_demo.cpp - 解码耗时测量示例
// 第四章：为什么卡顿？性能分析

#include <iostream>
#include <chrono>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// 测量单次解码耗时
void measure_decode_time(AVCodecContext* ctx, AVPacket* pkt, AVFrame* frame) {
    using namespace std::chrono;
    
    auto start = high_resolution_clock::now();
    
    avcodec_send_packet(ctx, pkt);
    avcodec_receive_frame(ctx, frame);
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start).count();
    
    std::cout << "解码耗时: " << duration << " μs" << std::endl;
}

// 统计多帧平均耗时
void measure_average_decode_time(const char* filename, int max_frames = 100) {
    AVFormatContext* fmt_ctx = nullptr;
    avformat_open_input(&fmt_ctx, filename, nullptr, nullptr);
    avformat_find_stream_info(fmt_ctx, nullptr);
    
    int video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st = fmt_ctx->streams[video_idx];
    
    const AVCodec* codec = avcodec_find_decoder(st->ccodecpar->codec_id);
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, st->ccodecpar);
    avcodec_open2(ctx, codec, nullptr);
    
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    std::vector<long long> times;
    int count = 0;
    
    while (av_read_frame(fmt_ctx, pkt) >= 0 && count < max_frames) {
        if (pkt->stream_index == video_idx) {
            using namespace std::chrono;
            auto start = high_resolution_clock::now();
            
            avcodec_send_packet(ctx, pkt);
            while (avcodec_receive_frame(ctx, frame) == 0) {
                // 解码成功
            }
            
            auto end = high_resolution_clock::now();
            times.push_back(duration_cast<microseconds>(end - start).count());
            count++;
        }
        av_packet_unref(pkt);
    }
    
    // 统计结果
    long long sum = 0, max_t = 0, min_t = 999999;
    for (auto t : times) {
        sum += t;
        max_t = std::max(max_t, t);
        min_t = std::min(min_t, t);
    }
    
    std::cout << "=== 解码耗时统计 ===" << std::endl;
    std::cout << "样本数: " << times.size() << std::endl;
    std::cout << "平均: " << sum / times.size() << " μs" << std::endl;
    std::cout << "最大: " << max_t << " μs" << std::endl;
    std::cout << "最小: " << min_t << " μs" << std::endl;
    std::cout << "帧率预算: 33ms = 33000μs" << std::endl;
    std::cout << "预算余量: " << (33000 - sum / times.size()) << " μs" << std::endl;
    
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt_ctx);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <视频文件>" << std::endl;
        return 1;
    }
    
    measure_average_decode_time(argv[1]);
    return 0;
}

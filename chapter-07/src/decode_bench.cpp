/**
 * 解码性能对比工具 - 软硬件解码性能测试
 * 
 * 编译: g++ decode_bench.cpp -o decode_bench $(pkg-config --cflags --libs libavformat libavcodec libavutil)
 * 运行: ./decode_bench <video_file> [解码方式]
 *   解码方式: sw (软件), hw (硬件), both (对比)
 */

#include <stdio.h>
#include <string>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
}

struct DecodeResult {
    int total_frames = 0;
    int decoded_frames = 0;
    double total_time_ms = 0;
    double fps = 0;
    double cpu_percent = 0;
};

DecodeResult BenchmarkSoftwareDecode(const char* filename) {
    DecodeResult result;
    
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0) {
        fprintf(stderr, "Failed to open file\n");
        return result;
    }
    
    avformat_find_stream_info(fmt_ctx, nullptr);
    
    int video_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_idx = i;
            break;
        }
    }
    
    if (video_idx < 0) {
        avformat_close_input(&fmt_ctx);
        return result;
    }
    
    AVCodecParameters* par = fmt_ctx->streams[video_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, par);
    avcodec_open2(ctx, codec, nullptr);
    
    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_idx) {
            result.total_frames++;
            avcodec_send_packet(ctx, pkt);
            
            while (avcodec_receive_frame(ctx, frame) >= 0) {
                result.decoded_frames++;
            }
        }
        av_packet_unref(pkt);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    result.fps = result.decoded_frames * 1000.0 / result.total_time_ms;
    
    printf("Software Decode:\n");
    printf("  Total frames: %d\n", result.total_frames);
    printf("  Decoded frames: %d\n", result.decoded_frames);
    printf("  Time: %.2f ms\n", result.total_time_ms);
    printf("  FPS: %.2f\n", result.fps);
    
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt_ctx);
    
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <video_file> [sw|hw|both]\n", argv[0]);
        return 1;
    }
    
    const char* mode = (argc > 2) ? argv[2] : "sw";
    
    printf("Decode Benchmark\n");
    printf("File: %s\n", argv[1]);
    printf("Mode: %s\n\n", mode);
    
    if (strcmp(mode, "sw") == 0 || strcmp(mode, "both") == 0) {
        BenchmarkSoftwareDecode(argv[1]);
    }
    
    return 0;
}

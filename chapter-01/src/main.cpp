// Live Player - Chapter 01: 本地视频播放器
// 
// 本章目标：理解"解封装→解码→渲染"完整链路
// 架构：Demuxer → Decoder → Renderer

#include <cstdio>
#include <cstring>
#include <memory>
#include <chrono>
#include <thread>

#include "demuxer.h"
#include "decoder.h"
#include "renderer.h"

extern "C" {
#include <libavutil/time.h>
}

using namespace live;

void PrintUsage(const char* program) {
    printf("Usage: %s <video_file>\n", program);
    printf("Example: %s sample.mp4\n", program);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    printf("========================================\n");
    printf("Live Player - Chapter 01\n");
    printf("File: %s\n", filename);
    printf("========================================\n\n");

    // ==================== 1. 初始化解封装器 ====================
    // 作用：从文件读取压缩数据包（H.264 NAL单元等）
    printf("[1/4] 初始化解封装器...\n");
    
    Demuxer demuxer;
    if (!demuxer.Open(filename)) {
        fprintf(stderr, "Error: %s\n", demuxer.error().c_str());
        return 1;
    }

    printf("      文件时长: %.2f 秒\n", demuxer.duration());
    printf("      视频流索引: #%d\n\n", demuxer.video_stream_index());

    // ==================== 2. 初始化解码器 ====================
    // 作用：H.264/H.265 → YUV420P
    printf("[2/4] 初始化解码器...\n");
    
    Decoder decoder;
    AVStream* video_stream = demuxer.video_stream();
    if (!decoder.Init(video_stream->codecpar)) {
        fprintf(stderr, "Error: %s\n", decoder.error().c_str());
        return 1;
    }
    printf("\n");

    // ==================== 3. 初始化渲染器 ====================
    // 作用：YUV420P → 屏幕显示
    printf("[3/4] 初始化渲染器...\n");
    
    Renderer renderer;
    if (!renderer.Init(decoder.width(), decoder.height(), "Live Player - Chapter 01")) {
        fprintf(stderr, "Error: %s\n", renderer.error().c_str());
        return 1;
    }
    printf("\n");

    // ==================== 4. 播放循环 ====================
    printf("[4/4] 开始播放 (按 ESC 退出)...\n\n");

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    int packet_count = 0;
    int frame_count = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        // ---- 4.1 读取压缩数据包 ----
        if (!demuxer.ReadPacket(packet)) {
            // 文件结束或错误
            if (demuxer.error().empty()) {
                printf("\n[Info] 文件读取完成\n");
            } else {
                fprintf(stderr, "\n[Error] %s\n", demuxer.error().c_str());
            }
            break;
        }

        // 只处理视频包
        if (packet->stream_index != demuxer.video_stream_index()) {
            av_packet_unref(packet);
            continue;
        }

        packet_count++;

        // ---- 4.2 发送给解码器 ----
        if (!decoder.SendPacket(packet)) {
            fprintf(stderr, "[Error] %s\n", decoder.error().c_str());
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        // ---- 4.3 接收解码后的帧 ----
        // 注意：一个 packet 可能解码出多个 frame（B帧重排序）
        while (decoder.ReceiveFrame(frame)) {
            frame_count++;

            // ---- 4.4 渲染 ----
            if (!renderer.RenderFrame(frame)) {
                fprintf(stderr, "[Error] %s\n", renderer.error().c_str());
            }

            // ---- 4.5 处理窗口事件 ----
            if (!renderer.PollEvents()) {
                printf("\n[Info] 用户退出\n");
                goto cleanup;  // 跳出多层循环
            }

            // ---- 4.6 简单的帧率控制 ----
            // 实际应该用 pts 做精确同步，这里简化处理
            std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30fps
        }
    }

    // ---- 刷新解码器缓冲 ----
    // 送空 packet，取出解码器内部缓冲的帧
    printf("[Info] 刷新解码器缓冲...\n");
    decoder.SendPacket(nullptr);
    while (decoder.ReceiveFrame(frame)) {
        renderer.RenderFrame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

cleanup:
    // ==================== 5. 清理 ====================
    printf("[Info] 清理资源...\n");
    
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    // 统计信息
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
    
    printf("\n========================================\n");
    printf("播放统计:\n");
    printf("  读取数据包: %d\n", packet_count);
    printf("  解码帧数: %d\n", frame_count);
    printf("  播放时长: %ld 秒\n", elapsed);
    if (elapsed > 0) {
        printf("  实际帧率: %.1f fps\n", static_cast<double>(frame_count) / elapsed);
    }
    printf("========================================\n");

    return 0;
}

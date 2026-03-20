// Live Player - Chapter 01
// Pipeline 架构与本地播放

#include "core/simple_pipeline.h"
#include <cstdio>

using namespace live;

// 简单的观察者实现（打印事件）
class PrintObserver : public PipelineObserver {
public:
    void OnError(ErrorCode code, const std::string& message) override {
        printf("[Observer] Error: %s - %s\n", ErrorCodeToString(code), message.c_str());
    }
    
    void OnFrameRendered(const VideoFrame& frame, Timestamp render_time) override {
        // 每 30 帧打印一次（避免刷屏）
        if (frame.frame_number % 30 == 0) {
            printf("[Observer] Frame %ld rendered, pts=%.2fms\n", 
                   frame.frame_number, frame.pts / 1000.0);
        }
    }
    
    void OnStatsUpdated(const PipelineStats& stats) override {
        printf("[Observer] Stats: fps=%.1f, rendered=%ld, dropped=%ld (%.1f%%)\n",
               stats.current_fps,
               stats.rendered_frames,
               stats.dropped_frames,
               stats.DropRate() * 100);
    }
    
    void OnEndOfStream() override {
        printf("[Observer] Playback finished\n");
    }
};

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
    
    // 创建 Pipeline
    SimplePipeline pipeline;
    
    // 设置观察者
    PrintObserver observer;
    pipeline.SetObserver(&observer);
    
    // 初始化
    ErrorCode ret = pipeline.Init(filename);
    if (ret != ErrorCode::OK) {
        printf("Failed to initialize: %s\n", ErrorCodeToString(ret));
        return 1;
    }
    
    printf("\nStarting playback (press ESC to exit)...\n\n");
    
    // 开始播放（阻塞直到结束）
    ret = pipeline.Start();
    if (ret != ErrorCode::OK) {
        printf("Playback error: %s\n", ErrorCodeToString(ret));
        return 1;
    }
    
    // 打印最终统计
    auto stats = pipeline.GetStats();
    printf("\n========================================\n");
    printf("Playback Statistics:\n");
    printf("  Total frames: %ld\n", stats.total_frames);
    printf("  Rendered: %ld\n", stats.rendered_frames);
    printf("  Dropped: %ld (%.2f%%)\n", 
           stats.dropped_frames, stats.DropRate() * 100);
    printf("  Average FPS: %.1f\n", stats.current_fps);
    printf("========================================\n");
    
    return 0;
}

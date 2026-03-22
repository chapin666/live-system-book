// src/main.cpp
#include "live/player_pipeline.h"
#include <cstdio>

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
    printf("Chapter 03: Pipeline Architecture\n");
    printf("File: %s\n", filename);
    printf("========================================\n\n");
    
    // 创建并初始化Pipeline
    PlayerPipeline pipeline;
    
    PipelineConfig config;
    config.url = filename;
    config.decode_threads = 4;  // 启用4线程解码
    
    ErrorCode ret = pipeline.Init(config);
    if (ret != ErrorCode::OK) {
        printf("Failed to initialize: %d\n", static_cast<int>(ret));
        return 1;
    }
    
    // 开始播放
    ret = pipeline.Run();
    if (ret != ErrorCode::OK) {
        printf("Playback error: %d\n", static_cast<int>(ret));
        return 1;
    }
    
    printf("\nTotal frames played: %ld\n", pipeline.GetPlayedFrames());
    printf("Bye!\n");
    
    return 0;
}

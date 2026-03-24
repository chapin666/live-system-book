#include "live/streamer.h"
#include <iostream>
#include <signal.h>

static volatile bool g_running = true;

void SignalHandler(int sig) {
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <rtmp_url> <stream_key>" << std::endl;
        std::cerr << "Example: " << argv[0] << " rtmp://live.example.com/live mystreamkey" << std::endl;
        return 1;
    }
    
    live::StreamerConfig config;
    strncpy(config.server_url, argv[1], sizeof(config.server_url) - 1);
    strncpy(config.stream_key, argv[2], sizeof(config.stream_key) - 1);
    
    live::Streamer streamer;
    
    streamer.on_status = [](const char* status) {
        std::cout << "[状态] " << status << std::endl;
    };
    
    streamer.on_stats = [](float bitrate, int fps) {
        std::cout << "\r[统计] 码率: " << bitrate << " kbps, FPS: " << fps << "    " << std::flush;
    };
    
    std::cout << "初始化主播端..." << std::endl;
    if (!streamer.Init(config)) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }
    
    std::cout << "开始推流到 " << config.server_url << std::endl;
    if (!streamer.StartStream()) {
        std::cerr << "开始推流失败" << std::endl;
        return 1;
    }
    
    std::cout << "推流中... 按 Ctrl+C 停止" << std::endl;
    
    while (g_running && streamer.IsStreaming()) {
        // 这里实际应该采集视频帧并发送
        // 简化示例只显示统计
        std::cout << "\r发送数据... 总字节: " << streamer.GetBytesSent() << std::flush;
        
        struct timespec ts = {0, 100000000};  // 100ms
        nanosleep(&ts, nullptr);
    }
    
    std::cout << std::endl << "停止推流..." << std::endl;
    streamer.StopStream();
    streamer.Shutdown();
    
    std::cout << "推流结束" << std::endl;
    return 0;
}
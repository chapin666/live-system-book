#include "live/network_player.h"
#include <iostream>

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program << " <video_url>" << std::endl;
    std::cerr << "Supports: HTTP, HTTPS, local file" << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << program << " test.mp4" << std::endl;
    std::cerr << "  " << program << \" \"https://example.com/video.mp4\"" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    
    live::NetworkPlayer player;
    
    // 设置回调
    player.on_buffer_progress = [](int progress) {
        std::cout << "\r缓冲进度: " << progress << "%" << std::flush;
    };
    
    player.on_network_error = [](const char* error) {
        std::cerr << "\n网络错误: " << error << std::endl;
    };
    
    // 带重试的播放
    if (!player.PlayWithRetry(url, 3)) {
        std::cerr << "无法播放: " << url << std::endl;
        return 1;
    }
    
    return 0;
}
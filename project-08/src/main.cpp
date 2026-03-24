#include "live/sfu_client.h"
#include "live/video_grid.h"
#include <iostream>
#include <string>
#include <signal.h>
#include <thread>

static volatile bool g_running = true;

void SignalHandler(int sig) {
    g_running = false;
}

void PrintUsage(const char* program) {
    std::cout << "用法: " << program << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  --server <url>    SFU服务器地址 (默认: ws://localhost:7880)" << std::endl;
    std::cout << "  --room <id>      房间ID (默认: meeting-001)" << std::endl;
    std::cout << "  --user <name>    用户名 (默认: user-随机)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program << " --room team-meeting --user Alice" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    std::string server_url = "ws://localhost:7880";
    std::string room_id = "meeting-001";
    std::string user_id = "user-" + std::to_string(rand() % 10000);
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            server_url = argv[++i];
        } else if (strcmp(argv[i], "--room") == 0 && i + 1 < argc) {
            room_id = argv[++i];
        } else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user_id = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  多人会议系统" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "SFU服务器: " << server_url << std::endl;
    std::cout << "房间: " << room_id << std::endl;
    std::cout << "用户: " << user_id << std::endl;
    std::cout << std::endl;
    
    // 初始化 SFU 客户端
    live::SfuClient sfu;
    
    sfu.SetStreamAddedCallback([](const live::RemoteStream& stream) {
        std::cout << "[会议] 新成员加入: " << stream.peer_id << std::endl;
        std::cout << "       视频: " << stream.width << "x" << stream.height << std::endl;
    });
    
    sfu.SetStreamRemovedCallback([](const std::string& peer_id) {
        std::cout << "[会议] 成员离开: " << peer_id << std::endl;
    });
    
    // 连接 SFU
    if (!sfu.Connect(server_url)) {
        std::cerr << "连接 SFU 服务器失败" << std::endl;
        return 1;
    }
    
    // 加入房间
    if (!sfu.JoinRoom(room_id, user_id)) {
        std::cerr << "加入房间失败" << std::endl;
        return 1;
    }
    
    // 开始推流
    if (!sfu.Publish(true, true)) {
        std::cerr << "推流失败" << std::endl;
        return 1;
    }
    
    // 初始化视频网格
    live::VideoGrid video_grid;
    if (!video_grid.Initialize(1280, 720)) {
        std::cerr << "初始化视频渲染失败" << std::endl;
        return 1;
    }
    
    std::cout << "进入会议，按 ESC 或关闭窗口退出" << std::endl;
    std::cout << std::endl;
    
    // 模拟添加一些远端流（实际应从 SFU 获取）
    std::cout << "模拟多人场景..." << std::endl;
    
    // 主循环
    int frame_count = 0;
    while (g_running && video_grid.PollEvents()) {
        // 模拟接收新成员
        if (frame_count == 60 && video_grid.GetSlotCount() < 3) {
            std::string peer_id = "user-" + std::to_string(rand() % 1000);
            video_grid.AddSlot(peer_id);
            sfu.Subscribe(peer_id);
        }
        
        if (frame_count == 180 && video_grid.GetSlotCount() < 5) {
            std::string peer_id = "user-" + std::to_string(rand() % 1000);
            video_grid.AddSlot(peer_id);
            sfu.Subscribe(peer_id);
        }
        
        // 渲染
        video_grid.Render();
        
        SDL_Delay(16);  // ~60fps
        frame_count++;
    }
    
    std::cout << "\n离开会议..." << std::endl;
    
    // 清理
    sfu.Unpublish();
    sfu.LeaveRoom();
    sfu.Disconnect();
    video_grid.Shutdown();
    
    std::cout << "会议结束" << std::endl;
    return 0;
}
#include "live/sfu_server.h"
#include "live/mcu_server.h"
#include "live/signaling_server.h"
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
    std::cout << "  --sfu-port <port>     SFU UDP端口 (默认: 7881)" << std::endl;
    std::cout << "  --signaling <port>    信令WebSocket端口 (默认: 7880)" << std::endl;
    std::cout << "  --mode <mode>        运行模式: sfu/mcu/all (默认: all)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program << " --mode sfu        # 仅启动SFU服务" << std::endl;
    std::cout << "  " << program << " --mode all        # 启动所有服务" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    int sfu_port = 7881;
    int signaling_port = 7880;
    std::string mode = "all";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--sfu-port") == 0 && i + 1 < argc) {
            sfu_port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--signaling") == 0 && i + 1 < argc) {
            signaling_port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  直播系统完整服务端" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "运行模式: " << mode << std::endl;
    std::cout << "SFU端口: " << sfu_port << std::endl;
    std::cout << "信令端口: " << signaling_port << std::endl;
    std::cout << std::endl;
    
    std::unique_ptr<live::SfuServer> sfu_server;
    std::unique_ptr<live::McuServer> mcu_server;
    std::unique_ptr<live::SignalingServer> signaling_server;
    
    // 启动 SFU 服务
    if (mode == "sfu" || mode == "all") {
        sfu_server = std::make_unique<live::SfuServer>();
        live::SfuConfig sfu_config;
        sfu_config.udp_port = sfu_port;
        
        if (!sfu_server->Initialize(sfu_config)) {
            std::cerr << "SFU服务器初始化失败" << std::endl;
            return 1;
        }
        if (!sfu_server->Start()) {
            std::cerr << "SFU服务器启动失败" << std::endl;
            return 1;
        }
    }
    
    // 启动 MCU 服务
    if (mode == "mcu" || mode == "all") {
        mcu_server = std::make_unique<live::McuServer>();
        live::McuConfig mcu_config;
        
        if (!mcu_server->Initialize(mcu_config)) {
            std::cerr << "MCU服务器初始化失败" << std::endl;
            return 1;
        }
        if (!mcu_server->Start()) {
            std::cerr << "MCU服务器启动失败" << std::endl;
            return 1;
        }
    }
    
    // 启动信令服务
    if (mode == "all") {
        signaling_server = std::make_unique<live::SignalingServer>();
        
        if (!signaling_server->Initialize(signaling_port)) {
            std::cerr << "信令服务器初始化失败" << std::endl;
            return 1;
        }
        
        // 设置回调
        signaling_server->SetMessageCallback(
            [&sfu_server, &mcu_server](const std::string& client_id, 
                                        const std::string& message) {
                std::cout << "[信令] 来自 " << client_id << ": " << message << std::endl;
                // 解析消息并处理
            });
        
        if (!signaling_server->Start()) {
            std::cerr << "信令服务器启动失败" << std::endl;
            return 1;
        }
    }
    
    // 创建测试房间
    if (sfu_server) {
        sfu_server->CreateRoom("test-room");
    }
    if (mcu_server) {
        mcu_server->CreateRoom("test-room");
    }
    
    std::cout << "\n服务运行中..." << std::endl;
    std::cout << "按 Ctrl+C 停止" << std::endl;
    
    // 主循环
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n停止服务..." << std::endl;
    
    if (signaling_server) signaling_server->Stop();
    if (sfu_server) sfu_server->Stop();
    if (mcu_server) mcu_server->Stop();
    
    std::cout << "服务已停止" << std::endl;
    return 0;
}
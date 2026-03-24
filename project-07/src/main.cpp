#include "live/webrtc_client.h"
#include "live/signaling_client.h"
#include <iostream>
#include <string>
#include <signal.h>

static volatile bool g_running = true;

void SignalHandler(int sig) {
    g_running = false;
}

void PrintUsage(const char* program) {
    std::cout << "用法: " << program << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  -s                    作为发起方(创建Offer)" << std::endl;
    std::cout << "  -r                    作为接收方(等待Offer)" << std::endl;
    std::cout << "  --server <url>        信令服务器地址" << std::endl;
    std::cout << "  --room <id>          房间ID" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program << " -s --server ws://localhost:8080 --room test123" << std::endl;
    std::cout << "  " << program << " -r --server ws://localhost:8080 --room test123" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    bool is_sender = false;
    bool is_receiver = false;
    std::string server_url = "ws://localhost:8080";
    std::string room_id = "test-room";
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            is_sender = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            is_receiver = true;
        } else if (strcmp(argv[i], "--server") == 0 && i + 1 < argc) {
            server_url = argv[++i];
        } else if (strcmp(argv[i], "--room") == 0 && i + 1 < argc) {
            room_id = argv[++i];
        }
    }
    
    if (!is_sender && !is_receiver) {
        std::cerr << "错误: 必须指定 -s (发起方) 或 -r (接收方)" << std::endl;
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  WebRTC 连麦客户端" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "模式: " << (is_sender ? "发起方 (Offer)" : "接收方 (Answer)") << std::endl;
    std::cout << "信令服务器: " << server_url << std::endl;
    std::cout << "房间: " << room_id << std::endl;
    std::cout << std::endl;
    
    // 创建 WebRTC 客户端
    live::MediaConfig config;
    config.audio_enabled = true;
    config.video_enabled = true;
    config.video_width = 1280;
    config.video_height = 720;
    config.video_fps = 30;
    
    live::WebrtcClient webrtc;
    if (!webrtc.Initialize(config)) {
        std::cerr << "WebRTC 初始化失败" << std::endl;
        return 1;
    }
    
    // 设置回调
    webrtc.SetStateCallback([](live::WebrtcState state) {
        const char* state_str = "未知";
        switch (state) {
            case live::WebrtcState::NEW: state_str = "新建"; break;
            case live::WebrtcState::CONNECTING: state_str = "连接中"; break;
            case live::WebrtcState::CONNECTED: state_str = "已连接"; break;
            case live::WebrtcState::DISCONNECTED: state_str = "已断开"; break;
            case live::WebrtcState::FAILED: state_str = "失败"; break;
            case live::WebrtcState::CLOSED: state_str = "已关闭"; break;
        }
        std::cout << "[WebRTC 状态] " << state_str << std::endl;
    });
    
    webrtc.SetIceCandidateCallback([](const std::string& candidate) {
        std::cout << "[ICE 候选] " <> candidate.substr(0, 60) << "..." << std::endl;
    });
    
    // 创建信令客户端
    live::SignalingClient signaling;
    
    signaling.SetOfferCallback([&webrtc](const std::string& offer) {
        std::cout << "\n[信令] 收到 Offer" << std::endl;
        std::string answer = webrtc.CreateAnswer(offer);
        std::cout << "[信令] 发送 Answer" << std::endl;
    });
    
    signaling.SetAnswerCallback([&webrtc](const std::string& answer) {
        std::cout << "\n[信令] 收到 Answer" << std::endl;
        webrtc.SetRemoteAnswer(answer);
    });
    
    signaling.SetIceCallback([](const std::string& candidate) {
        std::cout << "[信令] 收到远端 ICE 候选" << std::endl;
    });
    
    signaling.SetPeerJoinCallback([](const std::string& peer_id) {
        std::cout << "[信令] 对端加入: " << peer_id << std::endl;
    });
    
    signaling.SetPeerLeaveCallback([](const std::string& peer_id) {
        std::cout << "[信令] 对端离开: " << peer_id << std::endl;
    });
    
    // 连接信令服务器
    if (!signaling.Connect(server_url)) {
        std::cerr << "信令服务器连接失败" << std::endl;
        return 1;
    }
    
    // 加入房间
    if (!signaling.JoinRoom(room_id)) {
        std::cerr << "加入房间失败" << std::endl;
        return 1;
    }
    
    // 发起方创建 Offer
    if (is_sender) {
        std::cout << "\n创建 Offer..." << std::endl;
        std::string offer = webrtc.CreateOffer();
        
        std::cout << "\n========== SDP Offer ==========" << std::endl;
        std::cout << offer << std::endl;
        std::cout << "================================\n" << std::endl;
        
        signaling.SendOffer(offer);
        std::cout << "等待 Answer..." << std::endl;
        
        // 模拟收到 Answer
        std::cout << "\n模拟收到 Answer (实际应从信令服务器获取)" << std::endl;
        std::string answer = webrtc.CreateAnswer(offer);
        webrtc.SetRemoteAnswer(answer);
    } else {
        std::cout << "\n等待 Offer..." << std::endl;
        
        // 模拟收到 Offer
        std::cout << "模拟收到 Offer (实际应从信令服务器获取)" << std::endl;
        std::string offer = webrtc.CreateOffer();
        
        std::cout << "\n========== SDP Offer ==========" << std::endl;
        std::cout << offer << std::endl;
        std::cout << "================================\n" << std::endl;
        
        std::string answer = webrtc.CreateAnswer(offer);
        
        std::cout << "\n========== SDP Answer ==========" << std::endl;
        std::cout << answer << std::endl;
        std::cout << "=================================\n" << std::endl;
        
        signaling.SendAnswer(answer);
    }
    
    // 等待连接建立
    std::cout << "\n等待连接建立..." << std::endl;
    int retry = 0;
    while (g_running && !webrtc.IsConnected() && retry < 20) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retry++;
    }
    
    if (webrtc.IsConnected()) {
        std::cout << "\n✓ 连麦成功! 可以进行音视频通话了" << std::endl;
        std::cout << "按 Ctrl+C 结束通话" << std::endl;
        
        // 保持连接
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } else {
        std::cerr << "\n✗ 连接失败" << std::endl;
    }
    
    // 清理
    std::cout << "\n结束通话..." << std::endl;
    signaling.LeaveRoom();
    signaling.Disconnect();
    webrtc.Shutdown();
    
    return 0;
}
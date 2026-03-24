#include "live/ice_agent.h"
#include "src/rtp_packet.h"
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
    std::cout << "  -s              作为服务器(控制方)" << std::endl;
    std::cout << "  -c <候选>       添加远端候选地址" << std::endl;
    std::cout << "  --stun <地址>   STUN服务器地址 (默认: stun.l.google.com:19302)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program << " -s                          # 启动服务器" << std::endl;
    std::cout << "  " << program << " -c 192.168.1.100:5000       # 连接服务器" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    bool is_server = false;
    std::vector<live::IceCandidate> remote_candidates;
    live::SocketAddress stun_server;
    stun_server.FromString("74.125.250.129:19302");  // Google STUN
    
    // 解析参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            is_server = true;
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            live::IceCandidate candidate;
            candidate.address.FromString(argv[++i]);
            candidate.type = live::IceCandidateType::HOST;
            candidate.foundation = "1";
            candidate.priority = 2130706431;
            candidate.protocol = "udp";
            remote_candidates.push_back(candidate);
        } else if (strcmp(argv[i], "--stun") == 0 && i + 1 < argc) {
            stun_server.FromString(argv[++i]);
        }
    }
    
    std::cout << "P2P通话工具" << std::endl;
    std::cout << "模式: " << (is_server ? "服务器(控制方)" : "客户端(被控方)") << std::endl;
    
    live::IceAgent agent;
    
    // 设置回调
    agent.SetStateCallback([](live::IceState state) {
        const char* state_str = "UNKNOWN";
        switch (state) {
            case live::IceState::NEW: state_str = "NEW"; break;
            case live::IceState::GATHERING: state_str = "GATHERING"; break;
            case live::IceState::CHECKING: state_str = "CHECKING"; break;
            case live::IceState::CONNECTED: state_str = "CONNECTED"; break;
            case live::IceState::COMPLETED: state_str = "COMPLETED"; break;
            case live::IceState::FAILED: state_str = "FAILED"; break;
            case live::IceState::DISCONNECTED: state_str = "DISCONNECTED"; break;
        }
        std::cout << "[ICE状态] " << state_str << std::endl;
    });
    
    agent.SetCandidateCallback([](const live::IceCandidate& candidate) {
        std::cout << "[本地候选] " << candidate.ToSdp() << std::endl;
    });
    
    // 初始化
    live::IceRole role = is_server ? live::IceRole::CONTROLLING : live::IceRole::CONTROLLED;
    if (!agent.Initialize(role, stun_server, {})) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }
    
    // 收集候选
    std::cout << "\n开始收集候选地址..." << std::endl;
    agent.StartGathering();
    
    // 添加远端候选
    for (const auto& candidate : remote_candidates) {
        std::cout << "[添加远端候选] " << candidate.address.ToString() << std::endl;
        agent.AddRemoteCandidate(candidate);
    }
    
    // 开始连通性检测
    if (!remote_candidates.empty()) {
        std::cout << "\n开始连通性检测..." << std::endl;
        agent.StartChecking();
    } else {
        std::cout << "\n等待远端候选，请在对端运行程序并输入上述候选地址" << std::endl;
        std::cout << "按 Ctrl+C 退出" << std::endl;
        
        while (g_running) {
            sleep(1);
        }
        return 0;
    }
    
    // 等待连接建立
    int retry = 0;
    while (agent.GetState() != live::IceState::CONNECTED && retry < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        retry++;
    }
    
    if (agent.GetState() != live::IceState::CONNECTED) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }
    
    // 获取选定地址对
    live::SocketAddress local_addr, remote_addr;
    if (agent.GetSelectedPair(&local_addr, &remote_addr)) {
        std::cout << "\n连接成功!" << std::endl;
        std::cout << "本地地址: " << local_addr.ToString() << std::endl;
        std::cout << "远端地址: " << remote_addr.ToString() << std::endl;
    }
    
    std::cout << "\nP2P连接已建立，可以开始传输数据" << std::endl;
    std::cout << "按 Ctrl+C 退出" << std::endl;
    
    // 发送测试 RTP 包
    int seq_num = 0;
    while (g_running) {
        // 构建测试 RTP 包
        live::RtpPacket rtp_packet;
        rtp_packet.header.version = 2;
        rtp_packet.header.padding = 0;
        rtp_packet.header.extension = 0;
        rtp_packet.header.csrc_count = 0;
        rtp_packet.header.marker = 0;
        rtp_packet.header.payload_type = 96;  // VP8
        rtp_packet.header.sequence_number = seq_num++;
        rtp_packet.header.timestamp = seq_num * 3000;
        rtp_packet.header.ssrc = 0x12345678;
        
        const char* test_data = "Hello P2P";
        rtp_packet.payload.assign(test_data, test_data + strlen(test_data));
        
        auto packet_data = rtp_packet.Serialize();
        if (!agent.Send(packet_data.data(), packet_data.size())) {
            std::cerr << "发送失败" << std::endl;
        }
        
        // 尝试接收
        uint8_t buffer[2048];
        size_t len = sizeof(buffer);
        if (agent.Receive(buffer, &len)) {
            std::cout << "收到数据: " << len << " 字节" << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "\n关闭连接..." << std::endl;
    agent.Shutdown();
    
    return 0;
}
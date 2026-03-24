/**
 * Chapter 20: WebRTC Native 开发示例
 * PeerConnection API 使用
 */

#include <iostream>
#include <string.h>
#include <memory>
#include <functional.h>

// 模拟 WebRTC Native API (实际开发需引入 libwebrtc)
namespace webrtc {

enum class PeerConnectionState {
    NEW,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    FAILED,
    CLOSED
};

// 模拟 PeerConnection
class PeerConnection {
public:
    using StateCallback = std::function<void(PeerConnectionState)>;
    using IceCallback = std::function<void(const std::string& candidate)>;

    bool Initialize() {
        std::cout << "[PeerConnection] 初始化\n";
        return true;
    }

    std::string CreateOffer() {
        // 生成 SDP Offer
        std::string offer = 
            "v=0\r\n"
            "o=- 123456 0 IN IP4 127.0.0.1\r\n"
            "s=-\r\n"
            "t=0 0\r\n"
            "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
            "a=rtpmap:111 opus/48000/2\r\n"
            "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
            "a=rtpmap:96 VP8/90000\r\n";
        
        state_ = PeerConnectionState::CONNECTING;
        if (state_cb_) state_cb_(state_);
        
        return offer;
    }

    std::string CreateAnswer(const std::string& offer) {
        std::cout << "[PeerConnection] 创建 Answer\n";
        
        std::string answer = 
            "v=0\r\n"
            "o=- 654321 0 IN IP4 127.0.0.1\r\n"
            "s=-\r\n"
            "t=0 0\r\n"
            "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n"
            "a=rtpmap:111 opus/48000/2\r\n"
            "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
            "a=rtpmap:96 VP8/90000\r\n";
        
        return answer;
    }

    bool SetRemoteDescription(const std::string& sdp) {
        std::cout << "[PeerConnection] 设置远端描述\n";
        state_ = PeerConnectionState::CONNECTED;
        if (state_cb_) state_cb_(state_);
        return true;
    }

    void AddIceCandidate(const std::string& candidate) {
        std::cout << "[PeerConnection] 添加 ICE 候选\n";
    }

    void SetStateCallback(StateCallback cb) { state_cb_ = cb; }
    void SetIceCallback(IceCallback cb) { ice_cb_ = cb; }

    PeerConnectionState GetState() const { return state_; }

private:
    PeerConnectionState state_ = PeerConnectionState::NEW;
    StateCallback state_cb_;
    IceCallback ice_cb_;
};

} // namespace webrtc

// WebRTC 开发流程演示
void WebrtcWorkflowDemo() {
    std::cout << "\n=== WebRTC Native 开发流程 ===\n\n";

    // 1. 创建 PeerConnection
    auto pc = std::make_unique<webrtc::PeerConnection>();
    
    // 2. 设置回调
    pc->SetStateCallback([](webrtc::PeerConnectionState state) {
        const char* state_str = "Unknown";
        switch (state) {
            case webrtc::PeerConnectionState::NEW: state_str = "NEW"; break;
            case webrtc::PeerConnectionState::CONNECTING: state_str = "CONNECTING"; break;
            case webrtc::PeerConnectionState::CONNECTED: state_str = "CONNECTED"; break;
            case webrtc::PeerConnectionState::FAILED: state_str = "FAILED"; break;
            case webrtc::PeerConnectionState::CLOSED: state_str = "CLOSED"; break;
            default: break;
        }
        std::cout << "[状态变化] " << state_str << "\n";
    });

    pc->SetIceCallback([](const std::string& candidate) {
        std::cout << "[ICE] " << candidate.substr(0, 50) << "...\n";
    });

    // 3. 初始化
    pc->Initialize();

    // 4. 创建 Offer
    std::cout << "\n--- 步骤 1: 创建 Offer ---\n";
    std::string offer = pc->CreateOffer();
    std::cout << offer << "\n";

    // 5. 模拟信令交换
    std::cout << "--- 步骤 2: 信令交换 ---\n";
    std::cout << "发送 Offer 到远端...\n";
    std::cout << "接收远端 Answer...\n";

    // 6. 设置远端描述
    std::cout << "--- 步骤 3: 设置远端描述 ---\n";
    std::string answer = pc->CreateAnswer(offer);
    pc->SetRemoteDescription(answer);

    std::cout << "\n✓ 连接建立成功!\n";
}

// GN 构建系统示例
void PrintGnBuildConfig() {
    std::cout << "\n=== WebRTC GN 构建配置示例 ===\n\n";
    
    std::cout << "// BUILD.gn\n";
    std::cout << "executable(\"my_webrtc_app\") {\n";
    std::cout << "  sources = [\n";
    std::cout << "    \"main.cpp\",\n";
    std::cout << "    \"peer_connection_wrapper.cpp\",\n";
    std::cout << "  ]\n";
    std::cout << "\n";
    std::cout << "  deps = [\n";
    std::cout << "    \"//webrtc/api:libjingle_peerconnection_api\",\n";
    std::cout << "    \"//webrtc/media:webrtc_media\",\n";
    std::cout << "    \"//webrtc/pc:peerconnection\",\n";
    std::cout << "  ]\n";
    std::cout << "}\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Chapter 20: WebRTC Native 开发示例\n";
    std::cout << "===================================\n\n";

    // WebRTC 开发流程
    WebrtcWorkflowDemo();

    // GN 构建配置
    PrintGnBuildConfig();

    std::cout << "\n实际开发步骤:\n";
    std::cout << "  1. 获取 WebRTC 源码: fetch --nohooks webrtc\n";
    std::cout << "  2. 同步代码: gclient sync\n";
    std::cout << "  3. 生成构建文件: gn gen out/Debug\n";
    std::cout << "  4. 编译: ninja -C out/Debug\n";
    std::cout << "  5. 链接 libwebrtc.a 到你的项目\n";

    return 0;
}
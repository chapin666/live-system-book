#pragma once
#include <string>
#include <functional>
#include <memory>

namespace live {

// WebRTC 连接状态
enum class WebrtcState {
    NEW,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
    FAILED,
    CLOSED
};

// 媒体配置
struct MediaConfig {
    bool audio_enabled = true;
    bool video_enabled = true;
    int video_width = 1280;
    int video_height = 720;
    int video_fps = 30;
};

// WebRTC 客户端
class WebrtcClient {
public:
    WebrtcClient();
    ~WebrtcClient();
    
    // 初始化
    bool Initialize(const MediaConfig& config);
    void Shutdown();
    
    // 创建 Offer
    std::string CreateOffer();
    
    // 创建 Answer
    std::string CreateAnswer(const std::string& offer_sdp);
    
    // 设置远端 Answer
    bool SetRemoteAnswer(const std::string& answer_sdp);
    
    // 添加 ICE 候选
    void AddIceCandidate(const std::string& candidate_sdp);
    
    // 获取本地 SDP
    std::string GetLocalSdp() const;
    
    // 状态
    WebrtcState GetState() const;
    bool IsConnected() const;
    
    // 回调
    using StateCallback = std::function<void(WebrtcState)>;
    using IceCandidateCallback = std::function<void(const std::string&)>;
    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;
    
    void SetStateCallback(StateCallback cb);
    void SetIceCandidateCallback(IceCandidateCallback cb);
    void SetRemoteDataCallback(DataCallback cb);
    
    // 发送数据
    bool SendData(const uint8_t* data, size_t len);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
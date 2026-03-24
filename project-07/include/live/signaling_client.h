#pragma once
#include <string>
#include <functional>
#include <memory>

namespace live {

// 信令客户端
class SignalingClient {
public:
    SignalingClient();
    ~SignalingClient();
    
    // 连接信令服务器
    bool Connect(const std::string& server_url);
    void Disconnect();
    
    // 加入/离开房间
    bool JoinRoom(const std::string& room_id);
    bool LeaveRoom();
    
    // 发送 SDP
    bool SendOffer(const std::string& offer_sdp);
    bool SendAnswer(const std::string& answer_sdp);
    bool SendIceCandidate(const std::string& candidate_sdp);
    
    // 回调
    using OfferCallback = std::function<void(const std::string& offer)>;
    using AnswerCallback = std::function<void(const std::string& answer)>;
    using IceCallback = std::function<void(const std::string& candidate)>;
    using PeerJoinCallback = std::function<void(const std::string& peer_id)>;
    using PeerLeaveCallback = std::function<void(const std::string& peer_id)>;
    
    void SetOfferCallback(OfferCallback cb);
    void SetAnswerCallback(AnswerCallback cb);
    void SetIceCallback(IceCallback cb);
    void SetPeerJoinCallback(PeerJoinCallback cb);
    void SetPeerLeaveCallback(PeerLeaveCallback cb);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
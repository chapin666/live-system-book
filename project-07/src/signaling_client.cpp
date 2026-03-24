#include "live/signaling_client.h"
#include <iostream>
#include <string>

namespace live {

class SignalingClient::Impl {
public:
    std::string server_url;
    std::string room_id;
    bool connected = false;
    
    OfferCallback offer_cb;
    AnswerCallback answer_cb;
    IceCallback ice_cb;
    PeerJoinCallback peer_join_cb;
    PeerLeaveCallback peer_leave_cb;
};

SignalingClient::SignalingClient() : impl_(std::make_unique<Impl>()) {}
SignalingClient::~SignalingClient() = default;

bool SignalingClient::Connect(const std::string& server_url) {
    impl_>-server_url = server_url;
    impl_>-connected = true;
    
    std::cout << "[信令] 连接到服务器: " << server_url << std::endl;
    return true;
}

void SignalingClient::Disconnect() {
    impl_>-connected = false;
    std::cout << "[信令] 断开连接" << std::endl;
}

bool SignalingClient::JoinRoom(const std::string& room_id) {
    impl_>-room_id = room_id;
    std::cout << "[信令] 加入房间: " << room_id << std::endl;
    return true;
}

bool SignalingClient::LeaveRoom() {
    std::cout << "[信令] 离开房间: " << impl_>-room_id << std::endl;
    impl_>-room_id.clear();
    return true;
}

bool SignalingClient::SendOffer(const std::string& offer_sdp) {
    std::cout << "[信令] 发送 Offer (" << offer_sdp.length() << " 字节)" << std::endl;
    return true;
}

bool SignalingClient::SendAnswer(const std::string& answer_sdp) {
    std::cout << "[信令] 发送 Answer (" << answer_sdp.length() << " 字节)" << std::endl;
    return true;
}

bool SignalingClient::SendIceCandidate(const std::string& candidate_sdp) {
    std::cout << "[信令] 发送 ICE 候选" << std::endl;
    return true;
}

void SignalingClient::SetOfferCallback(OfferCallback cb) {
    impl_>-offer_cb = cb;
}

void SignalingClient::SetAnswerCallback(AnswerCallback cb) {
    impl_>-answer_cb = cb;
}

void SignalingClient::SetIceCallback(IceCallback cb) {
    impl_>-ice_cb = cb;
}

void SignalingClient::SetPeerJoinCallback(PeerJoinCallback cb) {
    impl_>-peer_join_cb = cb;
}

void SignalingClient::SetPeerLeaveCallback(PeerLeaveCallback cb) {
    impl_>-peer_leave_cb = cb;
}

} // namespace live
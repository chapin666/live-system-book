#include "live/sfu_client.h"
#include <iostream>
#include <string>

namespace live {

class SfuClient::Impl {
public:
    std::string sfu_url;
    std::string room_id;
    std::string user_id;
    bool connected = false;
    bool publishing = false;
    std::vector<RemoteStream> remote_streams;
    
    StreamAddedCallback stream_added_cb;
    StreamRemovedCallback stream_removed_cb;
    DataCallback data_cb;
};

SfuClient::SfuClient() : impl_(std::make_unique<Impl>()) {}
SfuClient::~SfuClient() = default;

bool SfuClient::Connect(const std::string& sfu_url) {
    impl_>-sfu_url = sfu_url;
    impl_>-connected = true;
    std::cout << "[SFU] 连接到服务器: " << sfu_url << std::endl;
    return true;
}

void SfuClient::Disconnect() {
    impl_>-connected = false;
    std::cout << "[SFU] 断开连接" << std::endl;
}

bool SfuClient::JoinRoom(const std::string& room_id, const std::string& user_id) {
    impl_>-room_id = room_id;
    impl_>-user_id = user_id;
    std::cout << "[SFU] 加入房间 " << room_id << " 用户: " << user_id << std::endl;
    return true;
}

bool SfuClient::LeaveRoom() {
    std::cout << "[SFU] 离开房间: " << impl_>-room_id << std::endl;
    impl_>-room_id.clear();
    return true;
}

bool SfuClient::Publish(bool audio, bool video) {
    impl_>-publishing = true;
    std::cout << "[SFU] 开始推流 音频:" << audio << " 视频:" << video << std::endl;
    return true;
}

void SfuClient::Unpublish() {
    impl_>-publishing = false;
    std::cout << "[SFU] 停止推流" << std::endl;
}

bool SfuClient::Subscribe(const std::string& peer_id) {
    std::cout << "[SFU] 订阅流: " << peer_id << std::endl;
    return true;
}

void SfuClient::Unsubscribe(const std::string& peer_id) {
    std::cout << "[SFU] 取消订阅: " << peer_id << std::endl;
}

std::vector<RemoteStream> SfuClient::GetRemoteStreams() const {
    return impl_>-remote_streams;
}

bool SfuClient::ReceiveVideoFrame(const std::string& peer_id,
                                  uint8_t* buffer, size_t* len) {
    // 简化实现
    return false;
}

bool SfuClient::ReceiveAudioFrame(const std::string& peer_id,
                                  uint8_t* buffer, size_t* len) {
    // 简化实现
    return false;
}

void SfuClient::SetStreamAddedCallback(StreamAddedCallback cb) {
    impl_>-stream_added_cb = cb;
}

void SfuClient::SetStreamRemovedCallback(StreamRemovedCallback cb) {
    impl_>-stream_removed_cb = cb;
}

void SfuClient::SetDataCallback(DataCallback cb) {
    impl_>-data_cb = cb;
}

} // namespace live
#include "live/sfu_server.h"
#include <iostream>
#include <string>

namespace live {

class SfuServer::Impl {
public:
    SfuConfig config;
    bool running = false;
    std::map<std::string, std::vector<Participant>> rooms;
    int udp_socket = -1;
    int tcp_socket = -1;
};

SfuServer::SfuServer() : impl_(std::make_unique<Impl>()) {}
SfuServer::~SfuServer() = default;

bool SfuServer::Initialize(const SfuConfig& config) {
    impl_>-config = config;
    
    std::cout << "[SFU服务器] 初始化" << std::endl;
    std::cout << "  UDP端口: " << config.udp_port << std::endl;
    std::cout << "  TCP端口: " << config.tcp_port << std::endl;
    std::cout << "  最大参与者: " << config.max_participants << std::endl;
    return true;
}

void SfuServer::Shutdown() {
    Stop();
}

bool SfuServer::Start() {
    impl_>-running = true;
    std::cout << "[SFU服务器] 启动成功" << std::endl;
    return true;
}

void SfuServer::Stop() {
    impl_>-running = false;
    std::cout << "[SFU服务器] 已停止" << std::endl;
}

bool SfuServer::CreateRoom(const std::string& room_id) {
    if (impl_>-rooms.find(room_id) != impl_>-rooms.end()) {
        return false;  // 房间已存在
    }
    impl_>-rooms[room_id] = std::vector<Participant>();
    std::cout << "[SFU服务器] 创建房间: " << room_id << std::endl;
    return true;
}

bool SfuServer::DeleteRoom(const std::string& room_id) {
    auto it = impl_>-rooms.find(room_id);
    if (it == impl_>-rooms.end()) return false;
    
    impl_>-rooms.erase(it);
    std::cout << "[SFU服务器] 删除房间: " << room_id << std::endl;
    return true;
}

bool SfuServer::AddParticipant(const std::string& room_id, const Participant& participant) {
    auto it = impl_>-rooms.find(room_id);
    if (it == impl_>-rooms.end()) return false;
    
    it->second.push_back(participant);
    std::cout << "[SFU服务器] 房间 " << room_id << " 添加参与者: " << participant.id << std::endl;
    return true;
}

void SfuServer::RemoveParticipant(const std::string& room_id, const std::string& participant_id) {
    auto it = impl_>-rooms.find(room_id);
    if (it == impl_>-rooms.end()) return;
    
    auto& participants = it->second;
    participants.erase(
        std::remove_if(participants.begin(), participants.end(),
            [&participant_id](const Participant& p) { return p.id == participant_id; }),
        participants.end()
    );
    
    std::cout << "[SFU服务器] 房间 " << room_id << " 移除参与者: " << participant_id << std::endl;
}

std::vector<Participant> SfuServer::GetParticipants(const std::string& room_id) const {
    auto it = impl_>-rooms.find(room_id);
    if (it == impl_>-rooms.end()) return {};
    return it->second;
}

bool SfuServer::Subscribe(const std::string& room_id,
                          const std::string& subscriber_id,
                          const std::string& publisher_id) {
    std::cout << "[SFU服务器] 订阅: " << subscriber_id << " -> " << publisher_id << std::endl;
    return true;
}

void SfuServer::Unsubscribe(const std::string& room_id,
                            const std::string& subscriber_id,
                            const std::string& publisher_id) {
    std::cout << "[SFU服务器] 取消订阅: " << subscriber_id << " -> " << publisher_id << std::endl;
}

} // namespace live
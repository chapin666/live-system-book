#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace live {

// SFU 服务器配置
struct SfuConfig {
    int udp_port = 7881;
    int tcp_port = 7882;
    int max_participants = 100;
    int relay_bandwidth_kbps = 2000;
};

// 参与者信息
struct Participant {
    std::string id;
    std::string name;
    uint32_t audio_ssrc;
    uint32_t video_ssrc;
    bool audio_enabled;
    bool video_enabled;
};

// SFU 服务器
class SfuServer {
public:
    SfuServer();
    ~SfuServer();
    
    // 初始化
    bool Initialize(const SfuConfig& config);
    void Shutdown();
    
    // 启动/停止
    bool Start();
    void Stop();
    
    // 房间管理
    bool CreateRoom(const std::string& room_id);
    bool DeleteRoom(const std::string& room_id);
    
    // 参与者管理
    bool AddParticipant(const std::string& room_id, const Participant& participant);
    void RemoveParticipant(const std::string& room_id, const std::string& participant_id);
    std::vector<Participant> GetParticipants(const std::string& room_id) const;
    
    // 订阅管理
    bool Subscribe(const std::string& room_id,
                   const std::string& subscriber_id,
                   const std::string& publisher_id);
    void Unsubscribe(const std::string& room_id,
                     const std::string& subscriber_id,
                     const std::string& publisher_id);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
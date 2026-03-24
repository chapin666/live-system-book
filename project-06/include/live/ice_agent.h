#pragma once
#include "live/stun_client.h"
#include "live/turn_client.h"
#include <vector>
#include <memory>
#include <functional>

namespace live {

// ICE 候选类型
enum class IceCandidateType {
    HOST,
    SRFLX,  // Server Reflexive
    PRFLX,  // Peer Reflexive
    RELAY
};

// ICE 候选
struct IceCandidate {
    std::string foundation;
    uint32_t priority;
    std::string protocol;  // "udp"
    IceCandidateType type;
    SocketAddress address;
    SocketAddress related_address;  // 用于 SRFLX/RELAY
    
    std::string ToSdp() const;
    bool FromSdp(const std::string& sdp);
};

// ICE 角色
enum class IceRole {
    CONTROLLING,
    CONTROLLED
};

// ICE 状态
enum class IceState {
    NEW,
    GATHERING,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED
};

// ICE Agent
class IceAgent {
public:
    IceAgent();
    ~IceAgent();
    
    // 初始化
    bool Initialize(IceRole role,
                    const SocketAddress& stun_server,
                    const SocketAddress& turn_server);
    void Shutdown();
    
    // 开始收集候选
    void StartGathering();
    std::vector<IceCandidate> GetLocalCandidates() const;
    
    // 添加远端候选
    void AddRemoteCandidate(const IceCandidate& candidate);
    
    // 开始连通性检测
    void StartChecking();
    
    // 获取选定地址对
    bool GetSelectedPair(SocketAddress* local, SocketAddress* remote) const;
    
    // 发送/接收数据
    bool Send(const uint8_t* data, size_t len);
    bool Receive(uint8_t* buffer, size_t* len);
    
    // 状态回调
    using StateCallback = std::function<void(IceState)>;
    using CandidateCallback = std::function<void(const IceCandidate&)>;
    void SetStateCallback(StateCallback cb);
    void SetCandidateCallback(CandidateCallback cb);
    
    IceState GetState() const { return state_; }
    
private:
    void GatherHostCandidates();
    void GatherServerReflexiveCandidates();
    void GatherRelayCandidates();
    void CheckConnectivity();
    
    IceRole role_;
    IceState state_ = IceState::NEW;
    
    SocketAddress stun_server_;
    SocketAddress turn_server_;
    
    std::vector<IceCandidate> local_candidates_;
    std::vector<IceCandidate> remote_candidates_;
    
    std::unique_ptr<StunClient> stun_client_;
    std::unique_ptr<TurnClient> turn_client_;
    
    SocketAddress selected_local_;
    SocketAddress selected_remote_;
    int sock_fd_ = -1;
    
    StateCallback state_cb_;
    CandidateCallback candidate_cb_;
};

} // namespace live
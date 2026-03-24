#include "live/ice_agent.h"
#include <iostream>
#include <string>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace live {

std::string IceCandidate::ToSdp() const {
    char buf[512];
    const char* type_str = "host";
    switch (type) {
        case IceCandidateType::SRFLX: type_str = "srflx"; break;
        case IceCandidateType::RELAY: type_str = "relay"; break;
        case IceCandidateType::PRFLX: type_str = "prflx"; break;
        default: break;
    }
    
    snprintf(buf, sizeof(buf),
             "candidate:%s 1 UDP %u %s %d typ %s",
             foundation.c_str(), priority,
             address.ToString().c_str(),
             ntohs(address.port),
             type_str);
    return std::string(buf);
}

bool IceCandidate::FromSdp(const std::string& sdp) {
    // 简化解析
    return true;
}

IceAgent::IceAgent() = default;
IceAgent::~IceAgent() { Shutdown(); }

bool IceAgent::Initialize(IceRole role,
                          const SocketAddress& stun_server,
                          const SocketAddress& turn_server) {
    role_ = role;
    stun_server_ = stun_server;
    turn_server_ = turn_server;
    
    stun_client_ = std::make_unique<StunClient>();
    if (!stun_client_>Initialize()) return false;
    
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) return false;
    
    return true;
}

void IceAgent::Shutdown() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

void IceAgent::StartGathering() {
    state_ = IceState::GATHERING;
    if (state_cb_) state_cb_(state_);
    
    GatherHostCandidates();
    GatherServerReflexiveCandidates();
    GatherRelayCandidates();
    
    state_ = IceState::NEW;
    if (state_cb_) state_cb_(state_);
}

void IceAgent::GatherHostCandidates() {
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == -1) return;
    
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        
        struct sockaddr_in* sin = (struct sockaddr_in*)ifa->ifa_addr;
        
        IceCandidate candidate;
        candidate.foundation = "1";
        candidate.priority = 2130706431;  // Host 优先级最高
        candidate.protocol = "udp";
        candidate.type = IceCandidateType::HOST;
        candidate.address.ip = sin->sin_addr.s_addr;
        candidate.address.port = htons(0);  // 未绑定具体端口
        
        local_candidates_.push_back(candidate);
        if (candidate_cb_) candidate_cb_(candidate);
    }
    
    freeifaddrs(ifaddr);
}

void IceAgent::GatherServerReflexiveCandidates() {
    SocketAddress public_addr;
    if (stun_client_>QueryPublicAddress(stun_server_, &public_addr)) {
        IceCandidate candidate;
        candidate.foundation = "2";
        candidate.priority = 1694498815;  // Srflx 优先级
        candidate.protocol = "udp";
        candidate.type = IceCandidateType::SRFLX;
        candidate.address = public_addr;
        
        local_candidates_.push_back(candidate);
        if (candidate_cb_) candidate_cb_(candidate);
    }
}

void IceAgent::GatherRelayCandidates() {
    // 简化实现：TURN 中继候选收集
}

void IceAgent::AddRemoteCandidate(const IceCandidate& candidate) {
    remote_candidates_.push_back(candidate);
}

void IceAgent::StartChecking() {
    state_ = IceState::CHECKING;
    if (state_cb_) state_cb_(state_);
    
    CheckConnectivity();
}

void IceAgent::CheckConnectivity() {
    // 简化实现：尝试与第一个远端候选连接
    if (remote_candidates_.empty()) {
        state_ = IceState::FAILED;
        if (state_cb_) state_cb_(state_);
        return;
    }
    
    // 实际应进行 STUN Binding 检测
    // 这里简化处理
    selected_local_ = local_candidates_[0].address;
    selected_remote_ = remote_candidates_[0].address;
    
    state_ = IceState::CONNECTED;
    if (state_cb_) state_cb_(state_);
}

bool IceAgent::GetSelectedPair(SocketAddress* local, SocketAddress* remote) const {
    if (state_ != IceState::CONNECTED && state_ != IceState::COMPLETED) {
        return false;
    }
    *local = selected_local_;
    *remote = selected_remote_;
    return true;
}

bool IceAgent::Send(const uint8_t* data, size_t len) {
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = selected_remote_.ip;
    addr.sin_port = selected_remote_.port;
    
    ssize_t sent = sendto(sock_fd_, data, len, 0,
                          (sockaddr*)&addr, sizeof(addr));
    return sent == (ssize_t)len;
}

bool IceAgent::Receive(uint8_t* buffer, size_t* len) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t received = recvfrom(sock_fd_, buffer, *len, 0,
                                (sockaddr*)&addr, &addr_len);
    
    if (received < 0) return false;
    
    *len = received;
    return true;
}

void IceAgent::SetStateCallback(StateCallback cb) {
    state_cb_ = cb;
}

void IceAgent::SetCandidateCallback(CandidateCallback cb) {
    candidate_cb_ = cb;
}

} // namespace live
#include "live/turn_client.h"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

namespace live {

TurnClient::TurnClient() = default;
TurnClient::~TurnClient() { Shutdown(); }

bool TurnClient::Initialize(const SocketAddress& turn_server,
                            const std::string& username,
                            const std::string& password) {
    turn_server_ = turn_server;
    username_ = username;
    password_ = password;
    
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) return false;
    
    return true;
}

void TurnClient::Shutdown() {
    running_ = false;
    if (refresh_thread_.joinable()) {
        refresh_thread_.join();
    }
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

bool TurnClient::Allocate(SocketAddress* relayed_addr,
                          SocketAddress* mapped_addr) {
    // 简化实现：实际应发送 TURN Allocate 请求
    // 这里使用 STUN Binding 作为替代演示
    StunClient stun;
    if (!stun.Initialize()) return false;
    
    SocketAddress public_addr;
    if (!stun.QueryPublicAddress(turn_server_, &public_addr)) {
        return false;
    }
    
    *relayed_addr = public_addr;
    *mapped_addr = public_addr;
    relayed_addr_ = public_addr;
    
    // 启动刷新线程
    running_ = true;
    refresh_thread_ = std::thread(&TurnClient::RefreshLoop, this);
    
    return true;
}

bool TurnClient::CreatePermission(const SocketAddress& peer_addr) {
    // 简化实现：实际应发送 CreatePermission 请求
    return true;
}

bool TurnClient::SendToPeer(const SocketAddress& peer,
                            const uint8_t* data, size_t len) {
    // 简化实现：直接通过 socket 发送
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = peer.ip;
    addr.sin_port = peer.port;
    
    ssize_t sent = sendto(sock_fd_, data, len, 0,
                          (sockaddr*)&addr, sizeof(addr));
    return sent == (ssize_t)len;
}

bool TurnClient::ReceiveFromPeer(SocketAddress* peer,
                                 uint8_t* buffer, size_t* len) {
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t received = recvfrom(sock_fd_, buffer, *len, 0,
                                (sockaddr*)&addr, &addr_len);
    
    if (received < 0) return false;
    
    peer->ip = addr.sin_addr.s_addr;
    peer->port = addr.sin_port;
    *len = received;
    
    return true;
}

void TurnClient::RefreshLoop() {
    // 每 5 分钟刷新一次
    while (running_) {
        for (int i = 0; i < 300 && running_; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        // 实际应发送 Refresh 请求
    }
}

void TurnClient::Deallocate() {
    Shutdown();
}

} // namespace live
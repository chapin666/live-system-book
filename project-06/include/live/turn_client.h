#pragma once
#include "live/stun_client.h"
#include <thread>
#include <atomic>

namespace live {

// TURN 客户端
class TurnClient {
public:
    TurnClient();
    ~TurnClient();
    
    // 初始化
    bool Initialize(const SocketAddress& turn_server,
                    const std::string& username,
                    const std::string& password);
    void Shutdown();
    
    // 分配中继地址
    bool Allocate(SocketAddress* relayed_addr,
                  SocketAddress* mapped_addr);
    
    // 创建转发许可
    bool CreatePermission(const SocketAddress& peer_addr);
    
    // 通过中继发送数据
    bool SendToPeer(const SocketAddress& peer,
                    const uint8_t* data, size_t len);
    
    // 接收数据
    bool ReceiveFromPeer(SocketAddress* peer,
                         uint8_t* buffer, size_t* len);
    
    // 释放分配
    void Deallocate();
    
private:
    void RefreshLoop();
    
    int sock_fd_ = -1;
    SocketAddress turn_server_;
    SocketAddress relayed_addr_;
    std::string username_;
    std::string password_;
    
    std::thread refresh_thread_;
    std::atomic<bool> running_{false};
    int lifetime_seconds_ = 600;
};

} // namespace live
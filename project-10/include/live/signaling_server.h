#pragma once
#include <string>
#include <functional>
#include <memory>

namespace live {

// WebSocket 信令服务器
class SignalingServer {
public:
    SignalingServer();
    ~SignalingServer();
    
    // 初始化
    bool Initialize(int ws_port);
    void Shutdown();
    
    // 启动/停止
    bool Start();
    void Stop();
    
    // 发送消息给特定客户端
    bool SendToClient(const std::string& client_id, const std::string& message);
    
    // 广播给房间内所有客户端
    bool BroadcastToRoom(const std::string& room_id, const std::string& message);
    
    // 回调
    using ConnectCallback = std::function<void(const std::string& client_id)>;
    using DisconnectCallback = std::function<void(const std::string& client_id)>;
    using MessageCallback = std::function<void(const std::string& client_id,
                                                 const std::string& message)>;
    
    void SetConnectCallback(ConnectCallback cb);
    void SetDisconnectCallback(DisconnectCallback cb);
    void SetMessageCallback(MessageCallback cb);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
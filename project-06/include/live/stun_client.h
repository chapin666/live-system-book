#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>

namespace live {

// STUN 消息类型
enum class StunMessageType : uint16_t {
    BINDING_REQUEST = 0x0001,
    BINDING_RESPONSE = 0x0101,
    BINDING_ERROR_RESPONSE = 0x0111
};

// STUN 属性类型
enum class StunAttributeType : uint16_t {
    MAPPED_ADDRESS = 0x0001,
    USERNAME = 0x0006,
    MESSAGE_INTEGRITY = 0x0008,
    ERROR_CODE = 0x0009,
    XOR_MAPPED_ADDRESS = 0x0020,
    PRIORITY = 0x0024,
    USE_CANDIDATE = 0x0025,
    FINGERPRINT = 0x8028
};

// 网络地址
struct SocketAddress {
    uint32_t ip = 0;      // IPv4 网络字节序
    uint16_t port = 0;    // 端口网络字节序
    
    std::string ToString() const;
    bool FromString(const std::string& str);
};

// STUN 客户端
class StunClient {
public:
    StunClient();
    ~StunClient();
    
    // 初始化 UDP socket
    bool Initialize(uint16_t local_port = 0);
    void Shutdown();
    
    // 查询公网地址
    bool QueryPublicAddress(const SocketAddress& stun_server,
                            SocketAddress* public_addr,
                            int timeout_ms = 3000);
    
    // 创建 Binding Request
    std::vector<uint8_t> CreateBindingRequest();
    
    // 解析 Binding Response
    bool ParseBindingResponse(const uint8_t* data, size_t len,
                              SocketAddress* mapped_addr);
    
private:
    int sock_fd_ = -1;
    static constexpr uint32_t kMagicCookie = 0x2112A442;
    
    SocketAddress DecodeXorMappedAddress(const uint8_t* data, uint16_t len);
};

} // namespace live
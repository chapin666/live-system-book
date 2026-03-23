#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace p2p {

// 常见类型定义
using ByteBuffer = std::vector<uint8_t>;

// 网络地址
struct SocketAddress {
    char ip[16];
    uint16_t port;
    
    bool operator==(const SocketAddress& other) const {
        return strcmp(ip, other.ip) == 0 && port == other.port;
    }
};

// RTP头部
struct RtpHeader {
    uint8_t version : 2;      // 2
    uint8_t padding : 1;
    uint8_t extension : 1;
    uint8_t csrc_count : 4;
    uint8_t marker : 1;
    uint8_t payload_type : 7;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t ssrc;
};

// ICE候选类型
enum class CandidateType {
    HOST,       // 本地地址
    SRFLX,      // STUN反射地址
    PRFLX,      // 对端反射地址
    RELAY       // TURN中继地址
};

struct IceCandidate {
    SocketAddress address;
    CandidateType type;
    uint32_t priority;
};

// 错误码
enum class ErrorCode {
    OK = 0,
    SOCKET_ERROR,
    BIND_ERROR,
    CONNECT_ERROR,
    STUN_ERROR,
    CODEC_ERROR,
    NETWORK_ERROR
};

} // namespace p2p

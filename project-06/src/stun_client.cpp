#include "live/stun_client.h"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace live {

std::string SocketAddress::ToString() const {
    char ip_str[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = ip;
    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
    return std::string(ip_str) + ":" + std::to_string(ntohs(port));
}

bool SocketAddress::FromString(const std::string& str) {
    size_t pos = str.find(':');
    if (pos == std::string::npos) return false;
    
    std::string ip_str = str.substr(0, pos);
    int port_num = std::stoi(str.substr(pos + 1));
    
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) != 1) {
        return false;
    }
    
    ip = addr.s_addr;
    port = htons(port_num);
    return true;
}

StunClient::StunClient() = default;
StunClient::~StunClient() { Shutdown(); }

bool StunClient::Initialize(uint16_t local_port) {
    sock_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd_ < 0) return false;
    
    if (local_port > 0) {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(local_port);
        addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(sock_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock_fd_);
            sock_fd_ = -1;
            return false;
        }
    }
    
    return true;
}

void StunClient::Shutdown() {
    if (sock_fd_ >= 0) {
        close(sock_fd_);
        sock_fd_ = -1;
    }
}

std::vector<uint8_t> StunClient::CreateBindingRequest() {
    std::vector<uint8_t> msg(20);  // 头部 20 字节
    
    // Message Type: Binding Request
    msg[0] = 0x00; msg[1] = 0x01;
    // Message Length: 0 (无属性)
    msg[2] = 0x00; msg[3] = 0x00;
    // Magic Cookie
    msg[4] = 0x21; msg[5] = 0x12; msg[6] = 0xA4; msg[7] = 0x42;
    // Transaction ID (12 bytes random)
    for (int i = 8; i < 20; i++) {
        msg[i] = rand() & 0xFF;
    }
    
    return msg;
}

bool StunClient::QueryPublicAddress(const SocketAddress& stun_server,
                                    SocketAddress* public_addr,
                                    int timeout_ms) {
    auto request = CreateBindingRequest();
    
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = stun_server.ip;
    server_addr.sin_port = stun_server.port;
    
    // 发送请求
    ssize_t sent = sendto(sock_fd_, request.data(), request.size(), 0,
                          (sockaddr*)&server_addr, sizeof(server_addr));
    if (sent < 0) return false;
    
    // 设置超时
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock_fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    // 接收响应
    uint8_t response[512];
    sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    ssize_t received = recvfrom(sock_fd_, response, sizeof(response), 0,
                                (sockaddr*)&from_addr, &from_len);
    
    if (received < 20) return false;
    
    return ParseBindingResponse(response, received, public_addr);
}

bool StunClient::ParseBindingResponse(const uint8_t* data, size_t len,
                                      SocketAddress* mapped_addr) {
    if (len < 20) return false;
    
    // 检查 Magic Cookie
    uint32_t magic = (data[4] << 24) | (data[5] << 16) | 
                     (data[6] << 8) | data[7];
    if (magic != kMagicCookie) return false;
    
    // 检查消息类型 (Binding Response = 0x0101)
    uint16_t msg_type = (data[0] << 8) | data[1];
    if (msg_type != 0x0101) return false;
    
    // 解析属性
    uint16_t attr_len = (data[2] << 8) | data[3];
    size_t pos = 20;
    
    while (pos < len) {
        if (pos + 4 > len) break;
        
        uint16_t attr_type = (data[pos] << 8) | data[pos + 1];
        uint16_t attr_val_len = (data[pos + 2] << 8) | data[pos + 3];
        pos += 4;
        
        if (attr_type == 0x0020) {  // XOR-MAPPED-ADDRESS
            *mapped_addr = DecodeXorMappedAddress(data + pos, attr_val_len);
            return true;
        }
        
        pos += attr_val_len;
        // 对齐到 4 字节边界
        if (pos % 4 != 0) pos += 4 - (pos % 4);
    }
    
    return false;
}

SocketAddress StunClient::DecodeXorMappedAddress(const uint8_t* data, uint16_t len) {
    SocketAddress addr;
    if (len < 8) return addr;
    
    // 跳过保留字节和地址族
    // data[0] = 保留, data[1] = 地址族 (1=IPv4)
    uint16_t xport = (data[2] << 8) | data[3];
    addr.port = htons(xport ^ (kMagicCookie >> 16));
    
    uint32_t xip = (data[4] << 24) | (data[5] << 16) | 
                   (data[6] << 8) | data[7];
    addr.ip = htonl(xip ^ kMagicCookie);
    
    return addr;
}

} // namespace live
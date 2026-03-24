#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cstring>
#include <functional>
#include <vector>
#include <queue>
#include <map>
#include <chrono>
#include <algorithm>

namespace live {

// ============================================
// RTP 头部结构（12字节）
// ============================================
#pragma pack(push, 1)
struct RtpHeader {
    uint8_t flags;        // V(2) + P(1) + X(1) + CC(4)
    uint8_t m_pt;         // M(1) + PT(7)
    uint16_t sequence;    // 序列号（网络字节序）
    uint32_t timestamp;   // 时间戳（网络字节序）
    uint32_t ssrc;        // 同步源（网络字节序）

    // 获取各字段
    uint8_t version() const { return (flags >> 6) & 0x03; }
    bool padding() const { return (flags >> 5) & 0x01; }
    bool extension() const { return (flags >> 4) & 0x01; }
    uint8_t csrc_count() const { return flags & 0x0F; }
    bool marker() const { return (m_pt >> 7) & 0x01; }
    uint8_t payload_type() const { return m_pt & 0x7F; }
    uint16_t seq() const { return ntohs(sequence); }
    uint32_t ts() const { return ntohl(timestamp); }
    uint32_t get_ssrc() const { return ntohl(ssrc); }

    // 设置各字段
    void set_version(uint8_t v) { flags = (flags & 0x3F) | ((v & 0x03) << 6); }
    void set_padding(bool p) { flags = p ? (flags | 0x20) : (flags & 0xDF); }
    void set_extension(bool x) { flags = x ? (flags | 0x10) : (flags & 0xEF); }
    void set_csrc_count(uint8_t cc) { flags = (flags & 0xF0) | (cc & 0x0F); }
    void set_marker(bool m) { m_pt = m ? (m_pt | 0x80) : (m_pt & 0x7F); }
    void set_payload_type(uint8_t pt) { m_pt = (m_pt & 0x80) | (pt & 0x7F); }
    void set_sequence(uint16_t seq) { sequence = htons(seq); }
    void set_timestamp(uint32_t ts) { timestamp = htonl(ts); }
    void set_ssrc(uint32_t s) { ssrc = htonl(s); }
};
static_assert(sizeof(RtpHeader) == 12, "RtpHeader must be 12 bytes");
#pragma pack(pop)

// ============================================
// RTP 包结构
// ============================================
struct RtpPacket {
    RtpHeader header;
    std::vector<uint8_t> payload;
    std::chrono::steady_clock::time_point arrival_time;

    uint16_t sequence() const { return header.seq(); }
    uint32_t timestamp() const { return header.ts(); }
    uint32_t ssrc() const { return header.get_ssrc(); }
};

// ============================================
// RTP 打包器
// ============================================
class RtpPacketizer {
public:
    RtpPacketizer(uint8_t payload_type, uint32_t ssrc, 
                  uint16_t initial_seq = 0)
        : payload_type_(payload_type)
        , ssrc_(ssrc)
        , sequence_(initial_seq) {}

    // 打包一帧数据（简单模式：不分片）
    std::vector<uint8_t> Packetize(const uint8_t* frame, size_t frame_len,
                                   uint32_t timestamp, bool marker) {
        std::vector<uint8_t> packet;
        packet.reserve(12 + frame_len);

        // 构建 RTP 头部
        RtpHeader header{};
        header.set_version(2);
        header.set_padding(false);
        header.set_extension(false);
        header.set_csrc_count(0);
        header.set_marker(marker);
        header.set_payload_type(payload_type_);
        header.set_sequence(sequence_++);
        header.set_timestamp(timestamp);
        header.set_ssrc(ssrc_);

        // 复制头部和负载
        packet.insert(packet.end(), 
                     reinterpret_cast<uint8_t*>(&header),
                     reinterpret_cast<uint8_t*>(&header) + 12);
        packet.insert(packet.end(), frame, frame + frame_len);

        return packet;
    }

private:
    uint8_t payload_type_;
    uint32_t ssrc_;
    uint16_t sequence_;
};

// ============================================
// RTP 解析器
// ============================================
class RtpParser {
public:
    bool Parse(const uint8_t* data, size_t len, RtpPacket& packet) {
        if (len < 12) return false;  // 至少要有 RTP 头部

        // 复制头部
        std::memcpy(&packet.header, data, 12);

        // 检查版本
        if (packet.header.version() != 2) return false;

        // 计算头部总长度
        size_t header_len = 12 + packet.header.csrc_count() * 4;
        
        // 检查扩展头
        if (packet.header.extension()) {
            if (len < header_len + 4) return false;
            uint16_t ext_len = (data[header_len + 2] << 8) | data[header_len + 3];
            header_len += 4 + ext_len * 4;
        }

        if (len < header_len) return false;

        // 提取负载
        size_t payload_len = len - header_len;
        packet.payload.assign(data + header_len, data + len);
        packet.arrival_time = std::chrono::steady_clock::now();

        return true;
    }
};

// ============================================
// JitterBuffer 实现
// ============================================
class JitterBuffer {
public:
    using FrameCallback = std::function<void(const RtpPacket& packet)>;

    explicit JitterBuffer(FrameCallback callback, 
                          int32_t min_delay_ms = 30,
                          int32_t max_delay_ms = 500)
        : callback_(callback)
        , min_delay_ms_(min_delay_ms)
        , max_delay_ms_(max_delay_ms)
        , target_delay_ms_(100) {}

    // 插入收到的数据包
    void InsertPacket(RtpPacket packet) {
        // 计算当前抖动
        UpdateJitterEstimate(packet);

        // 存储到缓冲区（按序列号）
        uint16_t seq = packet.sequence();
        packet_buffer_[seq] = std::move(packet);

        // 尝试输出
        TryOutput();
    }

    // 定期调用（如每 10ms）
    void Process() {
        TryOutput();
        AdaptTargetDelay();
    }

    // 获取当前统计
    struct Stats {
        int32_t jitter_ms;
        int32_t target_delay_ms;
        size_t buffer_size;
    };

    Stats GetStats() const {
        return { jitter_estimate_ms_, target_delay_ms_, packet_buffer_.size() };
    }

private:
    void UpdateJitterEstimate(const RtpPacket& packet) {
        auto now = std::chrono::steady_clock::now();

        // 计算传输时间（到达时间差 - RTP时间戳差）
        if (!last_arrival_.time_since_epoch().count()) {
            last_arrival_ = now;
            last_timestamp_ = packet.timestamp();
            return;
        }

        auto arrival_delta = std::chrono::duration_cast<
            std::chrono::milliseconds>(now - last_arrival_).count();
        
        // 假设 90kHz 时钟频率
        int32_t timestamp_delta = static_cast<int32_t>(
            packet.timestamp() - last_timestamp_) / 90;

        if (timestamp_delta > 0) {
            int32_t d = static_cast<int32_t>(arrival_delta) - timestamp_delta;
            if (d < 0) d = -d;

            // 指数加权移动平均：J = J + (|D| - J) / 16
            if (jitter_estimate_ms_ == 0) {
                jitter_estimate_ms_ = d;
            } else {
                jitter_estimate_ms_ += (d - jitter_estimate_ms_) / 16;
            }
        }

        last_arrival_ = now;
        last_timestamp_ = packet.timestamp();
    }

    void TryOutput() {
        auto now = std::chrono::steady_clock::now();

        while (!packet_buffer_.empty()) {
            auto it = packet_buffer_.begin();
            const auto& packet = it->second;

            // 计算包龄（到达后的等待时间）
            auto age_ms = std::chrono::duration_cast<
                std::chrono::milliseconds>(now - packet.arrival_time).count();

            // 如果已达到目标延迟，输出
            if (age_ms >= target_delay_ms_) {
                if (callback_) {
                    callback_(packet);
                }
                packet_buffer_.erase(it);
            } else {
                break;  // 还没到输出时间
            }
        }
    }

    void AdaptTargetDelay() {
        // 目标延迟 = 当前抖动估计 + 安全余量
        int32_t new_target = jitter_estimate_ms_ + 50;

        // 限制范围
        new_target = std::max(new_target, min_delay_ms_);
        new_target = std::min(new_target, max_delay_ms_);

        // 平滑调整（防止剧烈变化）
        target_delay_ms_ += (new_target - target_delay_ms_) / 4;
    }

    FrameCallback callback_;
    std::map<uint16_t, RtpPacket> packet_buffer_;
    
    int32_t jitter_estimate_ms_ = 0;
    int32_t target_delay_ms_;
    int32_t min_delay_ms_;
    int32_t max_delay_ms_;
    
    std::chrono::steady_clock::time_point last_arrival_;
    uint32_t last_timestamp_ = 0;
};

// ============================================
// UDP RTP 接收器
// ============================================
class UdpRtpReceiver {
public:
    using PacketCallback = std::function<void(const RtpPacket& packet)>;

    UdpRtpReceiver() : sockfd_(-1), running_(false) {}
    ~UdpRtpReceiver() { Stop(); }

    bool Start(uint16_t port, PacketCallback callback) {
        callback_ = callback;

        // 创建 UDP socket
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0) return false;

        // 设置接收缓冲区大小（重要！）
        int recv_buf_size = 512 * 1024;  // 512KB
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF, 
                   &recv_buf_size, sizeof(recv_buf_size));

        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        // 启动接收线程
        running_ = true;
        receive_thread_ = std::thread([this]() { ReceiveLoop(); });
        
        return true;
    }

    void Stop() {
        running_ = false;
        if (sockfd_ >= 0) {
            close(sockfd_);
            sockfd_ = -1;
        }
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
    }

private:
    void ReceiveLoop() {
        uint8_t buffer[2048];
        RtpParser parser;

        while (running_) {
            sockaddr_in from_addr;
            socklen_t addr_len = sizeof(from_addr);
            
            ssize_t n = recvfrom(sockfd_, buffer, sizeof(buffer), 0,
                                 (sockaddr*)&from_addr, &addr_len);
            if (n > 0) {
                RtpPacket packet;
                if (parser.Parse(buffer, n, packet)) {
                    if (callback_) {
                        callback_(packet);
                    }
                }
            }
        }
    }

    int sockfd_;
    std::atomic<bool> running_;
    std::thread receive_thread_;
    PacketCallback callback_;
};

// ============================================
// RTCP 报告结构
// ============================================
#pragma pack(push, 1)
struct RtcpHeader {
    uint8_t flags;      // V(2) + P(1) + RC(5)
    uint8_t packet_type;
    uint16_t length;    // 长度（32位字为单位）- 1

    uint8_t version() const { return (flags >> 6) & 0x03; }
    uint8_t count() const { return flags & 0x1F; }
};

struct ReceiverReportBlock {
    uint32_t ssrc;
    uint8_t fraction_lost;
    uint8_t cumulative_lost[3];
    uint32_t highest_seq;
    uint32_t jitter;
    uint32_t lsr;
    uint32_t dlsr;
};

struct RtcpReceiverReport {
    RtcpHeader header;
    uint32_t reporter_ssrc;
    // 后面跟着 count 个 ReceiverReportBlock
};
#pragma pack(pop)

// ============================================
// 丢包统计器
// ============================================
class LossStatistics {
public:
    void Update(uint16_t seq) {
        if (!initialized_) {
            base_seq_ = seq;
            max_seq_ = seq;
            received_ = 1;
            expected_ = 1;
            initialized_ = true;
            return;
        }

        // 处理序列号回绕
        if (seq < max_seq_ - 0x4000) {
            // 序列号大幅增加，可能回绕了
            expected_ += 65536 - max_seq_ + seq;
            max_seq_ = seq;
        } else if (seq > max_seq_) {
            expected_ += seq - max_seq_;
            max_seq_ = seq;
        }

        received_++;
    }

    // 计算丢包率 (0-255, 255=100%)
    uint8_t GetFractionLost() {
        if (expected_prev_ == 0) {
            expected_prev_ = expected_;
            received_prev_ = received_;
            return 0;
        }

        uint32_t expected_interval = expected_ - expected_prev_;
        uint32_t received_interval = received_ - received_prev_;

        expected_prev_ = expected_;
        received_prev_ = received_;

        if (expected_interval == 0 || received_interval >= expected_interval) {
            return 0;
        }

        uint32_t lost_interval = expected_interval - received_interval;
        return static_cast<uint8_t>((lost_interval << 8) / expected_interval);
    }

    uint32_t GetCumulativeLost() const {
        if (expected_ > received_) {
            return expected_ - received_;
        }
        return 0;
    }

    uint32_t GetHighestSeq() const { return max_seq_; }

private:
    bool initialized_ = false;
    uint16_t base_seq_ = 0;
    uint32_t max_seq_ = 0;
    uint32_t expected_ = 0;
    uint32_t received_ = 0;
    uint32_t expected_prev_ = 0;
    uint32_t received_prev_ = 0;
};

} // namespace live

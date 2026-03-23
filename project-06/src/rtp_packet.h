#pragma once

#include "common.h"
#include <string>
#include <cstdint>

namespace p2p {

class RtpPacket {
public:
    static constexpr size_t RTP_HEADER_SIZE = 12;
    static constexpr size_t MAX_PAYLOAD_SIZE = 1400;
    
    RtpHeader header;
    ByteBuffer payload;
    
    // 序列化
    ByteBuffer Serialize() const;
    
    // 解析
    bool Parse(const uint8_t* data, size_t len);
    
    // H264 FU-A分片创建
    static std::vector<RtpPacket> CreateH264Fragments(
        const uint8_t* nal_data, size_t nal_len,
        uint16_t& seq_num, uint32_t ssrc);
    
    // 计算时间戳（90kHz）
    static uint32_t CalcTimestamp90kHz(uint64_t ms);
};

} // namespace p2p

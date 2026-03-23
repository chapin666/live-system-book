#include "rtp_packet.h"
#include <string>
#include <algorithm>

namespace p2p {

ByteBuffer RtpPacket::Serialize() const {
    ByteBuffer buffer;
    buffer.reserve(RTP_HEADER_SIZE + payload.size());
    
    // 第一个字节: V(2) P(1) X(1) CC(4)
    uint8_t byte0 = (header.version << 6) | 
                    (header.padding << 5) |
                    (header.extension << 4) |
                    (header.csrc_count & 0x0F);
    buffer.push_back(byte0);
    
    // 第二个字节: M(1) PT(7)
    uint8_t byte1 = (header.marker << 7) | (header.payload_type & 0x7F);
    buffer.push_back(byte1);
    
    // 序列号 (16bit, 大端)
    buffer.push_back((header.sequence_number >> 8) & 0xFF);
    buffer.push_back(header.sequence_number & 0xFF);
    
    // 时间戳 (32bit, 大端)
    buffer.push_back((header.timestamp >> 24) & 0xFF);
    buffer.push_back((header.timestamp >> 16) & 0xFF);
    buffer.push_back((header.timestamp >> 8) & 0xFF);
    buffer.push_back(header.timestamp & 0xFF);
    
    // SSRC (32bit, 大端)
    buffer.push_back((header.ssrc >> 24) & 0xFF);
    buffer.push_back((header.ssrc >> 16) & 0xFF);
    buffer.push_back((header.ssrc >> 8) & 0xFF);
    buffer.push_back(header.ssrc & 0xFF);
    
    // 负载
    buffer.insert(buffer.end(), payload.begin(), payload.end());
    
    return buffer;
}

bool RtpPacket::Parse(const uint8_t* data, size_t len) {
    if (len < RTP_HEADER_SIZE) return false;
    
    header.version = (data[0] >> 6) & 0x03;
    if (header.version != 2) return false;
    
    header.padding = (data[0] >> 5) & 0x01;
    header.extension = (data[0] >> 4) & 0x01;
    header.csrc_count = data[0] & 0x0F;
    
    header.marker = (data[1] >> 7) & 0x01;
    header.payload_type = data[1] & 0x7F;
    
    header.sequence_number = (data[2] << 8) | data[3];
    header.timestamp = (data[4] << 24) | (data[5] << 16) | 
                       (data[6] << 8) | data[7];
    header.ssrc = (data[8] << 24) | (data[9] << 16) | 
                  (data[10] << 8) | data[11];
    
    payload.assign(data + RTP_HEADER_SIZE, data + len);
    return true;
}

uint32_t RtpPacket::CalcTimestamp90kHz(uint64_t ms) {
    return static_cast<uint32_t>((ms * 90000) / 1000);
}

} // namespace p2p

#include <live/flv_parser.h>
#include <cstring>
#include <iostream>

namespace live {

FlvParser::FlvParser() = default;
FlvParser::~FlvParser() = default;

bool FlvParser::ParseHeader(const uint8_t* data, size_t size) {
    if (size < 9) {
        return false; // 需要更多数据
    }
    
    // FLV Header: 'F' 'L' 'V' version flags headersize
    if (data[0] != 'F' || data[1] != 'L' || data[2] != 'V') {
        std::cerr << "Invalid FLV signature" << std::endl;
        return false;
    }
    
    uint8_t version = data[3];
    uint8_t flags = data[4];
    uint32_t header_size = (data[5] << 24) | (data[6] << 16) | 
                           (data[7] << 8) | data[8];
    
    has_video_ = (flags & 0x01) != 0;
    has_audio_ = (flags & 0x04) != 0;
    
    std::cout << "FLV Header: version=" << (int)version 
              << ", has_video=" << has_video_
              << ", has_audio=" << has_audio_
              << ", header_size=" << header_size << std::endl;
    
    header_parsed_ = true;
    return true;
}

size_t FlvParser::ParseTag(const uint8_t* data, size_t size) {
    if (!header_parsed_) {
        if (!ParseHeader(data, size)) {
            return 0;
        }
        // 返回FLV头部消耗的字节（通常是9）
        return 9;
    }
    
    // 需要：PrevTagSize(4) + TagType(1) + DataSize(3) + Timestamp(4) + StreamID(3) = 15 bytes
    if (size < 15) {
        return 0;
    }
    
    // 跳过PrevTagSize（第一个Tag之前也有4字节）
    size_t offset = 0;
    if (size >= 4) {
        offset = 4; // 跳过PrevTagSize
    }
    
    // 解析Tag Header
    uint8_t tag_type = data[offset];
    uint32_t data_size = (data[offset + 1] << 16) | 
                         (data[offset + 2] << 8) | 
                         data[offset + 3];
    uint32_t timestamp = (data[offset + 4] << 16) | 
                         (data[offset + 5] << 8) | 
                         data[offset + 6] |
                         (data[offset + 7] << 24); // 扩展时间戳
    uint32_t stream_id = (data[offset + 8] << 16) | 
                         (data[offset + 9] << 8) | 
                         data[offset + 10];
    
    // 检查是否有足够的数据
    if (size < offset + 11 + data_size + 4) {
        return 0; // 需要更多数据
    }
    
    const uint8_t* tag_data = data + offset + 11;
    
    // 解析具体Tag
    if (tag_type == 9) { // Video
        ParseVideoTag(tag_data, data_size, timestamp);
    } else if (tag_type == 8) { // Audio
        ParseAudioTag(tag_data, data_size, timestamp);
    } else if (tag_type == 18) { // Script
        // 跳过脚本数据
    }
    
    // 返回消耗的总字节数
    return offset + 11 + data_size + 4; // +4 for PrevTagSize
}

bool FlvParser::ParseVideoTag(const uint8_t* data, size_t size, uint32_t timestamp) {
    if (size < 5) return false;
    
    uint8_t flags = data[0];
    VideoFrameType frame_type = static_cast<VideoFrameType>((flags >> 4) & 0x0F);
    VideoCodecId codec_id = static_cast<VideoCodecId>(flags & 0x0F);
    
    if (codec_id != VideoCodecId::H264) {
        std::cerr << "Unsupported video codec: " << (int)codec_id << std::endl;
        return false;
    }
    
    uint8_t avc_packet_type = data[1];
    uint32_t composition_time = (data[2] << 16) | (data[3] << 8) | data[4];
    
    current_video_tag_.frame_type = frame_type;
    current_video_tag_.codec_id = codec_id;
    current_video_tag_.is_avc_header = (avc_packet_type == 0);
    current_video_tag_.composition_time = composition_time;
    current_video_tag_.data.assign(data + 5, data + size);
    
    has_video_tag_ = true;
    
    return true;
}

bool FlvParser::ParseAudioTag(const uint8_t* data, size_t size, uint32_t timestamp) {
    if (size < 2) return false;
    
    uint8_t flags = data[0];
    uint8_t sound_format = (flags >> 4) & 0x0F;
    uint8_t sound_rate = (flags >> 2) & 0x03;
    uint8_t sound_size = (flags >> 1) & 0x01;
    uint8_t sound_type = flags & 0x01;
    
    if (sound_format != 10) { // AAC
        std::cerr << "Unsupported audio format: " << (int)sound_format << std::endl;
        return false;
    }
    
    uint8_t aac_packet_type = data[1];
    
    current_audio_tag_.sound_format = sound_format;
    current_audio_tag_.sound_rate = sound_rate;
    current_audio_tag_.sound_size = sound_size;
    current_audio_tag_.sound_type = sound_type;
    current_audio_tag_.is_aac_header = (aac_packet_type == 0);
    current_audio_tag_.data.assign(data + 2, data + size);
    
    has_audio_tag_ = true;
    
    return true;
}

bool FlvParser::GetVideoTag(FlvVideoTag* tag) {
    if (!has_video_tag_) return false;
    *tag = current_video_tag_;
    has_video_tag_ = false;
    return true;
}

bool FlvParser::GetAudioTag(FlvAudioTag* tag) {
    if (!has_audio_tag_) return false;
    *tag = current_audio_tag_;
    has_audio_tag_ = false;
    return true;
}

} // namespace live

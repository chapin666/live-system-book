#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

// FLV Tag类型
enum class FlvTagType : uint8_t {
    Audio = 8,
    Video = 9,
    Script = 18
};

// 视频帧类型
enum class VideoFrameType : uint8_t {
    KeyFrame = 1,
    InterFrame = 2
};

// 视频CodecID
enum class VideoCodecId : uint8_t {
    H264 = 7
};

// FLV Video Tag数据结构
struct FlvVideoTag {
    VideoFrameType frame_type;
    VideoCodecId codec_id;
    bool is_avc_header;      // AVC sequence header
    uint32_t composition_time;
    std::vector<uint8_t> data;
};

// FLV Audio Tag数据结构
struct FlvAudioTag {
    uint8_t sound_format;
    uint8_t sound_rate;
    uint8_t sound_size;
    uint8_t sound_type;
    bool is_aac_header;      // AAC sequence header
    std::vector<uint8_t> data;
};

// FLV解析器
class FlvParser {
public:
    FlvParser();
    ~FlvParser();

    // 禁止拷贝
    FlvParser(const FlvParser&) = delete;
    FlvParser& operator=(const FlvParser&) = delete;

    // 解析FLV头部
    bool ParseHeader(const uint8_t* data, size_t size);

    // 解析一个Tag，返回消耗的bytes（0表示需要更多数据）
    size_t ParseTag(const uint8_t* data, size_t size);

    // 获取解析出的视频Tag（调用后内部清空）
    bool GetVideoTag(FlvVideoTag* tag);

    // 获取解析出的音频Tag（调用后内部清空）
    bool GetAudioTag(FlvAudioTag* tag);

    // 检查是否有待处理的Tag
    bool HasVideoTag() const { return has_video_tag_; }
    bool HasAudioTag() const { return has_audio_tag_; }

    // 获取FLV元数据信息
    bool HasVideo() const { return has_video_; }
    bool HasAudio() const { return has_audio_; }

private:
    bool ParseVideoTag(const uint8_t* data, size_t size, uint32_t timestamp);
    bool ParseAudioTag(const uint8_t* data, size_t size, uint32_t timestamp);

    bool header_parsed_ = false;
    bool has_video_ = false;
    bool has_audio_ = false;

    bool has_video_tag_ = false;
    bool has_audio_tag_ = false;
    FlvVideoTag current_video_tag_;
    FlvAudioTag current_audio_tag_;
};

} // namespace live

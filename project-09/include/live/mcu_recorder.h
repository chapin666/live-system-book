#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <stdint.h>

namespace live {

// 录制配置
struct RecordConfig {
    std::string output_dir;
    std::string filename_prefix;
    int segment_duration_sec = 10;  // 切片时长
    int max_files = 100;            // 最大保留文件数
    bool record_audio = true;
    bool record_video = true;
};

// MCU 录制器
class McuRecorder {
public:
    McuRecorder();
    ~McuRecorder();
    
    // 初始化
    bool Initialize(const RecordConfig& config);
    void Shutdown();
    
    // 开始/停止录制
    bool StartRecording();
    void StopRecording();
    
    // 添加输入流
    bool AddInputStream(const std::string& stream_id, 
                        int width, int height,
                        bool has_audio);
    void RemoveInputStream(const std::string& stream_id);
    
    // 写入视频帧
    bool WriteVideoFrame(const std::string& stream_id,
                         const uint8_t* data, size_t len,
                         int64_t timestamp);
    
    // 写入音频帧
    bool WriteAudioFrame(const std::string& stream_id,
                         const uint8_t* data, size_t len,
                         int64_t timestamp);
    
    // 获取录制统计
    int64_t GetBytesWritten() const;
    int64_t GetDurationMs() const;
    int GetSegmentCount() const;
    
    // 回调
    using SegmentCallback = std::function<void(const std::string& segment_path)>;
    void SetSegmentCallback(SegmentCallback cb);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
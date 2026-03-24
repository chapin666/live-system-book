#pragma once
#include <string>
#include <vector>
#include <map>

namespace live {

// HLS 片段
struct HlsSegment {
    std::string filename;
    double duration;
    int64_t timestamp;
};

// HLS 生成器
class HlsGenerator {
public:
    HlsGenerator();
    ~HlsGenerator();
    
    // 初始化
    bool Initialize(const std::string& output_dir,
                    int target_duration,
                    int playlist_length);
    void Shutdown();
    
    // 添加片段
    bool AddSegment(const std::string& segment_path, double duration);
    
    // 生成 m3u8 播放列表
    bool GeneratePlaylist();
    
    // 获取播放列表路径
    std::string GetPlaylistPath() const;
    
    // 获取时移播放列表（从指定时间点开始）
    std::string GetTimeShiftPlaylist(int64_t start_time_ms) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
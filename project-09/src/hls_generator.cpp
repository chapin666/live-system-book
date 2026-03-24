#include "live/hls_generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace live {

class HlsGenerator::Impl {
public:
    std::string output_dir;
    int target_duration = 10;
    int playlist_length = 6;
    std::vector<HlsSegment> segments;
    std::string playlist_path;
};

HlsGenerator::HlsGenerator() : impl_(std::make_unique<Impl>()) {}
HlsGenerator::~HlsGenerator() = default;

bool HlsGenerator::Initialize(const std::string& output_dir,
                              int target_duration,
                              int playlist_length) {
    impl_>-output_dir = output_dir;
    impl_>-target_duration = target_duration;
    impl_>-playlist_length = playlist_length;
    impl_>-playlist_path = output_dir + "/playlist.m3u8";
    
    std::cout << "[HLS] 初始化生成器" << std::endl;
    std::cout << "  输出目录: " << output_dir << std::endl;
    std::cout << "  目标时长: " << target_duration << "秒" << std::endl;
    std::cout << "  列表长度: " << playlist_length << "个片段" << std::endl;
    return true;
}

void HlsGenerator::Shutdown() {
    std::cout << "[HLS] 关闭生成器" << std::endl;
}

bool HlsGenerator::AddSegment(const std::string& segment_path, double duration) {
    HlsSegment seg;
    seg.filename = segment_path;
    seg.duration = duration;
    seg.timestamp = time(nullptr);
    
    impl_>-segments.push_back(seg);
    
    // 限制列表长度
    while ((int)impl_>-segments.size() > impl_>-playlist_length) {
        impl_>-segments.erase(impl_>-segments.begin());
    }
    
    return GeneratePlaylist();
}

bool HlsGenerator::GeneratePlaylist() {
    std::ofstream file(impl_>-playlist_path);
    if (!file.is_open()) return false;
    
    file << "#EXTM3U\n";
    file << "#EXT-X-VERSION:3\n";
    file << "#EXT-X-TARGETDURATION:" << impl_>-target_duration << "\n";
    file << "#EXT-X-MEDIA-SEQUENCE:" << (impl_>-segments.empty() ? 0 : 
                                        impl_>-segments[0].timestamp) << "\n";
    
    for (const auto& seg : impl_>-segments) {
        file << "#EXTINF:" << std::fixed << std::setprecision(3) 
              << seg.duration << ",\n";
        file << seg.filename << "\n";
    }
    
    file.close();
    return true;
}

std::string HlsGenerator::GetPlaylistPath() const {
    return impl_>-playlist_path;
}

std::string HlsGenerator::GetTimeShiftPlaylist(int64_t start_time_ms) const {
    // 生成时移播放列表
    std::stringstream ss;
    ss << "#EXTM3U\n";
    ss << "#EXT-X-VERSION:3\n";
    ss << "#EXT-X-TARGETDURATION:" << impl_>-target_duration << "\n";
    ss << "#EXT-X-PLAYLIST-TYPE:EVENT\n";
    
    for (const auto& seg : impl_>-segments) {
        if (seg.timestamp * 1000 >= start_time_ms) {
            ss << "#EXTINF:" << std::fixed << std::setprecision(3) 
               << seg.duration << ",\n";
            ss << seg.filename << "\n";
        }
    }
    
    return ss.str();
}

} // namespace live
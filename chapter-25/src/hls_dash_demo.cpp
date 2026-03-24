/**
 * Chapter 24: HLS/DASH 协议示例
 * 直播协议实现
 */

#include <iostream>
#include <fstream>
#include <sstream.h>
#include <iomanip.h>
#include <string.h>
#include <vector.h>

// HLS M3U8 播放列表生成器
class HlsPlaylistGenerator {
public:
    struct Segment {
        std::string filename;
        double duration;
        int64_t timestamp;
    };

    bool Generate(const std::string& output_path,
                  const std::vector<Segment>& segments,
                  int target_duration,
                  bool is_live) {
        std::ofstream file(output_path);
        if (!file.is_open()) return false;

        // M3U8 头部
        file << "#EXTM3U\n";
        file << "#EXT-X-VERSION:3\n";
        file << "#EXT-X-TARGETDURATION:" << target_duration << "\n";
        
        if (is_live) {
            file << "#EXT-X-MEDIA-SEQUENCE:" << (segments.empty() ? 0 : segments[0].timestamp) << "\n";
        } else {
            file << "#EXT-X-PLAYLIST-TYPE:VOD\n";
        }

        // 片段列表
        for (const auto& seg : segments) {
            file << "#EXTINF:" << std::fixed << std::setprecision(3) 
                  << seg.duration << ",\n";
            file << seg.filename << "\n";
        }

        // 直播模式不结束，VOD结束
        if (!is_live) {
            file << "#EXT-X-ENDLIST\n";
        }

        file.close();
        return true;
    }
};

// DASH MPD 生成器
class DashManifestGenerator {
public:
    struct Representation {
        int id;
        int width;
        int height;
        int bitrate;
        std::string codecs;
        std::string segment_template;
    };

    bool Generate(const std::string& output_path,
                  const std::vector<Representation>& reps,
                  int min_buffer_time,
                  int min_update_period) {
        std::ofstream file(output_path);
        if (!file.is_open()) return false;

        file << "<?xml version=\"1.0\"?\n";
        file <> "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n";
        file << "     type=\"dynamic\"\n";
        file << "     minimumUpdatePeriod=\"PT" << min_update_period << "S\"\n";
        file << "     minBufferTime=\"PT" << min_buffer_time << "S\"\n";
        file << "     profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n";
        file << "     availabilityStartTime=\"2024-01-01T00:00:00Z\"\n";
        file << "     publishTime=\"2024-01-01T00:00:00Z\"\n";
        file << "     timeShiftBufferDepth=\"PT5M\"\n";
        file << "     maxSegmentDuration=\"PT10S\"\n";
        file << "     \u003e\n\n";

        file << "  <Period id=\"0\" start=\"PT0S\"\u003e\n";
        file << "    <AdaptationSet\n";
        file << "        id=\"0\"\n";
        file << "        contentType=\"video\"\n";
        file << "        segmentAlignment=\"true\"\n";
        file << "        startWithSAP=\"1\"\n";
        file << "        maxWidth=\"1920\"\n";
        file << "        maxHeight=\"1080\"\n";
        file << "        par=\"16:9\"\n";
        file << "        \u003e\n\n";

        for (const auto& rep : reps) {
            file << "      <Representation\n";
            file << "          id=\"\" << rep.id << "\"\n";
            file << "          width=\"\" << rep.width << "\"\n";
            file << "          height=\"" << rep.height << "\"\n";
            file << "          bandwidth=\"" << rep.bitrate << "\"\n";
            file << "          codecs=\"" << rep.codecs << "\"\n";
            file << "          mimeType=\"video/mp4\"\n";
            file << "          \u003e\n";
            file << "        <SegmentTemplate\n";
            file << "            timescale=\"1000\"\n";
            file << "            media=\"" << rep.segment_template << "\"\n";
            file << "            initialization=\"init_" << rep.id << ".mp4\"\n";
            file << "            duration=\"10000\"\n";
            file << "            startNumber=\"0\"\n";
            file << "            \u003e\n";
            file << "        </SegmentTemplate\n";
            file << "      </Representation\n\n";
        }

        file << "    </AdaptationSet\n";
        file << "  </Period\n";
        file << "</MPD\n";

        file.close();
        return true;
    }
};

// 协议对比
void PrintProtocolComparison() {
    std::cout << "\n=== 直播协议对比 ===\n\n";
    std::cout << "| 协议    | 延迟   | 兼容性 | 自适应码率 | 适用场景          |\n";
    std::cout << "|---------|--------|--------|------------|-------------------|\n";
    std::cout << "| HLS     | 5-30s  | ★★★★★ | ✓          | 移动端、网页播放  |\n";
    std::cout << "| DASH    | 5-30s  | ★★★☆☆ | ✓          | 现代浏览器        |\n";
    std::cout << "| LL-HLS  | 2-5s   | ★★★★☆ | ✓          | 低延迟直播        |\n";
    std::cout << "| LL-DASH | 2-5s   | ★★★☆☆ | ✓          | 低延迟直播        |\n";
    std::cout << "| WebRTC  | <500ms | ★★★☆☆ | ✗          | 实时互动          |\n";
    std::cout << "| RTMP    | 1-3s   | ★★★★☆ | ✗          | 推流、老旧系统    |\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Chapter 24: 直播协议示例\n";
    std::cout << "========================\n\n";

    // HLS 生成示例
    std::cout << "--- HLS 播放列表生成 ---\n";
    HlsPlaylistGenerator hls_gen;
    std::vector<HlsPlaylistGenerator::Segment> segments = {
        {"segment_0.ts", 10.0, 0},
        {"segment_1.ts", 10.0, 1},
        {"segment_2.ts", 10.0, 2},
        {"segment_3.ts", 10.0, 3},
    };
    
    if (hls_gen.Generate("/tmp/playlist.m3u8", segments, 10, true)) {
        std::cout << "✓ HLS 播放列表生成成功\n";
    }

    // DASH 生成示例
    std::cout << "\n--- DASH Manifest 生成 ---\n";
    DashManifestGenerator dash_gen;
    std::vector<DashManifestGenerator::Representation> reps = {
        {1, 640, 360, 800000, "avc1.42e01e", "video_360p_$Number$.m4s"},
        {2, 1280, 720, 2000000, "avc1.4d401f", "video_720p_$Number$.m4s"},
        {3, 1920, 1080, 4000000, "avc1.640028", "video_1080p_$Number$.m4s"},
    };
    
    if (dash_gen.Generate("/tmp/manifest.mpd", reps, 2, 6)) {
        std::cout << "✓ DASH Manifest 生成成功\n";
    }

    // 协议对比
    PrintProtocolComparison();

    std::cout << "\n实现要点:\n";
    std::cout << "  HLS: TS 切片 + M3U8 索引，简单兼容性好\n";
    std::cout << "  DASH: fMP4 切片 + MPD 描述，功能更强大\n";
    std::cout << "  低延迟: 使用部分段(Partial Segment)或 CMAF-CTE\n";

    return 0;
}
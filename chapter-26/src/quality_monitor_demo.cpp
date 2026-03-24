/**
 * Chapter 26: 监控与质量分析示例
 * QoS 指标采集与展示
 */

#include <iostream>
#include <vector.h>
#include <string.h>
#include <math.h>

// 视频质量指标
struct VideoQualityMetrics {
    int64_t timestamp_ms;
    int bitrate_kbps;
    int fps;
    int width;
    int height;
    int frame_drop_count;
    int decode_time_ms;
};

// 网络质量指标
struct NetworkQualityMetrics {
    int64_t timestamp_ms;
    int rtt_ms;
    float packet_loss_rate;  // 0.0 - 1.0
    int jitter_ms;
    int bandwidth_estimate_kbps;
};

// 质量监控器
class QualityMonitor {
public:
    void RecordVideoMetrics(const VideoQualityMetrics& metrics) {
        video_history_.push_back(metrics);
        if (video_history_.size() > max_history_size_) {
            video_history_.erase(video_history_.begin());
        }
    }

    void RecordNetworkMetrics(const NetworkQualityMetrics& metrics) {
        network_history_.push_back(metrics);
        if (network_history_.size() > max_history_size_) {
            network_history_.erase(network_history_.begin());
        }
    }

    // 计算平均码率
    double GetAverageBitrate() const {
        if (video_history_.empty()) return 0;
        double sum = 0;
        for (const auto& m : video_history_) {
            sum += m.bitrate_kbps;
        }
        return sum / video_history_.size();
    }

    // 计算平均帧率
    double GetAverageFps() const {
        if (video_history_.empty()) return 0;
        double sum = 0;
        for (const auto& m : video_history_) {
            sum += m.fps;
        }
        return sum / video_history_.size();
    }

    // 计算卡顿率
    double GetStallRate() const {
        if (video_history_.size() < 2) return 0;
        int stall_count = 0;
        for (size_t i = 1; i < video_history_.size(); i++) {
            if (video_history_[i].fps < video_history_[i-1].fps * 0.5) {
                stall_count++;
            }
        }
        return (double)stall_count / (video_history_.size() - 1);
    }

    // MOS 评分估算 (1-5分)
    double EstimateMosScore() const {
        if (network_history_.empty()) return 0;
        
        const auto& latest = network_history_.back();
        
        // 基于 R 值的简化 MOS 计算
        // R = 93.2 - 0.024 * delay - 0.11 * (delay - 177.3) * H(delay - 177.3) - 11.14 * loss - 40 * ln(1 + 10 * loss)
        double delay = latest.rtt_ms;
        double loss = latest.packet_loss_rate;
        
        double r = 93.2 - 0.024 * delay;
        if (delay > 177.3) {
            r -= 0.11 * (delay - 177.3);
        }
        r -= 11.14 * loss * 100;
        
        // R 转 MOS
        double mos;
        if (r < 0) mos = 1;
        else if (r > 100) mos = 4.5;
        else {
            mos = 1 + 0.035 * r + r * (r - 60) * (100 - r) * 7e-6;
        }
        
        return std::max(1.0, std::min(5.0, mos));
    }

    void PrintReport() const {
        std::cout << "\n=== 质量监控报告 ===\n";
        std::cout << "视频统计:\n";
        std::cout << "  平均码率: " << GetAverageBitrate() << " kbps\n";
        std::cout << "  平均帧率: " << GetAverageFps() << " fps\n";
        std::cout << "  卡顿率:   " << (GetStallRate() * 100) << "%\n";
        
        std::cout << "\n网络统计:\n";
        if (!network_history_.empty()) {
            const auto& latest = network_history_.back();
            std::cout << "  RTT:      " << latest.rtt_ms << " ms\n";
            std::cout << "  丢包率:   " << (latest.packet_loss_rate * 100) << "%\n";
            std::cout << "  抖动:     " << latest.jitter_ms << " ms\n";
            std::cout << "  带宽估计: " << latest.bandwidth_estimate_kbps << " kbps\n";
        }
        
        std::cout << "\n体验评分:\n";
        double mos = EstimateMosScore();
        std::cout << "  MOS 评分: " << std::fixed << std::setprecision(2) << mos << "/5.0\n";
        
        if (mos >= 4.0) std::cout << "  评级: 优秀 ✓\n";
        else if (mos >= 3.5) std::cout << "  评级: 良好\n";
        else if (mos >= 3.0) std::cout << "  评级: 一般\n";
        else std::cout << "  评级: 较差 ✗\n";
    }

private:
    std::vector<VideoQualityMetrics> video_history_;
    std::vector<NetworkQualityMetrics> network_history_;
    static constexpr size_t max_history_size_ = 300;  // 5分钟 @ 1fps
};

// 模拟质量数据采集
void SimulateQualityData(QualityMonitor& monitor) {
    std::cout << "模拟采集 60 秒数据...\n";
    
    for (int i = 0; i < 60; i++) {
        // 视频数据 (模拟波动)
        VideoQualityMetrics vq = {
            i * 1000LL,
            2000 + (rand() % 500 - 250),  // 2000 ± 250 kbps
            30 - (i > 40 ? 5 : 0),         // 40秒后降帧率
            1280, 720,
            (i > 45) ? 1 : 0,              // 45秒后开始丢帧
            8 + rand() % 4
        };
        monitor.RecordVideoMetrics(vq);
        
        // 网络数据
        NetworkQualityMetrics nq = {
            i * 1000LL,
            50 + (i > 30 ? 30 : 0),        // 30秒后延迟增加
            (i > 35) ? 0.02f : 0.0f,       // 35秒后开始丢包
            5 + rand() % 5,
            3000 - (i > 30 ? 500 : 0)
        };
        monitor.RecordNetworkMetrics(nq);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "Chapter 26: 监控与质量分析示例\n";
    std::cout << "===============================\n\n";

    QualityMonitor monitor;
    
    // 模拟数据采集
    SimulateQualityData(monitor);
    
    // 输出报告
    monitor.PrintReport();
    
    std::cout << "\n关键指标说明:\n";
    std::cout << "  FPS: 帧率，低于 15 有明显卡顿感\n";
    std::cout << "  RTT: 往返时延，< 200ms 为良好\n";
    std::cout << "  丢包率: < 1% 为可接受\n";
    std::cout << "  MOS: 主观评分，> 3.5 为良好体验\n";

    return 0;
}
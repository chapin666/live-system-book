# 第二十六章：质量监控体系

> **本章目标**：建立完整的音视频质量监控体系，掌握关键指标的采集、分析和可视化方法。

在生产环境中，监控是保障服务质量的关键。实时音视频系统涉及多个环节（采集、编码、传输、解码、渲染），任何环节出现问题都会影响用户体验。本章将详细介绍如何构建完整的质量监控体系。

**本章你将学习**：
- 监控指标体系的设计（音视频质量、网络、设备）
- 端到端延迟测量方法
- 音视频质量评估标准（MOS、VMAF）
- 实时监控Dashboard设计
- 告警与自动化处理

**学习本章后，你将能够**：
- 设计全面的监控指标体系
- 实现端到端延迟测量
- 搭建质量监控Dashboard
- 配置有效告警策略
- 基于监控数据优化系统

---

## 目录

1. [监控指标体系](#1-监控指标体系)
2. [端到端延迟测量](#2-端到端延迟测量)
3. [音视频质量评估](#3-音视频质量评估)
4. [实时监控Dashboard](#4-实时监控dashboard)
5. [告警与自动化](#5-告警与自动化)
6. [本章总结](#6-本章总结)

---

## 1. 监控指标体系

### 1.1 监控指标分类

![监控指标体系](./diagrams/monitoring-metrics.svg)

**监控指标三大维度**：

| 维度 | 指标示例 | 重要性 |
|:---|:---|:---|
| **音视频质量** | 码率、帧率、分辨率、卡顿 | ⭐⭐⭐⭐⭐ |
| **网络质量** | 延迟、丢包、抖动、带宽 | ⭐⭐⭐⭐⭐ |
| **设备状态** | CPU、内存、温度、电量 | ⭐⭐⭐⭐ |

### 1.2 音视频质量指标

```cpp
namespace live {

// 音视频质量指标
struct MediaQualityMetrics {
    // 视频指标
    struct VideoMetrics {
        int target_bitrate;        // 目标码率 (bps)
        int actual_bitrate;        // 实际码率 (bps)
        int target_fps;            // 目标帧率
        int actual_fps;            // 实际帧率
        int encode_time_ms;        // 编码耗时
        int width;                 // 分辨率宽
        int height;                // 分辨率高
        int keyframe_interval;     // 关键帧间隔
        
        // 质量相关
        int frame_drop_count;      // 丢帧数
        int freeze_count;          // 卡顿次数
        int freeze_duration_ms;    // 卡顿时长
        float qp_average;          // 平均量化参数 (越小越好)
    } video;
    
    // 音频指标
    struct AudioMetrics {
        int target_bitrate;        // 目标码率
        int actual_bitrate;        // 实际码率
        int sample_rate;           // 采样率
        int channels;              // 声道数
        float input_level;         // 输入音量
        float output_level;        // 输出音量
        
        // 质量相关
        int audio_loss_count;      // 音频丢包数
        int audio_expand_count;    // 音频拉伸次数( concealment )
        float echo_return_loss;    // 回声消除效果
    } audio;
    
    // 时间戳
    int64_t timestamp_ms;
};

// 网络质量指标
struct NetworkMetrics {
    // 往返延迟
    int rtt_ms;                    // 往返时间
    
    // 丢包
    float packet_loss_rate;        // 丢包率 (0-1)
    int packets_sent;              // 发送包数
    int packets_received;          // 接收包数
    int packets_lost;              // 丢包数
    
    // 抖动
    int jitter_ms;                 // 网络抖动
    
    // 带宽
    int available_send_bandwidth;  // 可用发送带宽
    int available_recv_bandwidth;  // 可用接收带宽
    int target_bitrate;            // 目标码率
    
    // ICE状态
    std::string ice_state;         // new/checking/connected/completed/failed
    std::string connection_type;   // udp/tcp/relay
    std::string local_candidate_type;   // host/srflx/relay
    std::string remote_candidate_type;  // host/srflx/relay
    
    // 时间戳
    int64_t timestamp_ms;
};

// 设备状态指标
struct DeviceMetrics {
    // CPU
    float cpu_usage_percent;       // CPU使用率
    int cpu_cores;                 // CPU核心数
    
    // 内存
    int64_t memory_total_mb;       // 总内存
    int64_t memory_used_mb;        // 已用内存
    float memory_usage_percent;    // 内存使用率
    
    // 系统信息
    std::string os_name;           // 操作系统
    std::string os_version;        // 系统版本
    std::string device_model;      // 设备型号
    
    // 移动端特有
    float battery_level;           // 电量 (0-1)
    bool is_charging;              // 是否充电
    float device_temperature;      // 设备温度 (摄氏度)
    
    // 时间戳
    int64_t timestamp_ms;
};

} // namespace live
```

### 1.3 指标采集实现

```cpp
// metrics_collector.h
#pragma once

#include <memory>
#include <functional>
#include <vector>

namespace live {

// 指标采集器
class MetricsCollector {
public:
    using MetricsCallback = std::function<void(const MediaQualityMetrics&,
                                                  const NetworkMetrics&,
                                                  const DeviceMetrics&)>>;
    
    void SetCallback(MetricsCallback callback);
    
    // 启动采集
    void Start(int interval_ms = 1000);
    void Stop();
    
    // 上报原始数据
    void OnRtpSent(int packets, int bytes);
    void OnRtpReceived(int packets, int bytes, int lost);
    void OnRttMeasured(int rtt_ms);
    void OnVideoEncoded(int width, int height, int bitrate, int qp);
    void OnFrameRendered(bool was_dropped);
    
    // 获取当前统计
    MediaQualityMetrics GetMediaMetrics() const;
    NetworkMetrics GetNetworkMetrics() const;
    DeviceMetrics GetDeviceMetrics() const;
    
private:
    void CollectLoop();
    void CalculateMetrics();
    
    MetricsCallback callback_;
    std::atomic<bool> running_{false};
    std::thread collect_thread_;
    int interval_ms_ = 1000;
    
    // 原始数据统计
    struct RawStats {
        std::atomic<int64_t> rtp_sent_packets{0};
        std::atomic<int64_t> rtp_sent_bytes{0};
        std::atomic<int64_t> rtp_recv_packets{0};
        std::atomic<int64_t> rtp_recv_bytes{0};
        std::atomic<int64_t> rtp_lost_packets{0};
        std::atomic<int> rtt_ms{0};
        
        // 视频统计
        std::atomic<int> video_frames_encoded{0};
        std::atomic<int> video_frames_dropped{0};
        std::atomic<int> video_bitrate{0};
        
        // 窗口统计（用于计算速率）
        struct WindowStat {
            int64_t packets;
            int64_t bytes;
            int64_t timestamp_ms;
        };
        std::queue<WindowStat> send_window_;
        std::queue<WindowStat> recv_window_;
    };
    RawStats raw_stats_;
    
    // 计算后的指标
    MediaQualityMetrics media_metrics_;
    NetworkMetrics network_metrics_;
    DeviceMetrics device_metrics_;
    mutable std::mutex metrics_mutex_;
};

} // namespace live
```

---

## 2. 端到端延迟测量

### 2.1 延迟组成分析

![端到端延迟](./diagrams/e2e-latency.svg)

**延迟组成**：

| 阶段 | 典型值 | 优化方法 |
|:---|:---|:---|
| **采集延迟** | 10-50ms | 减少buffer大小 |
| **编码延迟** | 10-100ms | 低延迟编码模式 |
| **发送端缓冲** | 20-100ms | 动态调整buffer |
| **网络传输** | 20-200ms | 优化路由、减少跳数 |
| **接收端缓冲** | 50-200ms | 自适应jitter buffer |
| **解码延迟** | 10-50ms | 硬件解码 |
| **渲染延迟** | 16-33ms | 垂直同步优化 |
| **总计** | **150-700ms** | 各环节优化 |

### 2.2 延迟测量方法

**方法一：NTP同步法**
```cpp
namespace live {

// 基于NTP的延迟测量
class NtpLatencyMeasurer {
public:
    // 发送端在视频中嵌入时间戳水印
    void EmbedTimestampWatermark(VideoFrame* frame, int64_t ntp_time_ms);
    
    // 接收端检测水印并计算延迟
    int64_t DetectAndCalculateLatency(const VideoFrame& frame);
    
private:
    // 生成时间戳水印（二维码或数字）
    void GenerateTimestampQRCode(int64_t timestamp, uint8_t* buffer);
    
    // 解析水印
    int64_t ParseTimestampQRCode(const uint8_t* buffer);
};

} // namespace live
```

**方法二：RTCP XR扩展**
```cpp
// RTCP Extended Report (XR) 用于延迟测量
class RtcpXrDelayReport {
public:
    // 发送端发送LRR (Last Receiver Report)
    void SendLRR(uint32_t ssrc, uint32_t lrr, uint32_t dlrr);
    
    // 计算往返时间
    int64_t CalculateRoundTripTime(uint32_t lrr, uint32_t dlrr);
    
    // 计算端到端延迟（需要时钟同步）
    int64_t CalculateEndToEndDelay(
        int64_t sender_ntp_time,
        int64_t receiver_ntp_time,
        int64_t rtt
    );
};
```

**方法三：音视频同步时差法**
```cpp
// 利用音视频同步信息计算延迟
class A/VSyncLatencyCalculator {
public:
    // 音视频采集时使用同一个参考时钟
    void RecordCaptureTimestamp(int64_t video_timestamp, int64_t audio_timestamp);
    
    // 渲染时记录时间
    void RecordRenderTimestamp(int64_t video_timestamp, int64_t render_time);
    
    // 计算延迟
    int64_t CalculateLatency() {
        // 延迟 = 渲染时间 - 采集时间
        return render_time_ - capture_time_;
    }
};
```

---

## 3. 音视频质量评估

### 3.1 MOS评分系统

**MOS（Mean Opinion Score）**是主观质量评分的标准：

| 分值 | 质量等级 | 用户感受 |
|:---|:---|:---|
| 5 | 优秀 | 完全满意，无感知缺陷 |
| 4 | 良好 | 满意，有轻微缺陷但不影响 |
| 3 | 一般 | 有些不满意，有感知缺陷 |
| 2 | 较差 | 不满意，有明显缺陷 |
| 1 | 很差 | 完全不满意，无法使用 |

**客观MOS估算公式**：
```cpp
namespace live {

// 基于网络指标的MOS估算
class MosCalculator {
public:
    // 音频MOS (E-Model)
    static double CalculateAudioMOS(const NetworkMetrics& network) {
        // 简化的E-Model计算
        double delay_factor = 0;
        if (network.rtt_ms < 150) {
            delay_factor = 0;
        } else if (network.rtt_ms < 400) {
            delay_factor = (network.rtt_ms - 150) * 0.01;
        } else {
            delay_factor = 2.5 + (network.rtt_ms - 400) * 0.02;
        }
        
        double loss_factor = network.packet_loss_rate * 100 * 0.3;
        
        double r_value = 93.2 - delay_factor - loss_factor;
        
        // R值转MOS
        if (r_value < 0) return 1;
        if (r_value > 100) return 4.5;
        
        return 1 + 0.035 * r_value + r_value * (r_value - 60) * (100 - r_value) * 7e-6;
    }
    
    // 视频MOS (简化的质量模型)
    static double CalculateVideoMOS(const MediaQualityMetrics& media,
                                     const NetworkMetrics& network) {
        double score = 5.0;
        
        // 帧率影响
        if (media.video.actual_fps < 15) {
            score -= (15 - media.video.actual_fps) * 0.1;
        }
        
        // 分辨率影响
        int pixels = media.video.width * media.video.height;
        if (pixels < 640 * 480) {
            score -= 0.5;
        }
        
        // 卡顿影响
        score -= media.video.freeze_count * 0.3;
        
        // 丢包影响
        score -= network.packet_loss_rate * 100 * 0.05;
        
        return std::max(1.0, std::min(5.0, score));
    }
};

} // namespace live
```

### 3.2 VMAF视频质量评估

**VMAF（Video Multi-method Assessment Fusion）**是Netflix开源的视频质量评估算法。

```cpp
// VMAF集成
#ifdef USE_VMAF

class VMAFAnalyzer {
public:
    bool Initialize(const std::string& model_path);
    
    // 计算VMAF分数
    double CalculateVMAF(const VideoFrame& reference,
                         const VideoFrame& distorted);
    
    // 批量计算
    double CalculateVMAF(const std::vector& reference_frames,
                         const std::vector& distorted_frames);
    
private:
    void* vmaf_context_ = nullptr;
};

#endif // USE_VMAF
```

---

## 4. 实时监控Dashboard

### 4.1 Dashboard设计

![质量监控Dashboard](./diagrams/quality-dashboard.svg)

### 4.2 数据上报系统

```cpp
// metrics_reporter.h
#pragma once

namespace live {

// 监控数据上报器
class MetricsReporter {
public:
    struct Config {
        std::string endpoint;           // 上报地址
        int batch_size = 100;           // 批量上报大小
        int flush_interval_ms = 5000;   // 刷新间隔
        bool enable_compression = true; // 启用压缩
    };
    
    bool Initialize(const Config& config);
    void Shutdown();
    
    // 上报指标
    void ReportMetrics(const MediaQualityMetrics& media,
                       const NetworkMetrics& network,
                       const DeviceMetrics& device);
    
    // 上报事件
    void ReportEvent(const std::string& event_type,
                     const std::map<std::string, std::string>& properties);
    
    // 立即刷新
    void Flush();
    
private:
    void FlushLoop();
    bool SendBatch(const std::vector<std::string>& batch);
    
    Config config_;
    std::vector<std::string> buffer_;
    std::mutex buffer_mutex_;
    std::thread flush_thread_;
    std::atomic<bool> running_{false};
};

} // namespace live
```

### 4.3 Grafana Dashboard JSON

```json
{
  "dashboard": {
    "title": "Live Streaming Quality",
    "panels": [
      {
        "title": "Video Bitrate",
        "type": "graph",
        "targets": [
          {
            "expr": "video_bitrate{job=\"live-streaming\"}",
            "legendFormat": "{{user_id}}"
          }
        ]
      },
      {
        "title": "Packet Loss Rate",
        "type": "stat",
        "targets": [
          {
            "expr": "packet_loss_rate{job=\"live-streaming\"}",
            "thresholds": [0.01, 0.05]
          }
        ]
      },
      {
        "title": "MOS Score",
        "type": "gauge",
        "targets": [
          {
            "expr": "mos_score{job=\"live-streaming\"}",
            "min": 1,
            "max": 5
          }
        ]
      }
    ]
  }
}
```

---

## 5. 告警与自动化

### 5.1 告警规则配置

```cpp
namespace live {

// 告警规则
struct AlertRule {
    std::string name;              // 规则名称
    std::string metric;            // 监控指标
    std::string condition;         // 条件: >, <, ==, !=
    double threshold;              // 阈值
    int duration_sec;              // 持续时间
    std::string severity;          // 级别: info/warning/critical
    std::string message_template;  // 消息模板
};

// 告警管理器
class AlertManager {
public:
    void AddRule(const AlertRule& rule);
    void RemoveRule(const std::string& name);
    
    // 检查指标是否触发告警
    void CheckMetrics(const MediaQualityMetrics& media,
                      const NetworkMetrics& network);
    
    // 设置告警回调
    using AlertCallback = std::function<void(const AlertRule&, 
                                               const std::string& user_id,
                                               double current_value)>;
    void SetAlertCallback(AlertCallback callback);
    
private:
    std::vector<AlertRule> rules_;
    AlertCallback callback_;
    
    // 记录告警触发时间，用于去重
    std::map<std::string, int64_t> last_alert_time_;
};

// 预设告警规则
std::vector<AlertRule> GetDefaultAlertRules() {
    return {
        {
            "high_packet_loss",
            "packet_loss_rate",
            ">",
            0.05,  // 5%
            10,    // 持续10秒
            "warning",
            "User {user_id} packet loss rate is {value:.1%}"
        },
        {
            "high_latency",
            "rtt_ms",
            ">",
            300,   // 300ms
            30,    // 持续30秒
            "warning",
            "User {user_id} RTT is {value}ms"
        },
        {
            "video_freeze",
            "freeze_count",
            ">",
            3,     // 3秒内卡顿超过3次
            3,
            "critical",
            "User {user_id} video freeze detected"
        },
        {
            "low_mos",
            "mos_score",
            "<",
            3.0,   // MOS < 3
            60,    // 持续60秒
            "critical",
            "User {user_id} MOS score is {value:.1f}"
        }
    };
}

} // namespace live
```

### 5.2 自动化处理

```cpp
// 自动化处理
class AutoRemediation {
public:
    // 自动降低码率
    void AutoReduceBitrate(const std::string& user_id, 
                           int current_bitrate,
                           int target_bitrate);
    
    // 自动切换服务器
    void AutoSwitchServer(const std::string& user_id,
                          const std::string& current_server,
                          const std::string& backup_server);
    
    // 自动重连
    void AutoReconnect(const std::string& user_id);
    
    // 通知用户
    void NotifyUser(const std::string& user_id,
                    const std::string& message);
};
```

---

## 6. 本章总结

### 6.1 监控体系关键要素

1. **全面覆盖**：音视频、网络、设备三维度
2. **实时性**：秒级数据采集和上报
3. **可视化**：直观的Dashboard展示
4. **智能化**：基于数据的自动优化

### 6.2 质量指标参考值

| 指标 | 优秀 | 良好 | 一般 | 差 |
|:---|:---|:---|:---|:---|
| **MOS评分** | >4.0 | 3.5-4.0 | 3.0-3.5 | <3.0 |
| **端到端延迟** | <200ms | 200-400ms | 400-800ms | >800ms |
| **丢包率** | <1% | 1-3% | 3-5% | >5% |
| **卡顿率** | 0% | <1% | 1-3% | >3% |

### 6.3 课后思考

1. **监控设计**：设计一个针对万人直播活动的监控方案，包括关键指标和告警规则。

2. **延迟优化**：测量并优化现有系统的端到端延迟，目标从500ms降到300ms。

3. **MOS模型**：基于你的系统特点，改进MOS计算模型，使其更准确。

4. **告警降噪**：如何减少误报，设计智能告警合并策略。

---

**本章结束。下一章将学习安全防护，保障系统的安全性。**

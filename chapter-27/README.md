# 第27章：质量监控体系

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

### 1.2 监控理论基础

#### 1.2.1 指标类型详解

**时序数据的四种黄金指标类型**：

| 类型 | 定义 | 典型示例 | 采集频率 |
|:---|:---|:---|:---:|
| **计数器 (Counter)** | 单调递增的累积值，只能增加或归零 | 发送包数、接收字节数 | 实时 |
| **仪表盘 (Gauge)** | 可增可减的瞬时值 | CPU使用率、内存占用、队列长度 | 1-10秒 |
| **直方图 (Histogram)** | 采样值的分布情况 | 延迟分布、请求大小分布 | 事件触发 |
| **摘要 (Summary)** | 类似直方图，但计算滑动时间窗口内的分位数 | P99延迟、P95延迟 | 事件触发 |

**为什么需要区分指标类型？**

```cpp
// Counter：适合计算速率（rate）
// 错误示例：直接用当前值判断
if (packets_sent > threshold) { /* 错误！ */ }

// 正确做法：计算速率
float packet_rate = (packets_sent - last_packets_sent) / time_delta;

// Gauge：适合设置阈值告警
if (cpu_usage > 80.0) { /* 正确！ */ }

// Histogram：适合分析分布
// 不能只关注平均值， outliers 可能严重影响用户体验
LatencyHistogram.Record(rtt_ms);
// 分析：P50=50ms, P99=500ms → 部分用户体验极差
```

#### 1.2.2 采样策略

**采样是监控系统的核心设计决策**：

| 采样策略 | 原理 | 适用场景 | 优缺点 |
|:---|:---|:---|:---|
| **周期性采样** | 固定时间间隔采集（如每秒一次） | CPU、内存等连续指标 | 实现简单，可能错过突发峰值 |
| **事件驱动采样** | 特定事件发生时采集 | 错误、异常、状态变更 | 精准捕获关键事件，数据量不可控 |
| **自适应采样** | 根据数据变化率动态调整采样频率 | 网络抖动、码率变化 | 平衡精度和开销，实现复杂 |
| **头部采样** | 只采集前N个或按比例采样 | 日志、追踪数据 | 大幅降低数据量，可能丢失关键信息 |
| **尾部采样** | 延迟决策，根据完整上下文判断是否保留 | 分布式追踪 | 保留异常链路，需要缓存 |

**采样率选择原则**：

```cpp
// 不同指标的推荐采样策略
struct SamplingConfig {
    // 关键指标：100%采样（不能丢数据）
    struct CriticalMetrics {
        static constexpr float packet_loss = 1.0;      // 丢包事件
        static constexpr float connection_failure = 1.0; // 连接失败
        static constexpr float mos_score = 1.0;         // MOS评分
    };
    
    // 高频指标：自适应采样
    struct HighFreqMetrics {
        static constexpr float rtp_packets = 0.1;       // 10%采样
        static constexpr float frame_info = 0.01;       // 1%采样
    };
    
    // 聚合指标：周期性采样
    struct PeriodicMetrics {
        static constexpr int cpu_memory_ms = 1000;      // 1秒
        static constexpr int bandwidth_ms = 5000;       // 5秒
    };
};
```

#### 1.2.3 聚合方法

**时间聚合（降采样）**：

| 方法 | 公式 | 适用场景 | 注意事项 |
|:---|:---|:---|:---|
| **平均值 (Avg)** | $\frac{1}{n}\sum_{i=1}^{n}x_i$ | 平稳指标 | 掩盖异常值 |
| **最大值 (Max)** | $\max(x_1, x_2, ..., x_n)$ | 延迟、资源峰值 | 可能受噪声影响 |
| **最小值 (Min)** | $\min(x_1, x_2, ..., x_n)$ | 可用性指标 | |
| **百分位数 (Px)** | 排序后第x%位置的值 | 延迟、质量评估 | P99比Avg更能反映用户体验 |
| **标准差 (Std)** | $\sqrt{\frac{1}{n}\sum_{i=1}^{n}(x_i-\bar{x})^2}$ | 稳定性分析 | 值越大波动越大 |

**空间聚合（多实例合并）**：

```cpp
// 不同聚合方式的选择
enum class AggregationMethod {
    SUM,      // 总和：总带宽、总连接数
    AVG,      // 平均：平均CPU、平均延迟
    MAX,      // 最大：最坏情况延迟
    P99,      // P99：用户体验边界
    COUNT     // 计数：错误次数、异常数
};

// 示例：聚合10个SFU节点的指标
struct ClusterMetrics {
    // 总出口带宽 = SUM(各节点出口带宽)
    int64_t total_egress_bandwidth;
    
    // 平均CPU = AVG(各节点CPU)
    float avg_cpu_usage;
    
    // 集群P99延迟 = P99(所有连接的延迟)
    int p99_latency_ms;
    
    // 最大连接数 = MAX(各节点连接数)
    int max_connections;
};
```

**聚合粒度选择**：
- **原始数据**：1-10秒（用于实时告警和故障排查）
- **分钟级聚合**：保留1-7天（用于趋势分析）
- **小时级聚合**：保留30-90天（用于容量规划）
- **天级聚合**：长期保留（用于业务报表）

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

### 2.2 延迟测量方法对比

**三种主流延迟测量技术的深度对比**：

| 维度 | NTP同步法 | RTCP XR扩展 | 音视频同步时差法 |
|:---|:---|:---|:---|
| **精度** | ±5-10ms | ±1-5ms | ±1-3ms |
| **实现复杂度** | 中 | 低 | 高 |
| **侵入性** | 低（水印） | 无（协议扩展） | 高（需改采集/渲染）|
| **时钟要求** | 两端NTP同步 | 无需同步 | 同一参考时钟 |
| **适用场景** | 实验室测试 | 生产环境 | 高精度需求 |
| **额外开销** | 水印编码/解码 | 少量控制包 | 无时钟同步开销 |

#### 2.2.1 NTP同步法详解

**原理**：
1. 发送端在视频帧中嵌入当前NTP时间戳（水印/二维码）
2. 接收端解码识别水印，提取发送时间
3. 延迟 = 接收端当前时间 - 水印时间

**数学推导**：

假设：
- 发送端NTP时间：$T_s$
- 接收端NTP时间：$T_r$
- 真实延迟：$D$
- 时钟偏差：$\Delta = T_s^{real} - T_r^{real}$

则测量延迟：$D_{measured} = T_r - T_s = D + \Delta$

**精度限制**：NTP同步精度通常为5-10ms，这是该方法的误差下限。

```cpp
namespace live {

// 基于NTP的延迟测量
class NtpLatencyMeasurer {
public:
    // 发送端在视频中嵌入时间戳水印
    void EmbedTimestampWatermark(VideoFrame* frame, int64_t ntp_time_ms) {
        // 将时间戳编码为QR码
        uint8_t qr_data[64];
        EncodeTimestampToQR(ntp_time_ms, qr_data);
        
        // 嵌入到帧的角落（通常右下角，不遮挡内容）
        OverlayQRCode(frame, qr_data, 
                      frame->width - 100,   // x
                      frame->height - 100,  // y
                      80, 80);              // size
    }
    
    // 接收端检测水印并计算延迟
    int64_t DetectAndCalculateLatency(const VideoFrame& frame) {
        // 提取区域
        uint8_t extracted_qr[64];
        ExtractRegion(frame, 
                      frame.width - 100,
                      frame.height - 100,
                      80, 80,
                      extracted_qr);
        
        // 解码时间戳
        int64_t send_time = DecodeQRCode(extracted_qr);
        int64_t receive_time = GetCurrentNtpTimeMs();
        
        return receive_time - send_time;
    }
    
private:
    void EncodeTimestampToQR(int64_t timestamp, uint8_t* out);
    int64_t DecodeQRCode(const uint8_t* qr_data);
    void OverlayQRCode(VideoFrame* frame, const uint8_t* qr, int x, int y, int w, int h);
    void ExtractRegion(const VideoFrame& frame, int x, int y, int w, int h, uint8_t* out);
    int64_t GetCurrentNtpTimeMs();
};

} // namespace live
```

**适用场景**：实验室环境、自动化测试、需要精确测量但可接受少量侵入的场景。

#### 2.2.2 RTCP XR扩展详解

**原理**：利用RTCP Extended Report (XR) 协议扩展，通过往返时间推算单向延迟。

**关键公式**：

$$RTT = (T_{receive} - T_{send}) - (T_{process})$$

$$Delay_{one-way} \approx \frac{RTT}{2}$$

**优点**：
- 无需修改媒体流
- 标准协议，兼容性好
- 实现简单

**局限**：
- 假设往返路径对称（实际往往不对称）
- 受网络抖动影响大
- 无法精确测量端到端（只能测量到对端协议栈）

```cpp
// RTCP Extended Report (XR) 用于延迟测量
class RtcpXrDelayReport {
public:
    // RFC 3611: RTCP XR
    struct XrBlock {
        uint8_t block_type;      // BT = 4 (Receiver Reference Time)
        uint8_t reserved;
        uint16_t block_length;
        uint32_t ntp_timestamp_msw;  // NTP timestamp (MSW)
        uint32_t ntp_timestamp_lsw;  // NTP timestamp (LSW)
    };
    
    // 发送端发送LRR (Last Receiver Report)
    void SendLRR(uint32_t ssrc, uint32_t lrr, uint32_t dlrr);
    
    // 计算往返时间
    int64_t CalculateRoundTripTime(uint32_t lrr, uint32_t dlrr);
    
    // 计算端到端延迟（需要时钟同步）
    int64_t CalculateEndToEndDelay(
        int64_t sender_ntp_time,
        int64_t receiver_ntp_time,
        int64_t rtt
    ) {
        // 单向延迟估算 = RTT/2
        // 更精确的公式需要考虑时钟偏移
        return rtt / 2;
    }
    
    // 高级：使用时钟偏移补偿
    int64_t CalculateOneWayDelayWithSkew(
        int64_t t_send,      // 发送时间（发送端时钟）
        int64_t t_receive,   // 接收时间（接收端时钟）
        int64_t rtt,
        double skew_ppm      // 时钟漂移（ppm）
    );
};
```

#### 2.2.3 音视频同步时差法详解

**原理**：利用音视频采集时使用同一参考时钟的特性，通过音视频同步的时差来推算延迟。

**核心洞察**：
- 音视频在采集端是同步的（同一时间戳）
- 在渲染端，音视频也是同步渲染的
- 延迟差异反映了传输路径的差异

**公式推导**：

设：
- 视频采集时间：$T_{v,cap}$
- 音频采集时间：$T_{a,cap}$
- 视频渲染时间：$T_{v,ren}$
- 音频渲染时间：$T_{a,ren}$

由于采集同步：$T_{v,cap} = T_{a,cap}$

音视频延迟：
$$D_v = T_{v,ren} - T_{v,cap}$$
$$D_a = T_{a,ren} - T_{a,cap}$$

音视频时差：$\Delta = D_v - D_a = (T_{v,ren} - T_{a,ren})$

```cpp
// 利用音视频同步信息计算延迟
class AVSyncLatencyCalculator {
public:
    // 音视频采集时使用同一个参考时钟
    void RecordCaptureTimestamp(int64_t video_timestamp, int64_t audio_timestamp) {
        // 正常情况下 video_timestamp ≈ audio_timestamp
        capture_sync_point_ = (video_timestamp + audio_timestamp) / 2;
    }
    
    // 渲染时记录时间
    void RecordRenderTimestamp(int64_t video_timestamp, int64_t audio_timestamp) {
        int64_t now = GetMonotonicTimeMs();
        
        // 视频延迟
        if (video_timestamp > 0) {
            video_delay_samples_.push_back(now - video_timestamp);
        }
        
        // 音频延迟
        if (audio_timestamp > 0) {
            audio_delay_samples_.push_back(now - audio_timestamp);
        }
    }
    
    // 计算延迟统计
    struct LatencyStats {
        int64_t video_avg_ms;
        int64_t video_p99_ms;
        int64_t audio_avg_ms;
        int64_t audio_p99_ms;
        int64_t av_diff_ms;  // 音视频延迟差
    };
    
    LatencyStats CalculateLatency() {
        LatencyStats stats;
        stats.video_avg_ms = CalculateAverage(video_delay_samples_);
        stats.video_p99_ms = CalculatePercentile(video_delay_samples_, 99);
        stats.audio_avg_ms = CalculateAverage(audio_delay_samples_);
        stats.audio_p99_ms = CalculatePercentile(audio_delay_samples_, 99);
        stats.av_diff_ms = stats.video_avg_ms - stats.audio_avg_ms;
        return stats;
    }
    
private:
    int64_t capture_sync_point_ = 0;
    std::vector<int64_t> video_delay_samples_;
    std::vector<int64_t> audio_delay_samples_;
    
    int64_t CalculateAverage(const std::vector<int64_t>& samples);
    int64_t CalculatePercentile(const std::vector<int64_t>& samples, int p);
    int64_t GetMonotonicTimeMs();
};
```

**三种方法选型建议**：

| 场景 | 推荐方法 | 理由 |
|:---|:---|:---|
| 生产监控 | RTCP XR | 低开销、标准化 |
| 精度测试 | 音视频同步法 | 最高精度 |
| 实验室 | NTP水印法 | 直观、可视化 |
| 移动端 | 音视频同步法 | 硬件时钟统一 |
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

#### 3.1.1 E-Model理论详解

**E-Model（ITU-T G.107）**是业界广泛使用的语音质量客观评估模型，它通过传输参数计算期望的语音质量评分（R值），再转换为MOS。

**R值计算公式**：

$$R = R_0 - I_s - I_d - I_{e,eff} + A$$

| 参数 | 含义 | 典型范围 |
|:---|:---|:---:|
| $R_0$ | 基本信噪比因子（理想条件下的最高分） | 90-100 |
| $I_s$ | 同时损伤因子（编解码、量化等） | 0-30 |
| $I_d$ | 延迟损伤因子（网络延迟造成） | 0-50 |
| $I_{e,eff}$ | 设备损伤因子（丢包、抖动等） | 0-40 |
| $A$ | 优势因子（用户对技术的容忍度） | 0-20 |

**延迟损伤因子 $I_d$ 的计算**：

$$I_d = 0.024d + 0.11(d - 177.3) \cdot H(d - 177.3)$$

其中：
- $d$：单向端到端延迟（毫秒）
- $H(x)$：单位阶跃函数，$x<0$ 时为0，$x\geq0$ 时为1
- **177.3ms**：延迟损伤开始显著增加的临界点

```cpp
// E-Model延迟损伤计算
class EModelCalculator {
public:
    // 计算延迟损伤因子 Id
    static double CalculateDelayImpairment(double one_way_delay_ms) {
        if (one_way_delay_ms <= 177.3) {
            return 0.024 * one_way_delay_ms;
        } else {
            return 0.024 * one_way_delay_ms + 
                   0.11 * (one_way_delay_ms - 177.3);
        }
    }
    
    // 计算丢包损伤因子 Ie_eff
    static double CalculatePacketLossImpairment(
        double packet_loss_rate,  // 0-1
        int codec_type            // 编解码器类型
    ) {
        // 不同编解码器的丢包鲁棒性参数
        struct CodecParams {
            double Ie;      // 设备损伤基准
            double Bpl;     // 丢包鲁棒性因子
        };
        
        static const std::map<int, CodecParams> codec_params = {
            {0, {0, 25.0}},     // G.711
            {1, {10, 15.0}},    // G.729
            {2, {15, 10.0}},    // AMR
            {3, {5, 20.0}},     // Opus
        };
        
        auto it = codec_params.find(codec_type);
        if (it == codec_params.end()) return 0;
        
        const auto& params = it->second;
        return params.Ie + (95 - params.Ie) * 
               (packet_loss_rate / (packet_loss_rate + params.Bpl));
    }
    
    // 计算R值
    static double CalculateRValue(
        double rtt_ms,
        double packet_loss_rate,
        int codec_type = 3,    // 默认Opus
        double advantage_factor = 0
    ) {
        const double R0 = 93.2;  // 典型值
        double Is = 15.0;        // 编解码损伤（Opus）
        double Id = CalculateDelayImpairment(rtt_ms / 2);  // 单向延迟
        double Ie_eff = CalculatePacketLossImpairment(packet_loss_rate, codec_type);
        
        return R0 - Is - Id - Ie_eff + advantage_factor;
    }
    
    // R值转MOS
    static double RtoMOS(double R) {
        if (R < 0) return 1.0;
        if (R > 100) return 4.5;
        
        // ITU-T G.107 标准转换公式
        return 1 + 0.035 * R + R * (R - 60) * (100 - R) * 7e-6;
    }
};
```

**关键阈值参考**：

| R值 | MOS | 用户满意度 | 对应延迟（无丢包） |
|:---:|:---:|:---|:---:|
| 90-100 | 4.3-4.5 | 非常满意 | <50ms |
| 80-90 | 4.0-4.3 | 满意 | 50-150ms |
| 70-80 | 3.6-4.0 | 基本满意 | 150-300ms |
| 60-70 | 3.1-3.6 | 有些不满 | 300-500ms |
| 50-60 | 2.6-3.1 | 不满意 | 500-700ms |
| <50 | <2.6 | 无法接受 | >700ms |

#### 3.1.2 视频MOS模型

视频质量评估比音频更复杂，需要考虑：

| 因素 | 权重 | 影响说明 |
|:---|:---:|:---|
| **分辨率** | 高 | 低于期望值会显著降低满意度 |
| **帧率** | 中 | <15fps明显感知卡顿 |
| **卡顿** | 极高 |  freezes 是最差的用户体验 |
| **码率/压缩** | 中 | 影响画面清晰度 |
| **延迟** | 中 | 主要影响交互体验 |

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

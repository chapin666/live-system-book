# 第四章：RTMP 流媒体协议与弱网对抗

> **本章目标**：实现 RTMP 播放器，支持直播拉流，并具备弱网环境下的抗丢包能力。

第三章实现了基于 HTTP 的网络播放器，但 HTTP 协议延迟较高（3-10秒），不适合直播场景。本章将引入 **RTMP（Real-Time Messaging Protocol）**——直播行业的事实标准，延迟可控制在 1-3 秒。

同时，网络环境往往不稳定（弱网），本章还将介绍 **FEC（前向纠错）** 和 **JitterBuffer** 等技术，确保在丢包 20% 的情况下仍能流畅播放。

**阅读指南**：
- 第 1-3 节：理解 RTMP 协议原理、握手过程、Chunk 传输
- 第 4-6 节：实现 RTMP 播放器核心、FLV 解封装
- 第 7-8 节：弱网对抗技术（FEC、JitterBuffer）
- 第 9 节：性能测试与优化

---

## 目录

1. [为什么需要 RTMP：HTTP vs RTMP](#1-为什么需要-rtmphttp-vs-rtmp)
2. [RTMP 协议基础](#2-rtmp-协议基础)
3. [RTMP 握手与连接](#3-rtmp-握手与连接)
4. [Chunk 分块传输](#4-chunk-分块传输)
5. [FLV 封装格式](#5-flv-封装格式)
6. [RTMP 播放器实现](#6-rtmp-播放器实现)
7. [弱网对抗：FEC 前向纠错](#7-弱网对抗fec-前向纠错)
8. [JitterBuffer：平滑网络抖动](#8-jitterbuffer平滑网络抖动)
9. [本章总结与下一步](#9-本章总结与下一步)

---

## 1. 为什么需要 RTMP：HTTP vs RTMP

**本节概览**：对比 HTTP 和 RTMP 在直播场景下的差异，理解为什么直播行业选择 RTMP。

### 1.1 直播的核心需求

| 需求 | 说明 | HTTP 表现 | RTMP 表现 |
|:---|:---|:---|:---|
| **低延迟** | 主播说话到观众听到 < 3s | 5-10s（HLS）| 1-3s |
| **实时性** | 画面实时跟随主播 | 有缓存，滞后 | 接近实时 |
| **稳定性** | 网络波动时继续播放 | 容易卡顿 | 较好 |

### 1.2 HTTP 直播的问题

**HLS（HTTP Live Streaming）流程**：
```
服务器：将流切成 10 秒的 TS 片段
客户端：下载片段 1 → 下载片段 2 → 播放

延迟 = 切片时长 + 下载时间 + 缓冲时间 = 10s+
```

**DASH 类似**：也是基于文件分片，延迟 3-5 秒。

### 1.3 RTMP 的优势

RTMP 是**流式协议**，不是文件传输：

```
服务器 ──连续数据流──→ 客户端
         （无需等待文件生成）
```

**延迟组成**：
- 编码延迟：100-300ms
- 传输延迟：网络 RTT
- 解码延迟：50-100ms
- **总计：1-3 秒**

### 1.4 RTMP 的缺点

| 缺点 | 说明 | 应对 |
|:---|:---|:---|
| **防火墙** | 使用 1935 端口，可能被封锁 | 使用 RTMPS（443端口）|
| **浏览器** | Flash 淘汰后无原生支持 | 使用 WebRTC 或 H5 转码 |
| **移动端** | 耗电较高 | 使用 HLS 作为备选 |

**本节小结**：RTMP 适合需要低延迟的直播场景，但面临防火墙和浏览器支持问题。下一节将详细介绍 RTMP 协议。

---

## 2. RTMP 协议基础

**本节概览**：RTMP 是 Adobe 开发的二进制协议。本节介绍其消息类型、消息格式和基本流程。

### 2.1 RTMP 协议栈

```
┌─────────────────────────────────┐
│         RTMP 消息层              │  ← 命令、音视频数据
├─────────────────────────────────┤
│         Chunk 分块层             │  ← 分包传输
├─────────────────────────────────┤
│         TCP 传输层               │  ← 可靠传输
└─────────────────────────────────┘
```

### 2.2 消息类型

| 类型 ID | 名称 | 说明 |
|:---:|:---|:---|
| 1 | Set Chunk Size | 设置分块大小 |
| 4 | User Control | 用户控制消息（如 Stream Begin）|
| 5 | Window Ack Size | 窗口确认大小 |
| 6 | Set Peer Bandwidth | 设置对端带宽 |
| 8 | Audio | 音频数据 |
| 9 | Video | 视频数据 |
| 18 | AMF0 Data | 元数据（如分辨率、码率）|
| 20 | AMF0 Command | 命令（如 connect/play）|

### 2.3 基本流程

```
1. TCP 连接建立
        ↓
2. RTMP 握手（Handshake）
        ↓
3. connect 命令（连接应用）
        ↓
4. createStream 命令（创建流）
        ↓
5. play 命令（开始播放）
        ↓
6. 接收音视频数据
        ↓
7. 关闭连接
```

**本节小结**：RTMP 是分层协议，消息类型丰富，支持命令控制和数据传输。下一节将实现握手过程。

---

## 3. RTMP 握手与连接

**本节概览**：RTMP 握手是建立连接的第一步。本节介绍简单握手（Handshake）和复杂握手（带加密的握手机制）。

### 3.1 简单握手（Simple Handshake）

**流程**：
```
客户端 ──C0+C1──→ 服务器
客户端 ←──S0+S1+S2── 服务器
客户端 ──C2──→ 服务器
```

**C0/S0**：1字节，协议版本（0x03）
**C1/S1**：1536字节，时间戳 + 随机数据
**C2/S2**：1536字节，对端 S1/C1 的回复

### 3.2 握手代码实现

```cpp
// 发送 C0+C1
uint8_t c0 = 0x03;
send(socket, &c0, 1, 0);

uint8_t c1[1536];
memset(c1, 0, 4);  // 时间戳
memset(c1 + 4, 0, 4);  // 零值
// 随机填充剩余 1528 字节
for (int i = 8; i < 1536; i++) {
    c1[i] = rand() % 256;
}
send(socket, c1, 1536, 0);

// 接收 S0+S1+S2
uint8_t s0s1s2[3073];
recv(socket, s0s1s2, 3073, 0);

// 发送 C2（S1 的拷贝）
send(socket, s0s1s2 + 1, 1536, 0);  // S1 从索引 1 开始
```

### 3.3 connect 命令

握手成功后，发送 connect 命令：

```cpp
// AMF0 编码的 connect 命令
void SendConnect(int socket) {
    // 命令名 "connect"
    // 事务 ID 1
    // 对象 {
    //   app: "live",
    //   flashVer: "FMLE/3.0",
    //   tcUrl: "rtmp://example.com/live"
    // }
}
```

**本节小结**：RTMP 握手简单直接，完成后发送 connect 命令建立应用连接。下一节介绍 Chunk 分块传输。

---

## 4. Chunk 分块传输

**本节概览**：RTMP 将消息分割为 Chunk 传输，支持多路复用和优先级控制。

### 4.1 为什么分块

**问题**：一个大视频帧（如 1MB）独占连接，阻塞控制命令。

**解决**：将大消息切分成小块，交错传输：

```
视频帧 A (1MB)    音频帧 B (4KB)    控制消息 C
     │                 │                │
     ↓                 ↓                ↓
  [Chunk A1] ─→ [Chunk B1] ─→ [Chunk C] ─→ [Chunk A2] ─→ ...
```

### 4.2 Chunk 格式

```
┌────────────────────────────────────────┐
│  Basic Header (1-3 bytes)              │
│  - fmt: 2 bits (消息头格式)             │
│  - cs_id: 6-22 bits (Chunk Stream ID)  │
├────────────────────────────────────────┤
│  Message Header (0-11 bytes)           │
│  - timestamp (3/4 bytes)               │
│  - message length (3 bytes)            │
│  - message type id (1 byte)            │
│  - message stream id (4 bytes)         │
├────────────────────────────────────────┤
│  Extended Timestamp (0/4 bytes)        │
├────────────────────────────────────────┤
│  Chunk Data (最大 128 字节)             │
└────────────────────────────────────────┘
```

### 4.3 Chunk Stream ID

| CS ID | 用途 |
|:---:|:---|
| 2 | 低级别控制（如 Window Ack）|
| 3 | 命令消息（connect/play）|
| 4 | 音频数据 |
| 5 | 视频数据 |
| 6+ | 动态分配 |

**本节小结**：Chunk 机制让大消息不阻塞小消息，支持优先级控制。下一节介绍 FLV 封装格式。

---

## 5. FLV 封装格式

**本节概览**：RTMP 传输的音视频数据通常封装在 FLV 格式中。本节介绍 FLV 文件结构和 Tag 类型。

### 5.1 FLV 结构

```
┌─────────────────┐
│  FLV Header     │  9 bytes
├─────────────────┤
│  PreviousTagSize│  4 bytes (0)
├─────────────────┤
│  Tag 1          │  音频/视频/脚本
├─────────────────┤
│  PreviousTagSize│  4 bytes
├─────────────────┤
│  Tag 2          │
├─────────────────┤
│  ...            │
└─────────────────┘
```

### 5.2 Tag 格式

```
┌──────────────────────────────────┐
│  Tag Type          │  1 byte     │
│  Data Size         │  3 bytes    │
│  Timestamp         │  3 bytes    │
│  Timestamp Extended│  1 byte     │
│  Stream ID         │  3 bytes    │
├──────────────────────────────────┤
│  Data              │  n bytes    │
└──────────────────────────────────┘
```

**Tag Type**：
- 8 = 音频
- 9 = 视频
- 18 = 脚本/元数据

### 5.3 Video Tag 结构

```
第 1 字节：
  - 高 4 位：帧类型（1=关键帧，2=非关键帧）
  - 低 4 位：编码 ID（7=H.264）

后续数据：
  - AVC 包类型（1 byte）：0=序列头，1=NALU，2=结束
  - Composition Time（3 bytes）
  - NALU 数据
```

**本节小结**：FLV 是 RTMP 的常用封装格式，Tag 结构简单清晰。下一节将实现完整的 RTMP 播放器。

---

## 6. RTMP 播放器实现

**本节概览**：整合前面章节的内容，实现完整的 RTMP 拉流播放器。

### 6.1 类设计

```cpp
#pragma once
#include <string>
#include <functional>

namespace live {

using OnAudioCallback = std::function<void(const uint8_t* data, size_t size, uint32_t pts)>;
using OnVideoCallback = std::function<void(const uint8_t* data, size_t size, uint32_t pts, bool keyframe)>;

class RtmpPlayer {
public:
    explicit RtmpPlayer(const std::string& url);
    ~RtmpPlayer();

    bool Connect();
    void Disconnect();
    bool IsConnected() const { return connected_; }

    void SetAudioCallback(OnAudioCallback cb) { on_audio_ = cb; }
    void SetVideoCallback(OnVideoCallback cb) { on_video_ = cb; }

    void Run();  // 主循环，接收数据

private:
    bool Handshake();
    bool SendConnect();
    bool SendCreateStream();
    bool SendPlay();
    bool ReadChunk();
    void ParseMessage(uint8_t msg_type, const uint8_t* data, size_t size);

    std::string url_;
    int socket_ = -1;
    bool connected_ = false;
    
    OnAudioCallback on_audio_;
    OnVideoCallback on_video_;
};

} // namespace live
```

### 6.2 关键实现

```cpp
// 解析 FLV Video Tag 并回调
void RtmpPlayer::ParseVideoTag(const uint8_t* data, size_t size) {
    if (size < 5) return;
    
    uint8_t frame_type = (data[0] >> 4) & 0x0F;
    uint8_t codec_id = data[0] & 0x0F;
    
    if (codec_id != 7) {  // 不是 H.264
        return;
    }
    
    uint8_t avc_packet_type = data[1];
    // uint32_t composition_time = (data[2] << 16) | (data[3] << 8) | data[4];
    
    const uint8_t* nalu_data = data + 5;
    size_t nalu_size = size - 5;
    
    if (avc_packet_type == 0) {
        // AVC 序列头（SPS/PPS）
        // 解析并保存
    } else if (avc_packet_type == 1) {
        // NALU 数据
        bool keyframe = (frame_type == 1);
        if (on_video_) {
            on_video_(nalu_data, nalu_size, 0, keyframe);
        }
    }
}
```

### 6.3 与 FFmpeg 整合

```cpp
// 使用 FFmpeg 解码 H.264 NALU
void OnVideoFrame(const uint8_t* data, size_t size, uint32_t pts, bool keyframe) {
    // 构造 AVPacket
    AVPacket* pkt = av_packet_alloc();
    pkt->data = const_cast<uint8_t*>(data);
    pkt->size = size;
    pkt->pts = pts;
    pkt->flags = keyframe ? AV_PKT_FLAG_KEY : 0;
    
    // 发送给解码器
    avcodec_send_packet(codec_ctx, pkt);
    av_packet_free(&pkt);
    
    // 接收解码后的帧
    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(codec_ctx, frame) == 0) {
        Render(frame);
        av_frame_unref(frame);
    }
    av_frame_free(&frame);
}
```

**本节小结**：RTMP 播放器通过 TCP 连接、握手、命令交互后，接收 FLV 封装的音视频数据，解封装后送入 FFmpeg 解码。下一节介绍弱网对抗技术。

---

## 7. 弱网对抗：FEC 前向纠错

**本节概览**：网络丢包会导致视频花屏。FEC（Forward Error Correction）通过发送冗余数据，让接收端在丢包时仍能恢复原始数据。

### 7.1 FEC 原理

**问题**：发送 10 个包，丢 2 个，需要重传，延迟增加。

**FEC 解决**：发送 12 个包（10 个数据 + 2 个冗余），允许丢 2 个：

```
原始数据：D1 D2 D3 D4 D5 D6 D7 D8 D9 D10
FEC 编码：D1 D2 D3 D4 D5 D6 D7 D8 D9 D10 R1 R2
          └────────────────────────────┘
                      可丢 2 个
```

### 7.2 XOR FEC（简单实现）

```cpp
// 4 个数据包生成 1 个冗余包
// R = D1 ^ D2 ^ D3 ^ D4

void GenerateFec(const std::vector<std::vector<uint8_t>>& data_packets,
                 std::vector<uint8_t>& fec_packet) {
    size_t size = data_packets[0].size();
    fec_packet.resize(size, 0);
    
    for (const auto& pkt : data_packets) {
        for (size_t i = 0; i < size; i++) {
            fec_packet[i] ^= pkt[i];
        }
    }
}

// 恢复：如果丢了 D1，其他都在
// D1 = R ^ D2 ^ D3 ^ D4
bool RecoverPacket(std::vector<std::vector<uint8_t>>& packets,
                   int missing_idx,
                   const std::vector<uint8_t>& fec) {
    packets[missing_idx] = fec;
    for (size_t i = 0; i < packets.size(); i++) {
        if (i != static_cast<size_t>(missing_idx)) {
            for (size_t j = 0; j < packets[missing_idx].size(); j++) {
                packets[missing_idx][j] ^= packets[i][j];
            }
        }
    }
    return true;
}
```

### 7.3 权衡

| FEC 比例 | 冗余开销 | 可恢复丢包率 | 适用场景 |
|:---:|:---:|:---:|:---|
| 10% | 10% | ~10% | 较好网络 |
| 20% | 20% | ~20% | **推荐** |
| 50% | 50% | ~50% | 极差网络 |

**本节小结**：FEC 通过牺牲带宽换取抗丢包能力。简单 XOR 适合入门，实际系统使用 Reed-Solomon 等更高效的编码。下一节介绍 JitterBuffer。

---

## 8. JitterBuffer：平滑网络抖动

**本节概览**：网络延迟不是固定的，而是波动的（抖动）。JitterBuffer 通过缓冲和重排，平滑这种波动。

### 8.1 网络抖动现象

```
理想情况：包每 33ms 到达（30fps）
实际网络：
  时间(ms): 0   20  35  80  95  100  130
  包:       P1  P2  P3  P4  P5  P6   P7
  间隔:     -   20  15  45  15  5    30

抖动 = 最大间隔 - 最小间隔 = 45 - 5 = 40ms
```

### 8.2 JitterBuffer 工作原理

```cpp
class JitterBuffer {
public:
    void InsertPacket(RtpPacket packet);
    RtpPacket GetFrame();  // 返回完整可解码的帧
    
private:
    std::map<uint32_t, RtpPacket> packets_;  // 按序列号排序
    uint32_t last_played_seq_ = 0;
    int64_t target_delay_ms_ = 100;  // 目标缓冲深度
};

void JitterBuffer::InsertPacket(RtpPacket packet) {
    packets_[packet.seq] = packet;
    
    // 计算当前缓冲深度
    if (!packets_.empty()) {
        auto newest = packets_.rbegin()->first;
        auto oldest = packets_.begin()->first;
        int buffer_ms = (newest - oldest) * 33;  // 假设 30fps
        
        // 自适应调整目标延迟
        if (buffer_ms < target_delay_ms_ / 2) {
            target_delay_ms_ *= 0.9;  // 网络变好，降低延迟
        } else if (buffer_ms > target_delay_ms_ * 1.5) {
            target_delay_ms_ *= 1.1;  // 网络变差，增加延迟
        }
    }
}
```

### 8.3 与 RTMP 结合

RTMP 基于 TCP，本身有重传机制，但延迟较高。可以改用 **RTMFP（基于 UDP）** 或 **WebRTC** 配合 JitterBuffer 实现更低延迟。

**本节小结**：JitterBuffer 通过自适应缓冲深度，平衡延迟和流畅度。对于 RTMP/TCP 场景，TCP 本身处理了重传，但 JitterBuffer 仍可平滑播放节奏。

---

## 9. 本章总结与下一步

### 9.1 本章回顾

本章实现了 RTMP 播放器：

1. **RTMP 协议**：低延迟、流式传输，适合直播
2. **握手与连接**：简单握手，connect/play 命令
3. **Chunk 传输**：分块机制，避免大消息阻塞
4. **FLV 封装**：音视频数据封装格式
5. **播放器实现**：TCP + RTMP + FLV 解封装 + FFmpeg 解码
6. **FEC**：前向纠错，抗丢包
7. **JitterBuffer**：平滑网络抖动

### 9.2 本章局限

纯软件解码在 4K 等高分辨率场景下 CPU 占用过高：

```
1080p H.264 解码：CPU 30-50%
4K H.264 解码：CPU 80-100%（卡顿）
```

### 9.3 下一步：硬件解码

第五章将引入硬件解码：

- VideoToolbox（macOS/iOS）
- MediaCodec（Android）
- VAAPI/NVDEC（Linux）
- DirectX Video Acceleration（Windows）

**第 5 章预告**：
```
./player rtmp://example.com/live/4k-stream
# CPU 占用 < 10%，流畅播放 4K
```

---

## 附录

### 参考资源

- [RTMP Specification 1.0](https://www.adobe.com/devnet/rtmp.html)
- [FLV Format Specification](https://www.adobe.com/devnet/f4v.html)
- [Reed-Solomon FEC](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction)

### 术语表

| 术语 | 解释 |
|:---|:---|
| Chunk | RTMP 分块传输单元 |
| CS ID | Chunk Stream ID，区分不同流 |
| FLV | Flash Video，音视频封装格式 |
| NALU | H.264 编码单元 |
| FEC | 前向纠错，抗丢包 |
| Jitter | 网络延迟波动 |

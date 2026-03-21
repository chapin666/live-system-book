# 第四章：RTMP 流媒体协议

> **本章目标**：深入理解 RTMP 协议原理，实现完整的 RTMP 播放器，支持直播拉流。

第三章实现了基于 HTTP 的网络播放器，但 HTTP 协议是为文件传输设计的，用于直播时延迟高达 5-10 秒。想象一下，主播说"大家好"，观众 10 秒后才听到——这种体验显然 unacceptable。

本章将引入 **RTMP（Real-Time Messaging Protocol）**——直播行业的标准协议，延迟可控制在 1-3 秒。我们将从零实现 RTMP 握手、Chunk 分块传输、FLV 解封装，最终构建完整的直播播放器。

**阅读指南**：
- 第 1-3 节：理解为什么需要 RTMP，学习协议栈和握手过程
- 第 4-6 节：深入 Chunk 传输机制、消息类型、FLV 封装
- 第 7-8 节：完整实现 RTMP 播放器，包含连接、播放、数据处理
- 第 9-10 节：性能优化、常见问题排查、本章总结

---

## 目录

1. [为什么需要 RTMP：HTTP 直播的局限](#1-为什么需要-rtmphttp-直播的局限)
2. [RTMP 协议栈概览](#2-rtmp-协议栈概览)
3. [RTMP 握手：建立连接的第一步](#3-rtmp-握手建立连接的第一步)
4. [Chunk 分块传输机制](#4-chunk-分块传输机制)
5. [RTMP 消息类型详解](#5-rtmp-消息类型详解)
6. [FLV 封装格式](#6-flv-封装格式)
7. [RTMP 播放器架构设计](#7-rtmp-播放器架构设计)
8. [代码实现：完整 RTMP 播放器](#8-代码实现完整-rtmp-播放器)
9. [性能优化与缓冲策略](#9-性能优化与缓冲策略)
10. [本章总结与下一步](#10-本章总结与下一步)

---

## 1. 为什么需要 RTMP：HTTP 直播的局限

**本节概览**：通过对比 HTTP 和 RTMP，理解直播场景对低延迟的苛刻要求。

### 1.1 HTTP 直播的工作原理

最常用的 HTTP 直播方案是 **HLS（HTTP Live Streaming）**，由 Apple 提出：

```
服务器端：
直播流 → 切片器 → [3秒片段1.ts] [3秒片段2.ts] [3秒片段3.ts] ...

客户端：
下载片段1 → 等待下载 → 下载片段2 → 播放
   ↑______________↓
        延迟 = 切片时长 + 下载时间
```

**延迟来源**：
1. **切片等待**：服务器需要收集 3-10 秒数据才能生成一个切片
2. **下载时间**：客户端下载切片需要时间
3. **缓冲时间**：客户端为了流畅播放会额外缓冲 1-2 个切片

**总延迟**：3-10 秒（切片）+ 下载时间 + 缓冲 = **5-15 秒**

### 1.2 RTMP 的低延迟原理

RTMP 是**流式协议**，不是文件传输：

```
HTTP 方式（文件）：
服务器 ──生成文件──→ 客户端下载 ──播放
    ↑需要等待文件生成↑

RTMP 方式（流）：
服务器 ──实时数据流──→ 客户端立即播放
    ↑无需等待，收到即播↑
```

**RTMP 延迟组成**：
- 编码延迟：50-100ms（主播端编码）
- 网络传输：RTT × N（通常 20-100ms）
- 缓冲区：200-500ms（抗抖动）
- 解码延迟：10-30ms
- **总延迟：1-3 秒**

### 1.3 延迟对比实测

假设主播在 T0 时刻说"Hello"：

| 协议 | T0+0.5s | T0+1s | T0+3s | T0+5s | T0+10s |
|:---|:---:|:---:|:---:|:---:|:---:|
| **RTMP** | 编码中 | 传输中 | ✅ 播放 | - | - |
| **HLS(6s切片)** | 切片中 | 切片中 | 切片完成 | 下载中 | ✅ 播放 |

**直播场景需求**：
- **秀场直播**：3-5 秒延迟可接受
- **游戏直播**：1-3 秒延迟（主播与弹幕互动）
- **连麦互动**：< 500ms 延迟（实时对话）

### 1.4 RTMP 的代价

RTMP 并非完美，它也有自己的局限：

| 问题 | 原因 | 解决方案 |
|:---|:---|:---|
| **防火墙阻断** | 使用 1935 端口，非标准 HTTP 端口 | 使用 RTMPS（443端口）或 HTTP 隧道 |
| **浏览器不支持** | Flash 被淘汰，无原生支持 | 使用 WebRTC 或服务器转 HLS |
| **移动端耗电** | TCP 长连接保持心跳 | 优化心跳间隔，使用 QUIC |

**本节小结**：HTTP 直播延迟 5-15 秒，RTMP 延迟 1-3 秒。对于需要实时互动的直播场景，RTMP 是更好的选择。下一节将介绍 RTMP 协议栈结构。

---

## 2. RTMP 协议栈概览

**本节概览**：RTMP 不是单一协议，而是分层设计的协议栈。本节介绍各层职责和数据流向。

### 2.1 协议栈分层

```
┌─────────────────────────────────────────────┐
│              应用层（RTMP Message）          │  ← 命令、元数据、音视频
│         - Command（connect/play）            │
│         - Data（onMetaData）                 │
│         - Audio/Video 数据                   │
├─────────────────────────────────────────────┤
│              分块层（Chunk）                 │  ← 分包、复用、优先级
│         - Chunk Header                       │
│         - Chunk Data                         │
├─────────────────────────────────────────────┤
│              传输层（TCP）                   │  ← 可靠传输、拥塞控制
│         - 面向连接、字节流                   │
└─────────────────────────────────────────────┘
```

### 2.2 各层职责详解

**传输层（TCP）**：
- 提供可靠的字节流传输
- 自动重传、顺序保证、拥塞控制
- RTMP 默认端口：1935

**分块层（Chunk）**：
- 将大消息分割成小块（默认 128 字节）
- 支持多路复用（音频、视频、命令共享连接）
- 支持优先级（控制命令优先于音视频）

**消息层（Message）**：
- 定义消息类型（命令、数据、音视频）
- 携带时间戳和负载长度
- 支持自定义消息（如 AMF 格式）

### 2.3 数据流向

```
发送端：
App 数据 → Message 封装 → Chunk 分块 → TCP 发送

接收端：
TCP 接收 → Chunk 重组 → Message 解析 → App 处理
```

**本节小结**：RTMP 协议栈分三层：TCP 负责可靠传输，Chunk 层负责分包复用，Message 层负责应用语义。下一节介绍连接建立的第一步——握手。

---

## 3. RTMP 握手：建立连接的第一步

**本节概览**：握手是 RTMP 连接建立的第一步，类似于 TCP 握手，但 RTMP 有自己的协议。本节介绍简单握手和复杂握手的区别。

### 3.1 握手的作用

握手完成以下目标：
1. **协议版本协商**：确认双方支持 RTMP 1.0
2. **时间同步**：交换时间戳，用于后续延迟计算
3. **密钥交换**（复杂握手）：为加密通信做准备

### 3.2 简单握手（Simple Handshake）

简单握手使用固定格式的数据包，不进行加密：

**握手流程（3 次交换）**：
```
客户端                    服务器
  │                        │
  │ ─────── C0+C1 ───────→ │
  │                        │
  │ ←────── S0+S1+S2 ───── │
  │                        │
  │ ─────── C2 ─────────→  │
  │                        │
  │        [握手完成]       │
```

**数据包格式**：

| 包名 | 大小 | 内容 |
|:---:|:---:|:---|
| **C0/S0** | 1 byte | 版本号（固定 0x03）|
| **C1/S1** | 1536 bytes | 时间戳(4B) + 零值(4B) + 随机数据(1528B) |
| **C2/S2** | 1536 bytes | 对端 S1/C1 的拷贝 |

**代码实现**：

```cpp
// 发送 C0+C1
bool SendC0C1(int socket) {
    uint8_t c0 = 0x03;  // RTMP 版本
    if (send(socket, &c0, 1, 0) != 1) return false;
    
    uint8_t c1[1536];
    // 时间戳（毫秒）
    uint32_t timestamp = GetCurrentTimeMs();
    c1[0] = (timestamp >> 24) & 0xFF;
    c1[1] = (timestamp >> 16) & 0xFF;
    c1[2] = (timestamp >> 8) & 0xFF;
    c1[3] = timestamp & 0xFF;
    
    // 零值
    memset(c1 + 4, 0, 4);
    
    // 随机数据
    for (int i = 8; i < 1536; i++) {
        c1[i] = rand() % 256;
    }
    
    return send(socket, c1, 1536, 0) == 1536;
}

// 接收 S0+S1+S2
bool ReceiveS0S1S2(int socket, uint8_t* s0s1s2) {
    int total = 0;
    while (total < 3073) {
        int n = recv(socket, s0s1s2 + total, 3073 - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    return true;
}

// 发送 C2（S1 的拷贝）
bool SendC2(int socket, const uint8_t* s1) {
    return send(socket, s1, 1536, 0) == 1536;
}
```

### 3.3 复杂握手（Complex Handshake）

复杂握手增加了 Diffie-Hellman 密钥交换，用于生成加密密钥。Adobe 的 RTMPE（加密 RTMP）使用此握手。

**与简单握手的区别**：
- 时间戳后的 1528 字节不是随机数，而是 DH 参数
- 需要计算共享密钥
- 后续通信使用 RC4 加密

**本节小结**：简单握手适用于大多数场景，3 次交换完成连接建立。复杂握手用于加密通信，实现更复杂。下一节介绍 Chunk 分块传输机制。

---

## 4. Chunk 分块传输机制

**本节概览**：Chunk 是 RTMP 的核心机制，解决大消息阻塞问题。本节详细介绍 Chunk 格式和分块算法。

### 4.1 为什么需要分块

**问题场景**：一个 I 帧可能 100KB，如果不分块，会独占 TCP 连接：

```
无分块：
[==================== 大视频帧 ====================]
                     ↑
              控制命令被阻塞，无法发送

有分块：
[视频块1][控制命令][视频块2][音频块][视频块3]...
    ↑        ↑         ↑
  交错传输，控制命令及时送达
```

### 4.2 Chunk 格式

每个 Chunk 由 Header 和 Data 组成：

```
┌─────────────────────────────────────────┐
│  Basic Header (1-3 bytes)               │
│  ├─ fmt (2 bits): 消息头格式             │
│  └─ cs_id (6-22 bits): Chunk Stream ID   │
├─────────────────────────────────────────┤
│  Message Header (0-11 bytes)            │
│  ├─ timestamp (3 bytes)                 │
│  ├─ message length (3 bytes)            │
│  ├─ message type id (1 byte)            │
│  └─ message stream id (4 bytes)         │
├─────────────────────────────────────────┤
│  Extended Timestamp (0-4 bytes)         │
├─────────────────────────────────────────┤
│  Chunk Data (最多 128 字节)              │
└─────────────────────────────────────────┘
```

### 4.3 Basic Header 详解

Basic Header 第一个字节决定格式：

```
┌────────────────┬────────────────┐
│ fmt (2 bits)   │ cs_id (6 bits) │
└────────────────┴────────────────┘
  7   6          5   4   3   2   1   0
```

**cs_id 编码规则**：

| cs_id 值 | 编码方式 | 说明 |
|:---:|:---:|:---|
| 0-63 | 1 字节（cs_id 直接）| 常用流 |
| 64-319 | 2 字节（0 + 1 字节扩展）| 中等数量流 |
| 64-65599 | 3 字节（1 + 2 字节扩展）| 大量流 |

**预定义 CS ID**：

| CS ID | 用途 |
|:---:|:---|
| 2 | 低级别控制（Window Ack）|
| 3 | 命令消息（connect/play）|
| 4 | 音频数据 |
| 5 | 视频数据 |
| 6+ | 动态分配 |

### 4.4 Message Header 格式（fmt = 0）

当 fmt = 0 时，使用完整的 11 字节消息头：

```
┌─────────────────────────────────────────┐
│  timestamp (3 bytes)                    │  相对时间戳
├─────────────────────────────────────────┤
│  message length (3 bytes)               │  消息总长度
├─────────────────────────────────────────┤
│  message type id (1 byte)               │  消息类型
├─────────────────────────────────────────┤
│  message stream id (4 bytes, LE)        │  消息流 ID
└─────────────────────────────────────────┘
```

**fmt 值含义**：

| fmt | Header 大小 | 说明 |
|:---:|:---:|:---|
| 0 | 11 bytes | 新消息，完整头 |
| 1 | 7 bytes | 同一流，同类型，时间戳变化 |
| 2 | 3 bytes | 同一流，同类型，同大小，时间戳变化 |
| 3 | 0 bytes | 完全重复上一 Chunk 的头 |

### 4.5 分块算法

```cpp
// 将消息分块发送
void SendMessageInChunks(int socket, uint8_t msg_type, 
                         const uint8_t* data, size_t len,
                         uint32_t timestamp, uint32_t stream_id) {
    const size_t CHUNK_SIZE = 128;
    size_t offset = 0;
    bool first_chunk = true;
    
    while (offset < len) {
        // 计算本次发送的数据量
        size_t chunk_len = std::min(CHUNK_SIZE, len - offset);
        
        // 发送 Chunk Header
        if (first_chunk) {
            // 第一个 Chunk：fmt = 0，完整头
            SendChunkHeader(socket, 0, 5, timestamp, len, msg_type, stream_id);
            first_chunk = false;
        } else {
            // 后续 Chunk：fmt = 3，无头
            SendChunkHeader(socket, 3, 5, 0, 0, 0, 0);
        }
        
        // 发送 Chunk Data
        send(socket, data + offset, chunk_len, 0);
        offset += chunk_len;
    }
}
```

**本节小结**：Chunk 机制将大消息分块，支持交错传输，避免阻塞。Header 使用变长编码节省带宽。下一节介绍 RTMP 消息类型。

---

## 5. RTMP 消息类型详解

**本节概览**：RTMP 定义了多种消息类型，用于命令控制、数据传输和状态通知。本节详细介绍各类消息。

### 5.1 消息类型总览

| 类型 ID | 名称 | 方向 | 用途 |
|:---:|:---|:---:|:---|
| 1 | Set Chunk Size | C→S, S→C | 设置最大 Chunk 大小 |
| 2 | Abort Message | C→S, S→C | 丢弃部分消息 |
| 3 | Acknowledgement | C→S, S→C | 确认收到字节数 |
| 4 | User Control | S→C | 用户控制事件 |
| 5 | Window Ack Size | C→S, S→C | 设置窗口确认大小 |
| 6 | Set Peer Bandwidth | C→S, S→C | 限制对端带宽 |
| 8 | Audio Message | C→S, S→C | 音频数据 |
| 9 | Video Message | C→S, S→C | 视频数据 |
| 18 | AMF0 Data | C→S, S→C | 元数据（onMetaData）|
| 20 | AMF0 Command | C→S, S→C | 命令（connect/play）|

### 5.2 协议控制消息（1-6）

**Set Chunk Size（类型 1）**：
```
消息体：4 bytes（大端序）
默认值：128 bytes
最大值：65536 bytes

示例：设置 Chunk 大小为 4096
┌────┬────┬────┬────┐
│ 00 │ 00 │ 10 │ 00 │  = 4096
└────┴────┴────┴────┘
```

**Window Ack Size（类型 5）**：
```
作用：告知对端，收到多少字节后需要发送确认
默认：2500000 bytes

示例：每收到 1MB 发送确认
┌────┬────┬────┬────┐
│ 00 │ 10 │ 00 │ 00 │  = 1048576
└────┴────┴────┴────┘
```

### 5.3 命令消息（类型 20）

命令消息使用 AMF0（Action Message Format）编码，类似 JSON 但二进制格式。

**connect 命令**：
```
命令名："connect"
事务 ID：1
参数对象：
{
    app: "live",                    // 应用名
    flashVer: "FMLE/3.0",          // Flash 版本
    tcUrl: "rtmp://server/live",   // 服务器 URL
    fpad: false,                   // 代理标志
    capabilities: 15,              // 能力值
    audioCodecs: 0x0FFF,           // 支持的音频编码
    videoCodecs: 0x00FF,           // 支持的视频编码
    videoFunction: 1               // 支持的视频功能
}
```

**play 命令**：
```
命令名："play"
事务 ID：0（无响应）
参数：
{
    streamName: "stream123",  // 流名
    start: -2,                // -2=从直播点播放
    duration: -1,             // -1=播放全部
    reset: true               // 重置之前的播放
}
```

### 5.4 用户控制消息（类型 4）

用户控制消息用于通知客户端状态变化：

| 事件类型 | 值 | 说明 |
|:---:|:---:|:---|
| Stream Begin | 0 | 流开始，可以播放 |
| Stream EOF | 1 | 流结束 |
| Stream Dry | 2 | 流没有更多数据 |
| Set Buffer Length | 3 | 设置客户端缓冲区长度 |
| Stream Is Recorded | 4 | 流是录制文件 |
| Ping Request | 6 | 服务器心跳请求 |
| Ping Response | 7 | 客户端心跳响应 |

**本节小结**：RTMP 消息类型丰富，涵盖协议控制、命令交互、音视频传输。AMF0 用于命令和元数据编码。下一节介绍 FLV 封装格式。

---

## 6. FLV 封装格式

**本节概览**：RTMP 传输的音视频数据通常封装在 FLV 格式中。本节详细介绍 FLV 文件结构和 Tag 类型。

### 6.1 FLV 概述

FLV（Flash Video）是 Adobe 开发的格式，特点：
- **简单**：格式清晰，易于解析
- **通用**：几乎所有 RTMP 服务器支持
- **流式**：可以边下载边播放

### 6.2 FLV 文件结构

```
┌──────────────────────────┐
│  FLV Header (9 bytes)    │
│  - Signature: "FLV"      │
│  - Version: 1            │
│  - Flags: 音视频标志      │
│  - Header Size: 9        │
├──────────────────────────┤
│  PreviousTagSize0 (4B)   │  总是 0
├──────────────────────────┤
│  Tag 1                   │
├──────────────────────────┤
│  PreviousTagSize1 (4B)   │  = Tag1 大小
├──────────────────────────┤
│  Tag 2                   │
├──────────────────────────┤
│  PreviousTagSize2 (4B)   │  = Tag2 大小
├──────────────────────────┤
│  ...                     │
└──────────────────────────┘
```

### 6.3 FLV Tag 结构

```
┌─────────────────────────────────────────┐
│  Tag Type (1 byte)                      │
│  - 8: 音频                              │
│  - 9: 视频                              │
│  - 18: 脚本/元数据                       │
├─────────────────────────────────────────┤
│  Data Size (3 bytes, 大端)              │
├─────────────────────────────────────────┤
│  Timestamp (3 bytes, 毫秒)              │
├─────────────────────────────────────────┤
│  Timestamp Extended (1 byte)            │
│  （高 8 位，支持 32 位时间戳）           │
├─────────────────────────────────────────┤
│  Stream ID (3 bytes, 总是 0)            │
├─────────────────────────────────────────┤
│  Data (Data Size 字节)                  │
└─────────────────────────────────────────┘
```

### 6.4 Video Tag 详解

视频 Tag 的第一字节包含重要信息：

```
┌─────────────────┬─────────────────┐
│ 帧类型 (4 bits) │ 编码 ID (4 bits) │
└─────────────────┴─────────────────┘
```

**帧类型**：

| 值 | 类型 | 说明 |
|:---:|:---|:---|
| 1 | Keyframe | I 帧，可独立解码 |
| 2 | Inter-frame | P 帧，依赖参考帧 |
| 3 | Disposable Inter-frame | B 帧（较少使用）|
| 5 | Video Info | 视频信息帧 |

**编码 ID**：

| 值 | 编码 |
|:---:|:---|
| 2 | H.263 |
| 7 | AVC（H.264）|

**AVC 视频数据**：
```
第 1 字节：帧类型 + 编码 ID（0x17 = I 帧，0x27 = P 帧）
第 2 字节：AVC 包类型
         - 0: AVC 序列头（SPS/PPS）
         - 1: AVC NALU（视频数据）
         - 2: AVC 结束
第 3-5 字节：Composition Time（PTS - DTS）
第 6+ 字节：实际数据
```

### 6.5 Audio Tag 详解

```
第 1 字节：声音格式（4 bits）+ 采样率（2 bits）+ 位深（1 bit）+ 声道（1 bit）

格式值：
- 0: Linear PCM
- 10: AAC

后续字节：
- AAC 包类型（1 byte）：0=序列头，1=原始数据
- AAC 数据
```

**本节小结**：FLV 是 RTMP 的常用封装格式，结构简单清晰。Video Tag 包含帧类型和编码信息，Audio Tag 包含格式和采样信息。下一节设计 RTMP 播放器架构。

---

## 7. RTMP 播放器架构设计

**本节概览**：设计 RTMP 播放器的整体架构，包括连接管理、数据接收、解封装、解码渲染等模块。

### 7.1 架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                     RTMP 播放器架构                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   连接管理    │  │   数据接收    │  │   解封装     │          │
│  │  Connection  │→ │   Receiver   │→ │  FLV Parser │          │
│  │              │  │              │  │              │          │
│  │ - TCP 连接   │  │ - Chunk 重组 │  │ - 分离音视频 │          │
│  │ - RTMP 握手  │  │ - 消息解析   │  │ - 提取 NALU  │          │
│  │ - 命令交互   │  │ - 缓冲管理   │  │ - 时间戳同步 │          │
│  └──────────────┘  └──────────────┘  └──────┬───────┘          │
│                                             │                   │
│                                             ↓                   │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐          │
│  │   渲染显示    │← │   视频解码    │← │  音频解码    │          │
│  │   SDL/OpenGL │← │  H.264 Dec   │← │  AAC Dec    │          │
│  └──────────────┘  └──────────────┘  └──────────────┘          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 模块职责

**Connection 模块**：
- 建立 TCP 连接到 RTMP 服务器
- 执行 RTMP 握手
- 发送 connect、createStream、play 命令
- 维护连接状态，处理重连

**Receiver 模块**：
- 从 TCP socket 读取原始数据
- 解析 Chunk，重组消息
- 根据消息类型分发到不同处理器
- 处理窗口确认、流量控制

**FLV Parser 模块**：
- 解析 Audio/Video Tag
- 分离 H.264 NALU 和 AAC 数据
- 处理时间戳，生成 PTS/DTS

**Decoder 模块**：
- 视频：H.264 → YUV（使用 FFmpeg 或硬件解码）
- 音频：AAC → PCM（使用 FFmpeg）

**Render 模块**：
- 视频：YUV → SDL 纹理 → 屏幕
- 音频：PCM → SDL 音频队列 → 声卡

### 7.3 数据流

```
RTMP 服务器
    ↓
┌─────────────────────────────────────────────────────────────┐
│  1. TCP 接收原始数据                                          │
│     [Chunk1][Chunk2][Chunk3]...                              │
│                                                               │
│  2. Chunk 解析器重组消息                                       │
│     → Video Message (H.264 data)                             │
│     → Audio Message (AAC data)                               │
│                                                               │
│  3. FLV 解封装                                                │
│     → Video Tag → NALUs + PTS                                │
│     → Audio Tag → AAC frames + PTS                           │
│                                                               │
│  4. 解码                                                      │
│     → NALUs → YUV frames                                     │
│     → AAC → PCM samples                                      │
│                                                               │
│  5. 渲染                                                      │
│     → YUV → SDL → 屏幕                                       │
│     → PCM → SDL → 声卡                                       │
└─────────────────────────────────────────────────────────────┘
```

**本节小结**：RTMP 播放器分为连接、接收、解封装、解码、渲染五个模块。数据流从 TCP 接收，经过层层处理最终显示。下一节实现完整代码。

---

## 8. 代码实现：完整 RTMP 播放器

**本节概览**：实现完整的 RTMP 播放器，包含连接、握手、播放命令、数据接收、解封装、解码渲染。

### 8.1 项目结构

```
rtmp-player/
├── CMakeLists.txt
├── include/
│   └── live/
│       ├── rtmp_connection.h
│       ├── rtmp_receiver.h
│       └── flv_parser.h
└── src/
    ├── main.cpp
    ├── rtmp_connection.cpp
    ├── rtmp_receiver.cpp
    └── flv_parser.cpp
```

### 8.2 RTMP 连接管理

```cpp
// include/live/rtmp_connection.h
#pragma once
#include <string>
#include <functional>
#include <memory>

namespace live {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    Playing,
    Error
};

class RtmpConnection {
public:
    RtmpConnection();
    ~RtmpConnection();

    bool Connect(const std::string& url);
    void Disconnect();
    
    bool Play(const std::string& stream_name);
    void Stop();
    
    ConnectionState GetState() const { return state_; }
    int GetSocket() const { return socket_; }

private:
    bool ParseUrl(const std::string& url);
    bool TcpConnect();
    bool RtmpHandshake();
    bool SendConnect();
    bool SendCreateStream();
    bool SendPlay(const std::string& stream_name);
    
    std::string host_;
    int port_ = 1935;
    std::string app_;
    
    int socket_ = -1;
    ConnectionState state_ = ConnectionState::Disconnected;
    uint32_t stream_id_ = 0;
};

} // namespace live
```

```cpp
// src/rtmp_connection.cpp
#include "live/rtmp_connection.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace live {

RtmpConnection::RtmpConnection() = default;
RtmpConnection::~RtmpConnection() {
    Disconnect();
}

bool RtmpConnection::Connect(const std::string& url) {
    if (!ParseUrl(url)) {
        std::cerr << "[RTMP] Failed to parse URL: " << url << std::endl;
        return false;
    }
    
    state_ = ConnectionState::Connecting;
    
    if (!TcpConnect()) {
        state_ = ConnectionState::Error;
        return false;
    }
    
    state_ = ConnectionState::Handshaking;
    
    if (!RtmpHandshake()) {
        state_ = ConnectionState::Error;
        return false;
    }
    
    state_ = ConnectionState::Connected;
    
    if (!SendConnect()) {
        state_ = ConnectionState::Error;
        return false;
    }
    
    if (!SendCreateStream()) {
        state_ = ConnectionState::Error;
        return false;
    }
    
    std::cout << "[RTMP] Connected to " << host_ << ":" << port_ << std::endl;
    return true;
}

void RtmpConnection::Disconnect() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
    state_ = ConnectionState::Disconnected;
}

bool RtmpConnection::ParseUrl(const std::string& url) {
    // rtmp://host:port/app/stream
    // 简化解析，实际使用 URL 解析库
    if (url.find("rtmp://") != 0) {
        return false;
    }
    
    size_t host_start = 7;  // skip "rtmp://"
    size_t host_end = url.find(':', host_start);
    if (host_end == std::string::npos) {
        host_end = url.find('/', host_start);
        port_ = 1935;
    } else {
        size_t port_end = url.find('/', host_end);
        port_ = std::stoi(url.substr(host_end + 1, port_end - host_end - 1));
        host_end = port_end;
    }
    
    host_ = url.substr(host_start, host_end - host_start);
    
    size_t app_start = host_end + 1;
    size_t app_end = url.find('/', app_start);
    if (app_end != std::string::npos) {
        app_ = url.substr(app_start, app_end - app_start);
    } else {
        app_ = url.substr(app_start);
    }
    
    return true;
}

bool RtmpConnection::TcpConnect() {
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        perror("socket");
        return false;
    }
    
    struct hostent* server = gethostbyname(host_.c_str());
    if (!server) {
        std::cerr << "[RTMP] Failed to resolve host: " << host_ << std::endl;
        return false;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return false;
    }
    
    return true;
}

bool RtmpConnection::RtmpHandshake() {
    // 发送 C0
    uint8_t c0 = 0x03;
    if (send(socket_, &c0, 1, 0) != 1) return false;
    
    // 发送 C1
    uint8_t c1[1536];
    memset(c1, 0, 8);  // timestamp + zero
    for (int i = 8; i < 1536; i++) {
        c1[i] = rand() % 256;
    }
    if (send(socket_, c1, 1536, 0) != 1536) return false;
    
    // 接收 S0+S1+S2
    uint8_t s0s1s2[3073];
    int total = 0;
    while (total < 3073) {
        int n = recv(socket_, s0s1s2 + total, 3073 - total, 0);
        if (n <= 0) return false;
        total += n;
    }
    
    // 验证 S0
    if (s0s1s2[0] != 0x03) {
        std::cerr << "[RTMP] Unsupported version: " << (int)s0s1s2[0] << std::endl;
        return false;
    }
    
    // 发送 C2（S1 的拷贝）
    if (send(socket_, s0s1s2 + 1, 1536, 0) != 1536) return false;
    
    return true;
}

} // namespace live
```

### 8.3 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(rtmp-player VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil)
find_package(SDL2 REQUIRED)

add_executable(rtmp-player
    src/main.cpp
    src/rtmp_connection.cpp
    src/rtmp_receiver.cpp
    src/flv_parser.cpp
)

target_include_directories(rtmp-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(rtmp-player
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2
)
```

**本节小结**：实现了 RTMP 连接管理，包含 URL 解析、TCP 连接、RTMP 握手。完整播放器还需要接收器、解封装、解码渲染。下一节介绍性能优化。

---

## 9. 性能优化与缓冲策略

**本节概览**：直播播放面临网络抖动、丢包等问题。本节介绍缓冲策略、丢包恢复等优化手段。

### 9.1 网络抖动与缓冲

**问题**：网络延迟不是固定的，会有抖动：

```
理想情况：包每 33ms 到达（30fps）
实际网络：
  时间(ms): 0   15  30  80  95  100  130
  包:       P1  P2  P3  P4  P5  P6   P7
  间隔:     -   15  15  50  15  5    30

抖动 = 50 - 5 = 45ms
```

**解决方案**：接收缓冲区

```cpp
class JitterBuffer {
public:
    void InsertPacket(std::shared_ptr<VideoPacket> pkt);
    std::shared_ptr<VideoPacket> GetFrame();
    
private:
    std::map<uint32_t, std::shared_ptr<VideoPacket>> packets_;
    uint32_t last_played_seq_ = 0;
    int64_t target_delay_ms_ = 200;  // 目标缓冲 200ms
};
```

### 9.2 自适应缓冲

根据网络状况动态调整缓冲深度：

```cpp
void AdjustBuffer() {
    if (network_jitter_ < 50ms) {
        // 网络稳定，降低延迟
        target_buffer_ms_ = 100;
    } else if (network_jitter_ < 200ms) {
        // 轻度抖动
        target_buffer_ms_ = 300;
    } else {
        // 严重抖动
        target_buffer_ms_ = 500;
    }
}
```

**本节小结**：缓冲策略平衡延迟和流畅度，网络好时降低延迟，网络差时增加缓冲。下一节总结本章。

---

## 10. 本章总结与下一步

### 10.1 本章回顾

本章深入学习了 RTMP 协议：

1. **为什么需要 RTMP**：HTTP 直播延迟 5-15 秒，RTMP 延迟 1-3 秒
2. **协议栈**：TCP → Chunk → Message 三层架构
3. **握手**：简单握手 3 次交换完成连接建立
4. **Chunk 机制**：分块传输，避免大消息阻塞
5. **消息类型**：命令、音视频、控制消息
6. **FLV 封装**：Tag 结构，支持 H.264/AAC
7. **播放器架构**：连接 → 接收 → 解封装 → 解码 → 渲染

### 10.2 当前能力

```bash
./rtmp-player rtmp://server/live/stream
# 连接 RTMP 服务器
# 接收直播流
# 解码播放
```

### 10.3 下一步

第五章将实现**硬件解码**，让播放器支持 4K 直播。

**第 5 章预告**：
- VideoToolbox（macOS）
- VAAPI（Linux Intel/AMD）
- NVDEC（Linux NVIDIA）
- 4K 流畅播放

---

## 附录

### 参考资源

- [RTMP Specification 1.0](https://www.adobe.com/devnet/rtmp.html)
- [FLV Format Specification](https://www.adobe.com/devnet/f4v.html)
- [FFmpeg RTMP Protocol](https://ffmpeg.org/ffmpeg-protocols.html#rtmp)

### 术语表

| 术语 | 解释 |
|:---|:---|
| Chunk | RTMP 分块传输单元 |
| CS ID | Chunk Stream ID，流标识 |
| AMF0 | Action Message Format，二进制数据格式 |
| FLV | Flash Video，音视频封装格式 |
| NALU | H.264 编码单元 |
| Jitter | 网络延迟波动 |

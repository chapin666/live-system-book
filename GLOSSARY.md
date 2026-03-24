# 术语表 (GLOSSARY)

> 《直播连麦系统：从零到一》技术术语速查表

本术语表按字母顺序排列，涵盖了本书中涉及的核心概念和技术术语。

---

## A

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **AAC** | Advanced Audio Coding | 高级音频编码，一种有损音频压缩标准，广泛用于直播和流媒体 |
| **AEC** | Acoustic Echo Cancellation | 回声消除，消除扬声器声音被麦克风回采产生的回声 |
| **AGC** | Automatic Gain Control | 自动增益控制，自动调整麦克风输入音量 |
| **ANS** | Ambient Noise Suppression | 环境噪声抑制，降低背景噪声 |
| **ARQ** | Automatic Repeat reQuest | 自动重传请求，丢包后请求重传的数据恢复机制 |
| **AVFrame** | Audio Video Frame | FFmpeg 中的原始音视频帧数据结构 |
| **AVPacket** | Audio Video Packet | FFmpeg 中的压缩音视频数据包结构 |

## B

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **B帧** | Bidirectional Frame | 双向预测帧，参考前后帧编码，压缩率最高 |
| **BBR** | Bottleneck Bandwidth and RTT | Google 开发的拥塞控制算法，基于带宽和 RTT 建模 |
| **比特率** | Bitrate | 每秒传输的数据量，单位 bps、Kbps、Mbps |
| **编解码器** | Codec | Coder/Decoder 的缩写，用于压缩和解压音视频数据 |

## C

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **CBR** | Constant Bitrate | 恒定码率，直播常用，码率稳定 |
| **CDN** | Content Delivery Network | 内容分发网络，用于加速内容分发 |
| **CPU** | Central Processing Unit | 中央处理器，软编解码的主要计算资源 |
| **CRF** | Constant Rate Factor | 恒定质量因子，x264/x265 的质量控制模式 |
| **CV** | Computer Vision | 计算机视觉，用于美颜、滤镜等图像处理 |

## D

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **解码** | Decoding | 将压缩数据还原为原始数据的过程 |
| **DCT** | Discrete Cosine Transform | 离散余弦变换，视频压缩的核心算法 |
| **Docker** | Docker | 容器化平台，用于应用打包和部署 |
| **DTLS** | Datagram Transport Layer Security | 基于 UDP 的 TLS，用于 WebRTC 密钥交换 |
| **DTS** | Decoding Time Stamp | 解码时间戳，告诉解码器何时解码该帧 |

## E

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **编码** | Encoding | 将原始数据压缩为特定格式的过程 |
| **EGL** | Embedded Graphics Library | 嵌入式图形库，用于 GPU 渲染上下文管理 |
| **ES** | Elementary Stream | 基本流，编码后的裸音视频流 |

## F

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **FEC** | Forward Error Correction | 前向纠错，通过发送冗余数据恢复丢包 |
| **FFmpeg** | Fast Forward MPEG | 开源音视频处理框架，行业标准工具 |
| **FLV** | Flash Video | Adobe 开发的流媒体格式，RTMP 常用封装 |
| **FPS** | Frames Per Second | 帧率，每秒显示的画面数 |
| **帧** | Frame | 视频中的一幅静态图像 |

## G

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **GCC** | Google Congestion Control | Google 拥塞控制算法，WebRTC 使用 |
| **GOP** | Group of Pictures | 图像组，两个 I 帧之间的帧序列 |
| **GPU** | Graphics Processing Unit | 图形处理器，用于硬件编解码和渲染 |
| **Grafana** | Grafana | 开源监控可视化平台 |

## H

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **H.264** | H.264/AVC | 高级视频编码，最广泛使用的视频编码标准 |
| **H.265** | H.265/HEVC | 高效率视频编码，H.264 的继任者，压缩率翻倍 |
| **HLS** | HTTP Live Streaming | 苹果开发的基于 HTTP 的流媒体协议 |
| **HPA** | Horizontal Pod Autoscaler | Kubernetes 水平自动扩缩容 |
| **HTTP** | HyperText Transfer Protocol | 超文本传输协议，Web 基础协议 |

## I

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **I帧** | Intra Frame | 关键帧，独立编码的完整图像 |
| **ICE** | Interactive Connectivity Establishment | 交互式连接建立，NAT 穿透框架 |
| **IP** | Internet Protocol | 互联网协议，网络层基础协议 |

## J

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **JIT** | Just In Time | 即时编译，如 JIT 编译优化 |
| **JitterBuffer** | Jitter Buffer | 抖动缓冲区，平滑网络抖动导致的时间变化 |
| **JSON** | JavaScript Object Notation | 轻量级数据交换格式 |

## K

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **K8s** | Kubernetes | 容器编排平台，源自希腊语"舵手" |
| **Keyframe** | Keyframe | 关键帧，即 I 帧 |
| **Kbps** | Kilobits per second | 千比特每秒，码率单位 |

## L

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **LATM** | Low-overhead Audio Transport Multiplex | AAC 音频封装格式 |
| **LL-HLS** | Low Latency HLS | 低延迟 HLS，目标延迟 2-3 秒 |
| **Lossy** | Lossy Compression | 有损压缩，会丢失部分信息的压缩 |

## M

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **MCU** | Multipoint Control Unit | 多点控制单元，服务端混音混画 |
| **MOS** | Mean Opinion Score | 主观平均评分，1-5 分评价音视频质量 |
| **MP4** | MPEG-4 Part 14 | 常见视频文件容器格式 |
| **MPEG** | Moving Picture Experts Group | 动态图像专家组，制定视频标准的组织 |
| **MPEG-TS** | MPEG Transport Stream | MPEG 传输流，直播常用封装 |

## N

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **NALU** | Network Abstraction Layer Unit | H.264/H.265 的网络抽象层单元 |
| **NAT** | Network Address Translation | 网络地址转换，导致 P2P 困难的原因 |
| **NACK** | Negative ACKnowledgement | 否定确认，用于请求重传丢失的包 |
| **NTP** | Network Time Protocol | 网络时间协议，用于时间同步 |
| **NVENC** | NVIDIA Encoder | NVIDIA GPU 硬件编码器 |

## O

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **Opus** | Opus | 开源低延迟音频编解码器，WebRTC 默认使用 |
| **OS** | Operating System | 操作系统 |

## P

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **P帧** | Predicted Frame | 预测帧，参考前一帧编码 |
| **Packet** | Packet | 数据包，网络传输的基本单位 |
| **PCM** | Pulse Code Modulation | 脉冲编码调制，原始音频数据格式 |
| **Pipeline** | Pipeline | 流水线，数据顺序处理架构 |
| **Pod** | Pod | Kubernetes 中最小的部署单元 |
| **Prometheus** | Prometheus | 开源监控和告警系统 |
| **P2P** | Peer to Peer | 点对点通信，直接连接两个客户端 |
| **PTS** | Presentation Time Stamp | 显示时间戳，告诉播放器何时显示该帧 |
| **PV** | Persistent Volume | Kubernetes 持久化存储 |

## Q

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **QoE** | Quality of Experience | 体验质量，用户对服务的整体感受 |
| **QoS** | Quality of Service | 服务质量，网络服务的性能指标 |
| **QUIC** | Quick UDP Internet Connections | Google 开发的基于 UDP 的传输协议 |

## R

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **RAII** | Resource Acquisition Is Initialization | 资源获取即初始化，C++ 资源管理原则 |
| **RTP** | Real-time Transport Protocol | 实时传输协议，用于传输音视频数据 |
| **RTCP** | RTP Control Protocol | RTP 控制协议，传输统计信息 |
| **RTMP** | Real-Time Messaging Protocol | 实时消息协议，Flash 时代的直播协议 |
| **RTSP** | Real Time Streaming Protocol | 实时流协议，用于控制媒体服务器 |
| **RTT** | Round Trip Time | 往返时间，数据包一去一回的时间 |

## S

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **SFU** | Selective Forwarding Unit | 选择性转发单元，服务端只转发不解码 |
| **SDP** | Session Description Protocol | 会话描述协议，描述媒体协商参数 |
| **SDL** | Simple DirectMedia Layer | 跨平台多媒体开发库 |
| **SIMD** | Single Instruction Multiple Data | 单指令多数据，并行计算技术 |
| **SRTP** | Secure RTP | 安全的 RTP，加密传输 |
| **SRT** | Secure Reliable Transport | 安全可靠传输协议，广播级传输 |
| **SSRC** | Synchronization Source | 同步源标识，区分不同媒体流 |
| **STUN** | Session Traversal Utilities for NAT | NAT 会话穿越工具，用于发现公网地址 |
| **SVC** | Scalable Video Coding | 可伸缩视频编码，分层编码技术 |

## T

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **TCP** | Transmission Control Protocol | 传输控制协议，可靠但延迟较高 |
| **TLV** | Type-Length-Value | 类型-长度-值，数据序列化格式 |
| **TURN** | Traversal Using Relays around NAT | 使用中继穿透 NAT，通过服务器转发 |
| **TS** | Transport Stream | 传输流，MPEG-TS 的简称 |

## U

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **UDP** | User Datagram Protocol | 用户数据报协议，快速但不可靠 |
| **URL** | Uniform Resource Locator | 统一资源定位符，网址 |
| **UUID** | Universally Unique Identifier | 通用唯一标识符 |

## V

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **VAD** | Voice Activity Detection | 语音活动检测，检测是否有人说话 |
| **VBR** | Variable Bitrate | 可变码率，根据内容复杂度调整码率 |
| **VFR** | Variable Frame Rate | 可变帧率 |
| **VSync** | Vertical Synchronization | 垂直同步，防止画面撕裂 |

## W

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **WebRTC** | Web Real-Time Communication | Web 实时通信标准，浏览器原生支持 |
| **WebSocket** | WebSocket | 全双工通信协议，基于 TCP |
| **WHEP** | WebRTC-HTTP Egress Protocol | WebRTC HTTP 流出协议 |
| **WHIP** | WebRTC-HTTP Ingestion Protocol | WebRTC HTTP 流入协议 |
| **WMS** | Windows Media Services | 微软媒体服务 |

## X

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **x264** | x264 | 开源 H.264 编码器 |
| **x265** | x265 | 开源 H.265 编码器 |
| **XML** | eXtensible Markup Language | 可扩展标记语言 |

## Y

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **YUV** | YUV | 颜色空间，Y 为亮度，U/V 为色度 |
| **YUV420** | YUV 4:2:0 | YUV 采样格式，每 4 个 Y 共享 1 个 U/V |
| **YUV422** | YUV 4:2:2 | YUV 采样格式，每 2 个 Y 共享 1 个 U/V |

## Z

| 术语 | 英文 | 解释 |
|:---:|:---:|:---|
| **Zero-Copy** | Zero-Copy | 零拷贝，避免数据在内存中重复拷贝 |

---

## 快速索引

### 按技术领域分类

**视频编码**
- H.264, H.265/HEVC, AV1, SVC, GOP, I帧/P帧/B帧, NALU

**音频处理**
- AAC, Opus, PCM, 3A(AEC/ANS/AGC), VAD

**网络传输**
- RTP, RTCP, RTMP, HLS, DASH, WebRTC, SRT, WHIP, WHEP, WebTransport

**服务端架构**
- SFU, MCU, CDN, Load Balancer, K8s, Docker

**质量监控**
- QoS, QoE, MOS, FPS, Bitrate, Jitter, Latency

**开发工具**
- FFmpeg, SDL, GCC, GDB, perf, Wireshark

---

## 缩略语对照表

| 缩略语 | 全称 | 中文 |
|:---:|:---|:---|
| 3A | AEC + ANS + AGC | 音频三处理 |
| API | Application Programming Interface | 应用程序接口 |
| CPU | Central Processing Unit | 中央处理器 |
| GPU | Graphics Processing Unit | 图形处理器 |
| FPS | Frames Per Second | 每秒帧数 |
| HTTP | HyperText Transfer Protocol | 超文本传输协议 |
| IP | Internet Protocol | 互联网协议 |
| JSON | JavaScript Object Notation | JavaScript 对象表示法 |
| RTT | Round Trip Time | 往返时间 |
| SDK | Software Development Kit | 软件开发工具包 |
| SDP | Session Description Protocol | 会话描述协议 |
| SFU | Selective Forwarding Unit | 选择性转发单元 |
| SSL | Secure Sockets Layer | 安全套接层 |
| TCP | Transmission Control Protocol | 传输控制协议 |
| TLS | Transport Layer Security | 传输层安全 |
| UDP | User Datagram Protocol | 用户数据报协议 |
| URL | Uniform Resource Locator | 统一资源定位符 |
| VFR | Variable Frame Rate | 可变帧率 |
| XML | eXtensible Markup Language | 可扩展标记语言 |
| YUV | Luma and Chroma | 亮度和色度 |

---

**最后更新**：2026-03-24  
**版本**：v1.0

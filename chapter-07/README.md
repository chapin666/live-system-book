# 第七章：视频编码与码率控制

> **本章目标**：实现视频编码器，将原始视频数据压缩为 H.264，并掌握码率控制策略。

第六章完成了音视频采集，获得的是原始数据（YUV/PCM）。原始数据量巨大：
- 1080p@30fps YUV420P：93 MB/s
- 1 分钟视频：5.5 GB

显然无法直接传输，必须**编码压缩**。本章将学习 H.264 编码原理，使用 x264 编码器，并掌握 CBR/VBR 等码率控制策略。

**阅读指南**：
- 第 1-3 节：编码原理、H.264 特性、编码器选择
- 第 4-6 节：x264 使用、码率控制、编码器封装
- 第 7-8 节：硬编对比、性能优化

---

## 目录

1. [为什么需要编码：原始数据的代价](#1-为什么需要编码原始数据的代价)
2. [H.264 编码原理](#2-h264-编码原理)
3. [编码器选择：x264 vs 硬件编码](#3-编码器选择x264-vs-硬件编码)
4. [x264 编码器使用](#4-x264-编码器使用)
5. [码率控制：CBR vs VBR](#5-码率控制cbr-vs-vbr)
6. [编码器封装](#6-编码器封装)
7. [硬件编码对比](#7-硬件编码对比)
8. [本章总结](#8-本章总结)

---

## 1. 为什么需要编码：原始数据的代价

**本节概览**：通过数据对比，理解编码压缩的必要性。

### 1.1 原始视频数据量

| 分辨率 | 帧率 | 原始数据 (YUV420P) | 1 分钟大小 |
|:---|:---:|:---:|:---:|
| 720p | 30 | 42 MB/s | 2.5 GB |
| 1080p | 30 | 93 MB/s | 5.6 GB |
| 4K | 30 | 373 MB/s | 22 GB |

计算公式：
```
数据量 = 宽度 × 高度 × 1.5 (YUV420P) × 帧率
1080p: 1920 × 1080 × 1.5 × 30 = 93 MB/s
```

### 1.2 编码后的数据量

| 分辨率 | 帧率 | 编码后 (H.264) | 压缩率 |
|:---|:---:|:---:|:---:|
| 720p | 30 | 2 Mbps | 1/170 |
| 1080p | 30 | 4 Mbps | 1/185 |
| 4K | 30 | 20 Mbps | 1/150 |

**本节小结**：编码压缩率可达 100-200 倍，是视频传输的必要步骤。下一节介绍 H.264 编码原理。

---

## 2. H.264 编码原理

**本节概览**：介绍 H.264 的核心技术：帧内预测、帧间预测、变换量化、熵编码。

### 2.1 编码流程

```
原始 YUV 帧
    ↓
┌─────────┐
│ 分割    │  → 16x16 宏块
└────┬────┘
     ↓
┌─────────┐
│ 预测    │  → 帧内/帧间预测，生成残差
│ I/P/B   │
└────┬────┘
     ↓
┌─────────┐
│ 变换    │  → DCT 变换到频域
│ & 量化  │  → 量化降低精度
└────┬────┘
     ↓
┌─────────┐
│ 熵编码  │  → CABAC/CAVLC 压缩
└────┬────┘
     ↓
H.264 码流
```

### 2.2 关键技术

**帧内预测（I 帧）**：利用空间冗余
```
当前块：        预测方向：
┌───┐          ↑ 垂直
│ ? │          → 水平
└───┘          ↗ 对角线

用相邻像素预测当前块，只传残差
```

**帧间预测（P/B 帧）**：利用时间冗余
```
参考帧          当前帧          残差
┌────┐         ┌────┐         ┌────┐
│ 🚗 │    →    │ 🚗 │    =    │    │ (几乎为0)
└────┘         └────┘         └────┘

只传运动向量 + 少量残差
```

**本节小结**：H.264 通过预测+变换+熵编码实现高压缩率。下一节选择编码器实现。

---

## 3. 编码器选择：x264 vs 硬件编码

**本节概览**：对比软件编码器 x264 和硬件编码器的优劣。

### 3.1 编码器对比

| 特性 | x264 (软件) | VideoToolbox (硬件) | NVENC (硬件) |
|:---|:---|:---|:---|
| **质量** | ⭐⭐⭐ 最好 | ⭐⭐☆ 好 | ⭐⭐☆ 好 |
| **速度** | ⭐☆☆ 慢 | ⭐⭐⭐ 快 | ⭐⭐⭐ 快 |
| **CPU** | 高占用 | 低占用 | 低占用 |
| **延迟** | 高 | 低 | 低 |
| **适用** | 点播、录制 | 直播、实时 | 直播、实时 |

### 3.2 本章选择

本章使用 **x264**（软件编码）：
- 开源免费，跨平台
- 质量最好，学习价值高
- 生产环境可选择硬件编码

**本节小结**：x264 适合学习，硬件编码适合生产。下一节使用 x264。

---

## 4. x264 编码器使用

**本节概览**：使用 FFmpeg 的 libx264 进行视频编码。

### 4.1 初始化解码器

```cpp
// 查找 H.264 编码器
const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
AVCodecContext* ctx = avcodec_alloc_context3(codec);

// 配置参数
ctx->width = 1920;
ctx->height = 1080;
ctx->time_base = {1, 30};  // 30fps
ctx->framerate = {30, 1};
ctx->pix_fmt = AV_PIX_FMT_YUV420P;
ctx->gop_size = 30;        // I 帧间隔

// 码率控制
ctx->bit_rate = 4 * 1000 * 1000;  // 4 Mbps
ctx->rc_max_rate = 4 * 1000 * 1000;
ctx->rc_buffer_size = 2 * 1000 * 1000;

// 设置 preset（速度与压缩率权衡）
AVDictionary* opts = nullptr;
av_dict_set(&opts, "preset", "fast", 0);  // ultrafast, fast, medium, slow
av_dict_set(&opts, "tune", "zerolatency", 0);  // 低延迟

avcodec_open2(ctx, codec, &opts);
```

### 4.2 编码帧

```cpp
// YUV 帧编码为 H.264
void EncodeFrame(AVCodecContext* ctx, AVFrame* frame, FILE* outfile) {
    // 发送帧到编码器
    avcodec_send_frame(ctx, frame);
    
    // 接收编码后的包
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(ctx, pkt) == 0) {
        // 写入文件
        fwrite(pkt->data, 1, pkt->size, outfile);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}
```

**本节小结**：FFmpeg 封装了 x264，通过 AVCodecContext 配置编码参数。下一节介绍码率控制。

---

## 5. 码率控制：CBR vs VBR

**本节概览**：介绍恒定码率（CBR）和可变码率（VBR）的区别和适用场景。

### 5.1 CBR（恒定码率）

**特点**：码率恒定，文件大小可预测

**适用**：直播、视频会议（网络带宽固定）

```cpp
// CBR 配置
ctx->bit_rate = 4 * 1000 * 1000;  // 4 Mbps
ctx->rc_min_rate = 4 * 1000 * 1000;
ctx->rc_max_rate = 4 * 1000 * 1000;
ctx->rc_buffer_size = 4 * 1000 * 1000;

av_dict_set(&opts, "nal-hrd", "cbr", 0);
```

### 5.2 VBR（可变码率）

**特点**：复杂场景用高码率，简单场景用低码率，平均码率固定

**适用**：视频点播（质量优先）

```cpp
// VBR 配置
ctx->bit_rate = 4 * 1000 * 1000;  // 平均 4 Mbps
ctx->rc_min_rate = 0;
ctx->rc_max_rate = 8 * 1000 * 1000;  // 最高 8 Mbps

// crf 模式（恒定质量）
av_dict_set(&opts, "crf", "23", 0);  // 0-51，越小质量越高
```

### 5.3 对比

| 特性 | CBR | VBR |
|:---|:---|:---|
| 码率 | 恒定 | 波动 |
| 质量 | 波动 | 恒定 |
| 文件大小 | 可预测 | 不可预测 |
| 适用场景 | 直播、实时 | 点播、存储 |

**本节小结**：直播用 CBR，点播用 VBR。下一节封装编码器类。

---

## 6. 编码器封装

**本节概览**：封装统一的视频编码器接口。

### 6.1 接口设计

```cpp
#pragma once
#include <string>
#include <functional>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace live {

enum class RateControlMode {
    CBR,   // 恒定码率
    VBR,   // 可变码率
    CRF,   // 恒定质量
};

struct EncoderConfig {
    int width = 1280;
    int height = 720;
    int fps = 30;
    int bitrate = 4 * 1000 * 1000;  // bps
    int gop_size = 30;
    RateControlMode rc_mode = RateControlMode::CBR;
    std::string preset = "fast";  // ultrafast, fast, medium, slow
};

using OnEncodedPacket = std::function<void(const uint8_t* data, size_t size, bool keyframe)>;

class VideoEncoder {
public:
    explicit VideoEncoder(const EncoderConfig& config);
    ~VideoEncoder();

    bool Init();
    bool Encode(AVFrame* frame);  // frame 为 nullptr 时冲刷
    void SetCallback(OnEncodedPacket cb) { on_packet_ = cb; }

    int GetWidth() const { return config_.width; }
    int GetHeight() const { return config_.height; }

private:
    EncoderConfig config_;
    const AVCodec* codec_ = nullptr;
    AVCodecContext* ctx_ = nullptr;
    OnEncodedPacket on_packet_;
};

} // namespace live
```

### 6.2 实现代码

```cpp
#include "video_encoder.h"
#include <iostream>

namespace live {

VideoEncoder::VideoEncoder(const EncoderConfig& config)
    : config_(config) {
}

VideoEncoder::~VideoEncoder() {
    if (ctx_) {
        // 冲刷编码器
        Encode(nullptr);
        avcodec_free_context(&ctx_);
    }
}

bool VideoEncoder::Init() {
    // 查找编码器
    codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec_) {
        std::cerr << "[Encoder] H.264 encoder not found" << std::endl;
        return false;
    }
    
    ctx_ = avcodec_alloc_context3(codec_);
    
    // 基本参数
    ctx_>width = config_.width;
    ctx_>height = config_.height;
    ctx_>time_base = {1, config_.fps};
    ctx_>framerate = {config_.fps, 1};
    ctx_>pix_fmt = AV_PIX_FMT_YUV420P;
    ctx_>gop_size = config_.gop_size;
    
    // 码率控制
    ctx_>bit_rate = config_.bitrate;
    
    if (config_.rc_mode == RateControlMode::CBR) {
        ctx_>rc_min_rate = config_.bitrate;
        ctx_>rc_max_rate = config_.bitrate;
    }
    
    // x264 选项
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", config_.preset.c_str(), 0);
    
    if (config_.rc_mode == RateControlMode::CRF) {
        av_dict_set(&opts, "crf", "23", 0);
    } else if (config_.rc_mode == RateControlMode::CBR) {
        av_dict_set(&opts, "nal-hrd", "cbr", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
    }
    
    int ret = avcodec_open2(ctx_, codec_, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        std::cerr << "[Encoder] Failed to open codec" << std::endl;
        return false;
    }
    
    std::cout << "[Encoder] Initialized " << config_.width << "x" << config_.height
              << " @ " << config_.bitrate / 1000000 << " Mbps" << std::endl;
    return true;
}

bool VideoEncoder::Encode(AVFrame* frame) {
    if (!ctx_) return false;
    
    int ret = avcodec_send_frame(ctx_, frame);
    if (ret < 0 && ret != AVERROR_EOF) {
        return false;
    }
    
    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            break;
        }
        
        if (on_packet_) {
            bool keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
            on_packet_(pkt->data, pkt->size, keyframe);
        }
        
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    
    return true;
}

} // namespace live
```

**本节小结**：封装了统一的编码器接口，支持 CBR/VBR/CRF 码率控制。下一节对比硬件编码。

---

## 7. 硬件编码对比

**本节概览**：对比 x264 和各平台硬件编码的性能和质量。

### 7.1 性能测试

| 编码器 | 1080p@30fps | CPU 占用 | 质量 (SSIM) |
|:---|:---:|:---:|:---:|
| x264 preset=slow | 30 fps | 80% | 0.985 |
| x264 preset=fast | 60 fps | 50% | 0.975 |
| VideoToolbox | 60 fps | 15% | 0.970 |
| NVENC | 120 fps | 10% | 0.968 |

### 7.2 选择建议

| 场景 | 推荐编码器 | 理由 |
|:---|:---|:---|
| 学习/研究 | x264 | 开源，可控性强 |
| 直播（主播端）| VideoToolbox/NVENC | 低 CPU，不影响游戏 |
| 视频点播 | x264 preset=slow | 质量优先 |
| 移动端直播 | MediaCodec | 省电 |

**本节小结**：硬件编码速度快 CPU 占用低，软件编码质量高。根据场景选择。下一节总结本章。

---

## 8. 本章总结

### 8.1 本章回顾

本章实现了视频编码：

1. **编码必要性**：原始视频数据量太大（100MB/s），必须压缩
2. **H.264 原理**：预测+变换+熵编码，压缩率 100-200 倍
3. **编码器选择**：x264 学习用，硬件编码生产用
4. **码率控制**：CBR（直播）、VBR（点播）、CRF（质量优先）
5. **编码器封装**：统一接口，支持多种码率模式

### 8.2 当前能力

```
摄像头采集 → YUV420P → H.264 编码 → 压缩率 1/200
              93 MB/s      4 Mbps
```

### 8.3 下一步

第八章将实现**连麦互动**——使用 WebRTC 进行多人实时通信。

**第 8 章预告**：
- WebRTC 架构
- P2P 连接建立
- NAT 穿透
- 音视频实时传输

---

## 附录

### 参考资源

- [x264 Documentation](https://www.videolan.org/developers/x264.html)
- [FFmpeg Encoding Guide](https://trac.ffmpeg.org/wiki/Encode/H.264)
- [H.264 Specification](https://www.itu.int/rec/T-REC-H.264)

### 术语表

| 术语 | 解释 |
|:---|:---|
| CBR | 恒定码率 |
| VBR | 可变码率 |
| CRF | 恒定质量因子 |
| GOP | 图像组，I 帧间隔 |
| Preset | 编码速度与质量权衡 |

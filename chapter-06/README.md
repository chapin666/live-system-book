# 第六章：采集与音频 3A 处理

> **本章目标**：实现音视频采集，获取摄像头和麦克风数据，并进行音频 3A（AEC/ANS/AGC）处理，达到专业直播音质。

前五章实现了播放器端能力，从本章开始构建**主播端**。主播端的核心任务是**采集**——从硬件设备获取音视频原始数据，处理后推送到服务器。

**核心挑战**：
- 麦克风采集到扬声器回声 → 需要 **AEC（回声消除）**
- 环境噪声干扰 → 需要 **ANS（降噪）**
- 说话声音忽大忽小 → 需要 **AGC（自动增益）**

本章将使用 **WebRTC APM（Audio Processing Module）**——业界标准的音频处理库，实现专业级音频质量。

**阅读指南**：
- 第 1-3 节：理解采集的作用，视频采集原理，FFmpeg 设备访问
- 第 4-6 节：音频采集基础，3A 原理详解，WebRTC APM 集成
- 第 7-8 节：音视频同步，采集参数优化
- 第 9-10 节：性能优化，本章总结

---

## 目录

1. [从播放到直播：采集的重要性](#1-从播放到直播采集的重要性)
2. [视频采集原理](#2-视频采集原理)
3. [跨平台视频采集实现](#3-跨平台视频采集实现)
4. [音频采集基础](#4-音频采集基础)
5. [音频 3A 处理原理](#5-音频-3a-处理原理)
6. [WebRTC APM 集成](#6-webrtc-apm-集成)
7. [音视频同步](#7-音视频同步)
8. [采集参数配置](#8-采集参数配置)
9. [性能优化](#9-性能优化)
10. [本章总结](#10-本章总结)

---

## 1. 从播放到直播：采集的重要性

**本节概览**：回顾前五章内容，理解采集在直播系统中的位置和面临的挑战。

### 1.1 前五章回顾

```
前五章：播放器端（观众视角）
├── 本地文件播放
├── 网络下载播放
├── RTMP 直播拉流
└── 硬件解码

    ↓

第六章起：主播端（主播视角）
├── 音视频采集  ← 本章
├── 视频编码
├── RTMP 推流
└── 连麦互动
```

### 1.2 直播系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                        直播系统架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│    主播端                        服务器           观众端     │
│   ┌──────────┐                ┌─────────┐      ┌────────┐  │
│   │  摄像头  │──采集──→       │  接收   │      │        │  │
│   │  麦克风  │──采集──→       │  转码   │←─────│ 播放器 │  │
│   └──────────┘                │  分发   │      │        │  │
│       ↓                       └────┬────┘      └────────┘  │
│   ┌──────────┐                     │                       │
│   │ 3A处理   │                     ↓                       │
│   │ AEC/ANS  │                CDN 分发                      │
│   │ AGC      │                     │                       │
│   └──────────┘                     ↓                       │
│       ↓                       ┌─────────┐                  │
│   ┌──────────┐                │ 观众 1  │                  │
│   │ 视频编码 │──推流──→       │ 观众 2  │                  │
│   │ H.264   │                │ 观众 N  │                  │
│   └──────────┘                └─────────┘                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 采集面临的挑战

| 挑战 | 影响 | 解决方案 |
|:---|:---|:---|
| **设备兼容性** | 不同摄像头参数差异大 | 统一抽象接口 |
| **回声问题** | 扬声器声音被麦克风采集 | AEC 回声消除 |
| **环境噪声** | 键盘声、空调声干扰 | ANS 降噪 |
| **音量不均** | 说话声音忽大忽小 | AGC 自动增益 |
| **音视频同步** | 画面和声音不同步 | 时间戳对齐 |
| **资源占用** | 采集+编码同时运行 | 异步处理 |

### 1.4 本章目标

```
原始采集数据
    ↓
┌──────────────┐
│  视频采集     │  → 1280x720, 30fps, YUV420P
│  摄像头       │
└──────────────┘
    ↓
┌──────────────┐
│  音频采集     │  → 48kHz, 16bit, 立体声
│  麦克风       │
└──────────────┘
    ↓
┌──────────────┐
│  音频 3A      │  → 消除回声、降噪、音量均衡
│  AEC/ANS/AGC │
└──────────────┘
    ↓
处理后数据 → 编码 → 推流
```

**本节小结**：采集是主播端第一步，面临回声、噪声、同步等挑战。本章将使用 WebRTC APM 实现专业音频处理。下一节介绍视频采集原理。

---

## 2. 视频采集原理

**本节概览**：介绍摄像头的工作原理、常用像素格式、以及帧率控制。

### 2.1 摄像头工作流程

```
光学镜头
    ↓
图像传感器 (CMOS/CCD)
    ↓ 光电转换
原始 Bayer 数据
    ↓ ISP 处理
┌─────────────────────────────┐
│  ISP (Image Signal Processor)│
│  - 去噪 (Denoise)            │
│  - 白平衡 (White Balance)    │
│  - 曝光补偿 (Exposure)       │
│  - 色彩校正 (Color Correction)│
│  - 锐化 (Sharpen)            │
└─────────────────────────────┘
    ↓
输出图像 (YUV/RGB/MJPEG)
```

### 2.2 常用像素格式

| 格式 | 采样 | 每像素字节 | 用途 |
|:---|:---:|:---:|:---|
| **YUY2** | 4:2:2 | 2 | 传统摄像头 |
| **NV12** | 4:2:0 | 1.5 | 现代摄像头，硬件友好 |
| **YUV420P** | 4:2:0 | 1.5 | 编码器标准输入 |
| **MJPEG** | 压缩 | 可变 | 高分辨率场景 |
| **H.264** | 压缩 | 可变 | 部分摄像头直接输出 |

**格式选择建议**：
- **优先 NV12**：现代编码器原生支持，无需转换
- **避免 MJPEG**：需要解码，增加 CPU 负担

### 2.3 帧率与分辨率

| 场景 | 分辨率 | 帧率 | 码率建议 |
|:---|:---|:---:|:---:|
| 屏幕共享 | 1920x1080 | 15fps | 2 Mbps |
| 标准直播 | 1280x720 | 30fps | 4 Mbps |
| 游戏直播 | 1920x1080 | 60fps | 8 Mbps |
| 高清访谈 | 1920x1080 | 30fps | 6 Mbps |

**帧率与流畅度**：
- 15fps：可感知卡顿，适合静态内容
- 30fps：**标准选择**，流畅自然
- 60fps：丝滑体验，适合游戏/运动

**本节小结**：摄像头输出经过 ISP 处理，常用 NV12/YUV420P 格式。帧率选择根据场景需求。下一节实现视频采集代码。

---

## 3. 跨平台视频采集实现

**本节概览**：使用 FFmpeg 的 libavdevice 实现跨平台视频采集。

### 3.1 FFmpeg 设备采集

FFmpeg 封装了各平台的设备访问：
- Linux：Video4Linux2 (V4L2) - `/dev/video0`
- macOS：AVFoundation - `0` (默认摄像头)
- Windows：DirectShow - `video=Camera Name`

### 3.2 打开摄像头

```cpp
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <iostream>

class VideoCapture {
public:
    bool Open(const std::string& device, int width, int height, int fps) {
        // 注册设备
        avdevice_register_all();
        
        // 选择输入格式
        const AVInputFormat* input_format = nullptr;
        std::string dev = device;
        
#if defined(__APPLE__)
        input_format = av_find_input_format("avfoundation");
        if (device.empty()) dev = "0";
#elif defined(__linux__)
        input_format = av_find_input_format("v4l2");
        if (device.empty()) dev = "/dev/video0";
#endif
        
        // 设置参数
        AVDictionary* options = nullptr;
        char video_size[32];
        snprintf(video_size, sizeof(video_size), "%dx%d", width, height);
        av_dict_set(&options, "video_size", video_size, 0);
        
        char framerate[16];
        snprintf(framerate, sizeof(framerate), "%d", fps);
        av_dict_set(&options, "framerate", framerate, 0);
        
        // 优先尝试 NV12，其次是 YUY2
        av_dict_set(&options, "pixel_format", "nv12", 0);
        
        // 打开设备
        int ret = avformat_open_input(&ctx_, dev.c_str(), input_format, &options);
        av_dict_free(&options);
        
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "Failed to open camera: " << errbuf << std::endl;
            return false;
        }
        
        // 获取流信息
        ret = avformat_find_stream_info(ctx_, nullptr);
        if (ret < 0) {
            std::cerr << "Failed to find stream info" << std::endl;
            return false;
        }
        
        // 查找视频流
        video_idx_ = av_find_best_stream(ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (video_idx_ < 0) {
            std::cerr << "No video stream found" << std::endl;
            return false;
        }
        
        AVStream* stream = ctx_>streams[video_idx_];
        width_ = stream->codecpar->width;
        height_ = stream->codecpar->height;
        
        std::cout << "Camera opened: " << width_ << "x" << height_ << std::endl;
        return true;
    }
    
    AVFrame* ReadFrame() {
        AVPacket* pkt = av_packet_alloc();
        
        if (av_read_frame(ctx_, pkt) < 0) {
            av_packet_free(&pkt);
            return nullptr;
        }
        
        if (pkt->stream_index != video_idx_) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            return nullptr;
        }
        
        // 解码（如果是 MJPEG）
        // 简化处理，实际需要初始化解码器
        AVFrame* frame = av_frame_alloc();
        // ... 解码逻辑
        
        av_packet_unref(pkt);
        av_packet_free(&pkt);
        return frame;
    }
    
    void Close() {
        if (ctx_) {
            avformat_close_input(&ctx_);
        }
    }
    
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    AVFormatContext* ctx_ = nullptr;
    int video_idx_ = -1;
    int width_ = 0;
    int height_ = 0;
};
```

### 3.3 设备列表

```cpp
// 列出可用摄像头（Linux）
std::vector<std::string> ListCameras() {
    std::vector<std::string> cameras;
    for (int i = 0; i < 10; i++) {
        std::string dev = "/dev/video" + std::to_string(i);
        if (access(dev.c_str(), F_OK) == 0) {
            cameras.push_back(dev);
        }
    }
    return cameras;
}
```

**本节小结**：FFmpeg libavdevice 提供跨平台设备采集。Linux 使用 V4L2，macOS 使用 AVFoundation。优先选择 NV12 格式。下一节介绍音频采集。

---

## 4. 音频采集基础

**本节概览**：介绍音频采集的基本概念：采样率、位深、声道数，以及 FFmpeg 音频采集实现。

### 4.1 音频三要素

| 参数 | 常见值 | 说明 |
|:---|:---|:---|
| **采样率** | 44100 Hz, 48000 Hz | 每秒采样次数 |
| **位深** | 16-bit, 32-bit | 采样精度 |
| **声道数** | 1 (单声道), 2 (立体声) | 音频通道数 |

**数据量计算**：
```
48000 Hz × 16-bit × 2 声道 = 1536 kbps = 192 KB/s
1 分钟原始音频：192 KB/s × 60 = 11.25 MB
```

### 4.2 音频帧

音频数据以帧为单位处理：
```
10ms 音频帧 @ 48000Hz:
- 采样数：48000 × 0.01 = 480 个采样
- 字节数：480 × 2 声道 × 2 字节 = 1920 字节
```

常用帧长：
- 10ms：低延迟，适合实时通信
- 20ms：**标准选择**，平衡延迟和效率
- 40ms：高压缩率，适合语音

### 4.3 FFmpeg 音频采集

```cpp
#include <libavdevice/avdevice.h>

class AudioCapture {
public:
    bool Open(const std::string& device, int sample_rate, int channels) {
        avdevice_register_all();
        
        const AVInputFormat* input_format = nullptr;
        std::string dev = device;
        
#if defined(__APPLE__)
        input_format = av_find_input_format("avfoundation");
        if (device.empty()) dev = ":0";  // 默认音频输入
#elif defined(__linux__)
        input_format = av_find_input_format("alsa");
        if (device.empty()) dev = "default";
#endif
        
        AVDictionary* options = nullptr;
        char sample_rate_str[16];
        snprintf(sample_rate_str, sizeof(sample_rate_str), "%d", sample_rate);
        av_dict_set(&options, "sample_rate", sample_rate_str, 0);
        
        char channels_str[8];
        snprintf(channels_str, sizeof(channels_str), "%d", channels);
        av_dict_set(&options, "channels", channels_str, 0);
        
        int ret = avformat_open_input(&ctx_, dev.c_str(), input_format, &options);
        av_dict_free(&options);
        
        if (ret < 0) {
            std::cerr << "Failed to open audio device" << std::endl;
            return false;
        }
        
        audio_idx_ = av_find_best_stream(ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audio_idx_ < 0) {
            std::cerr << "No audio stream found" << std::endl;
            return false;
        }
        
        sample_rate_ = sample_rate;
        channels_ = channels;
        return true;
    }

private:
    AVFormatContext* ctx_ = nullptr;
    int audio_idx_ = -1;
    int sample_rate_ = 48000;
    int channels_ = 2;
};
```

**本节小结**：音频采集关注采样率（48kHz）、位深（16bit）、声道数（2）。原始数据量约 192KB/s。下一节介绍 3A 处理。

---

## 5. 音频 3A 处理原理

**本节概览**：详细介绍回声消除（AEC）、降噪（ANS）、自动增益（AGC）的工作原理。

### 5.1 回声消除（AEC）

**问题场景**：
```
远端语音 → 扬声器 → [房间混响] → 麦克风 → 回声
                ↑_____________________|
                        
主播听到观众的声音，又被自己的麦克风采集，
观众会听到自己的回声（延迟 100-500ms）
```

**AEC 原理**：
```
参考信号（扬声器输出）
    ↓
自适应滤波器 → 估计回声
    ↓
麦克风输入 - 估计回声 = 纯净语音
```

**关键算法**：
- **NLMS**（归一化最小均方）：自适应滤波
- **双端检测**：判断是单讲（只有远端/近端）还是双讲（同时说话）
- **非线性处理**：消除残余回声

### 5.2 降噪（ANS）

**问题场景**：
```
麦克风采集：
[主播语音] + [键盘敲击声] + [空调声] + [风扇声]
                    ↓
                ANS 处理
                    ↓
        [主播语音]（噪声被抑制）
```

**ANS 原理**：
1. **噪声估计**：检测无声段，估计噪声频谱
2. **频谱减法**：从带噪语音减去噪声频谱
3. **音乐噪声抑制**：平滑处理，减少失真

### 5.3 自动增益（AGC）

**问题场景**：
```
主播说话音量变化：
0-5s:   大声说话  ──→ 音量过大，刺耳
5-10s:  小声说话  ──→ 音量太小，听不清
10-15s: 正常说话  ──→ 理想音量

AGC 目标：将所有音量调整到舒适范围
```

**AGC 原理**：
```
输入音量检测 → 与目标音量比较 → 计算增益系数 → 应用增益

目标电平：-3dB ~ -1dB（峰值）
          -18dB ~ -12dB（RMS 平均）
```

**压缩特性**：
- 小声：放大增益（提升 10-20dB）
- 大声：降低增益（衰减 5-10dB）
- 防止削波：限制最大增益

### 5.4 3A 处理流程

```
麦克风输入
    ↓
┌─────────────────────────────────────┐
│ 1. AEC（回声消除）                    │
│    - 消除扬声器回声                   │
│    - 需要参考信号（扬声器输出）        │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ 2. NS（降噪）                        │
│    - 抑制键盘、风扇等稳态噪声          │
│    - 保留语音清晰度                   │
└─────────────────────────────────────┘
    ↓
┌─────────────────────────────────────┐
│ 3. AGC（自动增益）                    │
│    - 调整音量到目标范围               │
│    - 防止忽大忽小                     │
└─────────────────────────────────────┘
    ↓
处理后音频 → 编码 → 传输
```

**本节小结**：3A 处理是直播音频质量的关键。AEC 消除回声，ANS 抑制噪声，AGC 均衡音量。下一节集成 WebRTC APM。

---

## 6. WebRTC APM 集成

**本节概览**：WebRTC APM 是业界标准的音频处理库。本节介绍如何编译集成和调用 API。

### 6.1 WebRTC APM 简介

APM（Audio Processing Module）包含：
- **AEC3**：第三代回声消除，效果更好
- **NS**：噪声抑制
- **AGC2**：第二代自动增益
- **VAD**：语音活动检测
- **High Pass Filter**：高通滤波

### 6.2 集成步骤

```cpp
// include/live/audio_processor.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <memory>

namespace live {

struct AudioConfig {
    int sample_rate = 48000;
    int channels = 2;
    int frame_duration_ms = 10;
};

class AudioProcessor {
public:
    explicit AudioProcessor(const AudioConfig& config);
    ~AudioProcessor();

    bool Init();
    
    // 处理音频帧
    // mic: 麦克风输入（interleaved PCM）
    // speaker: 扬声器参考信号（用于 AEC）
    // out: 处理后输出
    void Process(const int16_t* mic, const int16_t* speaker, int16_t* out);
    
    // 设置处理开关
    void EnableAEC(bool enable);
    void EnableNS(bool enable);
    void EnableAGC(bool enable);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    AudioConfig config_;
};

} // namespace live
```

### 6.3 简化实现（基于 WebRTC）

```cpp
// src/audio_processor.cpp
#include "live/audio_processor.h"
#include <iostream>

// 简化的 3A 处理实现
// 实际生产环境应使用 WebRTC APM

namespace live {

class AudioProcessor::Impl {
public:
    Impl(const AudioConfig& cfg) : config_(cfg) {
        samples_per_frame_ = config_.sample_rate * config_.frame_duration_ms / 1000;
    }
    
    bool Init() {
        // 初始化 AEC、NS、AGC
        std::cout << "[AudioProcessor] Initialized: " 
                  <> config_.sample_rate <> "Hz, " 
                  <> config_.channels <> " channels" <> std::endl;
        return true;
    }
    
    void Process(const int16_t* mic, const int16_t* speaker, int16_t* out) {
        size_t samples = samples_per_frame_ * config_.channels;
        
        // 1. AEC：简单实现（实际用 WebRTC）
        if (aec_enabled_ && speaker) {
            for (size_t i = 0; i < samples; i++) {
                // 简单回声消除：减去参考信号的一部分
                int32_t val = mic[i] - (speaker[i] >> 2);  // 减去 25%
                out[i] = static_cast<int16_t>(std::max(-32768, std::min(32767, val)));
            }
        } else {
            memcpy(out, mic, samples * sizeof(int16_t));
        }
        
        // 2. NS：简单降噪（实际用频谱减法）
        if (ns_enabled_) {
            for (size_t i = 0; i < samples; i++) {
                // 简单门限降噪
                if (abs(out[i]) < 500) {
                    out[i] = 0;
                }
            }
        }
        
        // 3. AGC：简单增益（实际用压缩器）
        if (agc_enabled_) {
            // 计算 RMS
            int64_t sum = 0;
            for (size_t i = 0; i < samples; i++) {
                sum += out[i] * out[i];
            }
            int rms = static_cast<int>(sqrt(sum / samples));
            
            // 目标 RMS：3000
            if (rms > 0 && rms < 3000) {
                int gain = std::min(3000 / rms, 10);  // 最大增益 10x
                for (size_t i = 0; i < samples; i++) {
                    int32_t val = out[i] * gain;
                    out[i] = static_cast<int16_t>(std::max(-32768, std::min(32767, val)));
                }
            }
        }
    }

    bool aec_enabled_ = true;
    bool ns_enabled_ = true;
    bool agc_enabled_ = true;
    AudioConfig config_;
    int samples_per_frame_;
};

AudioProcessor::AudioProcessor(const AudioConfig& config)
    : impl_(std::make_unique<Impl>(config))
    , config_(config) {
}

AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::Init() {
    return impl_>Init();
}

void AudioProcessor::Process(const int16_t* mic, const int16_t* speaker, int16_t* out) {
    impl_>Process(mic, speaker, out);
}

void AudioProcessor::EnableAEC(bool enable) {
    impl_>aec_enabled_ = enable;
}

void AudioProcessor::EnableNS(bool enable) {
    impl_>ns_enabled_ = enable;
}

void AudioProcessor::EnableAGC(bool enable) {
    impl_>agc_enabled_ = enable;
}

} // namespace live
```

**本节小结**：WebRTC APM 是工业级 3A 处理库。本章使用简化实现演示原理，生产环境应集成完整 APM。下一节介绍音视频同步。

---

## 7. 音视频同步

**本节概览**：音视频采集可能产生时间差，需要通过时间戳对齐实现同步。

### 7.1 同步问题

```
理想情况：
视频帧 ────────┬────────┬────────┬────────
               ↓        ↓        ↓
音频帧 ────────┴────────┴────────┴────────
               T0       T1       T2

实际情况：
视频帧 ───────────┬────────┬────────┬──────── (延迟 50ms)
                  ↓
音频帧 ────────┬──┴────────┴────────┴────────
               ↑
           音视频不同步！
```

### 7.2 时间戳方案

```cpp
class AVSynchronizer {
public:
    // 获取当前系统时间（微秒）
    int64_t GetCurrentTime() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000000LL + tv.tv_usec;
    }
    
    // 视频帧打时间戳
    void TimestampVideoFrame(AVFrame* frame) {
        frame->pts = GetCurrentTime();
    }
    
    // 音频帧打时间戳
    void TimestampAudioFrame(AudioFrame* frame) {
        frame->pts = GetCurrentTime();
    }
    
    // 同步检查
    bool CheckSync(int64_t video_pts, int64_t audio_pts) {
        int64_t diff = video_pts - audio_pts;
        if (diff > 40000 || diff < -40000) {  // > 40ms
            std::cout << "AV sync drift: " <> diff <> " us" <> std::endl;
            return false;
        }
        return true;
    }
};
```

### 7.3 同步策略

| 策略 | 说明 | 适用 |
|:---|:---|:---|
| **视频同步到音频** | 调整视频播放速度 | 音乐直播 |
| **音频同步到视频** | 调整音频播放速度 | 口型要求高 |
| **外部时钟** | 两者都同步到独立时钟 | 专业场景 |

**本节小结**：音视频同步通过时间戳实现，容忍度约 ±40ms。视频通常同步到音频（人耳对音频更敏感）。下一节介绍采集参数配置。

---

## 8. 采集参数配置

**本节概览**：介绍采集参数的配置策略，以及不同场景的推荐设置。

### 8.1 分辨率与帧率选择

| 场景 | 分辨率 | 帧率 | 码率 |
|:---|:---|:---:|:---:|
| 屏幕共享 | 1920x1080 | 15fps | 2 Mbps |
| 标准直播 | 1280x720 | 30fps | 4 Mbps |
| 游戏直播 | 1920x1080 | 60fps | 8 Mbps |
| 高清访谈 | 1920x1080 | 30fps | 6 Mbps |

### 8.2 音频参数选择

| 参数 | 推荐值 | 说明 |
|:---|:---:|:---|
| 采样率 | 48000 Hz | 与视频行业一致 |
| 位深 | 16-bit | 足够动态范围 |
| 声道 | 立体声 | 空间感 |
| 帧长 | 20ms | 平衡延迟和效率 |

**本节小结**：采集参数根据场景选择。标准直播推荐 720p@30fps + 48kHz 音频。下一节介绍性能优化。

---

## 9. 性能优化

**本节概览**：采集+编码同时运行，需要优化 CPU 使用。

### 9.1 异步处理架构

```
采集线程
    ↓ 原始帧
┌─────────────────────────────────────┐
│  帧队列（生产者-消费者）              │
└─────────────────────────────────────┘
    ↓
处理线程（3A + 编码）
    ↓ 编码后数据
推流线程
```

### 9.2 优化建议

1. **使用硬件编码**：减轻 CPU 负担
2. **降低预览分辨率**：采集高分辨率，预览低分辨率
3. **帧率自适应**：网络差时降低帧率
4. **CPU 亲和性**：绑定采集线程到特定核心

**本节小结**：异步架构和硬件编码是性能优化的关键。下一节总结本章。

---

## 10. 本章总结

### 10.1 本章回顾

本章实现了音视频采集和 3A 处理：

1. **视频采集**：FFmpeg libavdevice，跨平台支持
2. **音频采集**：48kHz, 16-bit, 立体声
3. **音频 3A**：
   - AEC：消除扬声器回声
   - ANS：抑制环境噪声
   - AGC：自动调整音量
4. **WebRTC APM**：工业级 3A 处理库
5. **音视频同步**：时间戳对齐，±40ms 容忍度
6. **性能优化**：异步处理，硬件编码

### 10.2 当前能力

```
摄像头采集 → YUV420P
              ↓
麦克风采集 → PCM ─→ 3A处理 ─→ 纯净音频
              ↓                    ↓
           时间戳对齐 → 编码 → 推流
```

### 10.3 下一步

第七章将实现**视频编码**，将原始 YUV 压缩为 H.264。

**第 7 章预告**：
- H.264 编码原理
- x264 编码器使用
- 码率控制（CBR/VBR）
- 直播推流

---

## 附录

### 参考资源

- [FFmpeg Device Documentation](https://ffmpeg.org/ffmpeg-devices.html)
- [WebRTC Audio Processing](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/)
- [Video4Linux2 API](https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html)

### 术语表

| 术语 | 解释 |
|:---|:---|
| AEC | Acoustic Echo Cancellation，回声消除 |
| ANS/NS | Noise Suppression，降噪 |
| AGC | Automatic Gain Control，自动增益控制 |
| VAD | Voice Activity Detection，语音活动检测 |
| Interleaved | 交错采样（LR LR LR）|
| PCM | Pulse Code Modulation，脉冲编码调制 |

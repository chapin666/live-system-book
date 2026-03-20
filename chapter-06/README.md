# 第六章：采集与音频 3A 处理

> **本章目标**：实现音视频采集，获取摄像头和麦克风数据，并进行音频 3A（AEC/ANS/AGC）处理。

前五章实现了播放器能力，从本章开始进入**主播端**开发。主播端的核心是**采集**——从硬件设备获取音视频原始数据，处理后推送到服务器。

本章重点：
- 视频采集：摄像头设备访问
- 音频采集：麦克风设备访问
- 音频 3A：回声消除（AEC）、降噪（ANS）、自动增益（AGC）

**阅读指南**：
- 第 1-3 节：视频采集原理和实现（V4L2/AVFoundation）
- 第 4-6 节：音频采集和 3A 处理
- 第 7-8 节：采集参数配置、性能优化

---

## 目录

1. [为什么需要采集：从播放到直播](#1-为什么需要采集从播放到直播)
2. [视频采集原理](#2-视频采集原理)
3. [跨平台视频采集实现](#3-跨平台视频采集实现)
4. [音频采集基础](#4-音频采集基础)
5. [音频 3A 处理](#5-音频-3a-处理)
6. [WebRTC APM 集成](#6-webrtc-apm-集成)
7. [采集参数与优化](#7-采集参数与优化)
8. [本章总结](#8-本章总结)

---

## 1. 为什么需要采集：从播放到直播

**本节概览**：回顾前五章内容，理解采集在直播系统中的位置和重要性。

### 1.1 前五章回顾

```
第一章：本地播放器 ──→ 理解 Pipeline
第二章：异步架构   ──→ 多线程优化
第三章：网络基础   ──→ HTTP/下载
第四章：RTMP 协议  ──→ 直播拉流
第五章：硬件解码   ──→ 高性能播放

共同主题：播放器（播放端）
```

### 1.2 直播系统的另一端：主播端

```
┌─────────────────────────────────────────────────────────────┐
│                        直播系统架构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   主播端                           服务器        观众端      │
│  ┌──────────┐                   ┌─────────┐   ┌──────────┐ │
│  │ 采集设备 │──采集──→│ 接收    │   │ 播放器   │ │
│  │ 摄像头   │                   │ 转码    │←──│ (已实现) │ │
│  │ 麦克风   │──编码──→│ 分发    │   └──────────┘ │
│  └──────────┘      推流         └─────────┘                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 采集的挑战

| 挑战 | 说明 |
|:---|:---|
| **设备兼容性** | 不同品牌摄像头参数差异大 |
| **时序同步** | 音视频需要精确对齐（< 40ms）|
| **音频质量** | 回声、噪声、音量不均 |
| **资源占用** | 采集+编码同时运行，CPU 压力大 |

**本节小结**：采集是主播端的第一步，面临设备兼容、时序同步、音频质量等挑战。下一节介绍视频采集原理。

---

## 2. 视频采集原理

**本节概览**：介绍摄像头的工作原理、常用格式、以及帧率控制。

### 2.1 摄像头工作流程

```
光学镜头 → 图像传感器(CMOS/CCD) → ISP处理 → 数字图像
                              ↓
                         白平衡、曝光、降噪
                              ↓
                         YUV/RGB 输出
```

### 2.2 常用像素格式

| 格式 | 说明 | 用途 |
|:---|:---|:---|
| **YUY2** | YUV 4:2:2 打包 | 传统摄像头 |
| **NV12** | YUV 4:2:0 半平面 | 现代摄像头、硬件友好 |
| **MJPEG** | 压缩格式 | 高帧率高分辨率 |
| **H.264** | 编码格式 | 部分摄像头直接输出 |

### 2.3 帧率与分辨率

**帧率选择**：
- 15fps：节省带宽，适合屏幕共享
- 30fps：**标准选择**，流畅自然
- 60fps：游戏直播，丝滑体验

**分辨率选择**：
- 640x480：省流量，移动端
- 1280x720：**标准清晰度**
- 1920x1080：高清直播

**本节小结**：摄像头输出 YUV/NV12 格式，需要编码后才能传输。下一节实现跨平台采集。

---

## 3. 跨平台视频采集实现

**本节概览**：使用 FFmpeg 的 libavdevice 实现跨平台视频采集。

### 3.1 FFmpeg 设备采集

FFmpeg 封装了各平台的设备访问接口：
- Linux：Video4Linux2 (V4L2)
- macOS：AVFoundation
- Windows：DirectShow

### 3.2 打开摄像头

```cpp
#include <libavdevice/avdevice.h>

// 注册设备
avdevice_register_all();

// 打开摄像头
AVFormatContext* ctx = nullptr;
const char* device_name = "0";  // 设备索引

#if defined(__APPLE__)
    const AVInputFormat* iformat = av_find_input_format("avfoundation");
    // macOS: "0" 表示默认摄像头
#elif defined(__linux__)
    const AVInputFormat* iformat = av_find_input_format("v4l2");
    // Linux: "/dev/video0"
    device_name = "/dev/video0";
#endif

AVDictionary* options = nullptr;
av_dict_set(&options, "video_size", "1280x720", 0);
av_dict_set(&options, "framerate", "30", 0);
av_dict_set(&options, "pixel_format", "nv12", 0);

int ret = avformat_open_input(&ctx, device_name, iformat, &options);
if (ret < 0) {
    // 错误处理
}
```

### 3.3 读取视频帧

```cpp
// 查找视频流
avformat_find_stream_info(ctx, nullptr);
int video_idx = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);

// 读取帧
AVPacket* pkt = av_packet_alloc();
while (av_read_frame(ctx, pkt) >= 0) {
    if (pkt->stream_index == video_idx) {
        // 处理视频帧
        // pkt->data 可能是 YUV 或 MJPEG
    }
    av_packet_unref(pkt);
}
```

**本节小结**：FFmpeg 的 libavdevice 提供了统一的设备采集接口。下一节介绍音频采集。

---

## 4. 音频采集基础

**本节概览**：介绍音频采集的基本概念：采样率、位深、声道数。

### 4.1 音频参数

| 参数 | 常见值 | 说明 |
|:---|:---|:---|
| **采样率** | 44100 Hz, 48000 Hz | 每秒采样次数 |
| **位深** | 16-bit, 32-bit | 采样精度 |
| **声道数** | 单声道, 立体声 | 1 或 2 |

**数据量计算**：
```
48000 Hz × 16-bit × 2 声道 = 1536 kbps = 192 KB/s
```

### 4.2 音频帧

音频数据以帧为单位：
```
10ms 音频帧 @ 48000Hz = 480 个采样
480 采样 × 2 声道 × 2 字节 = 1920 字节
```

### 4.3 FFmpeg 音频采集

```cpp
// 打开麦克风
const char* audio_device = "default";  // Linux: default
#if defined(__APPLE__)
    audio_device = ":0";  // macOS: 默认音频输入
#endif

AVDictionary* audio_opts = nullptr;
av_dict_set(&audio_opts, "sample_rate", "48000", 0);
av_dict_set(&audio_opts, "channels", "2", 0);

AVFormatContext* audio_ctx = nullptr;
avformat_open_input(&audio_ctx, audio_device, iformat, &audio_opts);
```

**本节小结**：音频采集关注采样率、位深、声道数。原始数据量较大，需要编码压缩。下一节介绍音频 3A 处理。

---

## 5. 音频 3A 处理

**本节概览**：介绍音频三大处理技术：回声消除（AEC）、降噪（ANS）、自动增益（AGC）。

### 5.1 回声消除（AEC）

**问题**：扬声器播放的声音被麦克风采集，形成回声。

```
远端语音 → 扬声器 → [房间] → 麦克风 → 回声
              ↑___________________|
```

**原理**：将播放的音频作为参考信号，从麦克风输入中减去。

```cpp
// WebRTC AEC 使用示例
void ProcessAudio(const int16_t* mic,      // 麦克风输入
                  const int16_t* speaker,  // 扬声器参考
                  int16_t* out) {          // 处理后输出
    // AEC 算法消除回声
    aec_process(mic, speaker, out, samples);
}
```

### 5.2 降噪（ANS）

**问题**：环境噪声（键盘声、风扇声）影响语音质量。

**原理**：频谱分析，区分语音和噪声，抑制噪声频段。

```cpp
// 降噪前后对比
降噪前：[语音 + 空调声 + 键盘声]
降噪后：[语音]（空调声和键盘声被抑制）
```

### 5.3 自动增益（AGC）

**问题**：说话声音忽大忽小。

**原理**：自动调整音量到目标范围。

```cpp
// AGC 目标：-3dB ~ -1dB（电平表绿区）
输入音量小 → AGC 放大
输入音量大 → AGC 缩小
```

### 5.4 3A 处理流程

```
麦克风输入
    ↓
┌─────────┐
│   AEC   │  ← 消除扬声器回声
│ 回声消除 │
└────┬────┘
     ↓
┌─────────┐
│   ANS   │  ← 消除环境噪声
│  降噪   │
└────┬────┘
     ↓
┌─────────┐
│   AGC   │  ← 自动调整音量
│ 自动增益 │
└────┬────┘
     ↓
处理后音频 → 编码 → 传输
```

**本节小结**：3A 处理显著提升音频质量，是直播必备。下一节介绍 WebRTC APM 集成。

---

## 6. WebRTC APM 集成

**本节概览**：WebRTC 的 Audio Processing Module (APM) 是业界标准 3A 实现。本节介绍如何集成到项目中。

### 6.1 WebRTC APM 简介

WebRTC APM 包含：
- AEC3（回声消除第 3 代）
- NS（噪声抑制）
- AGC2（自动增益第 2 代）
- VAD（语音检测）

### 6.2 集成步骤

```cpp
#include "modules/audio_processing/include/audio_processing.h"

using namespace webrtc;

class Audio3AProcessor {
public:
    bool Init(int sample_rate, int channels) {
        // 创建 APM
        AudioProcessingBuilder builder;
        apm_ = builder.Create();
        
        // 配置参数
        ProcessingConfig config;
        config.pipeline_components[AEC] = true;
        config.pipeline_components[NS] = true;
        config.pipeline_components[AGC] = true;
        
        apm_>ApplyConfig(config);
        
        // 配置流
        StreamConfig stream_config(sample_rate, channels, false);
        apm_>Initialize(stream_config, stream_config, stream_config);
        
        return true;
    }
    
    void Process(const int16_t* mic,
                 const int16_t* speaker,
                 int16_t* out,
                 size_t samples) {
        // 设置参考信号（扬声器）
        StreamConfig config(48000, 2, false);
        apm_>ProcessReverseStream(speaker, config, config, speaker);
        
        // 处理麦克风信号
        AudioFrame frame;
        frame.UpdateFrame(0, 0, mic, samples, 48000, 2, 2);
        
        apm_>ProcessStream(&frame);
        
        // 复制输出
        memcpy(out, frame.data(), samples * sizeof(int16_t));
    }
    
private:
    std::unique_ptr<AudioProcessing> apm_;
};
```

### 6.3 CMake 配置

```cmake
# 添加 WebRTC APM 库
find_package(WebRTC REQUIRED)

add_executable(capture
    src/main.cpp
    src/audio_3a.cpp
    src/video_capture.cpp
)

target_link_libraries(capture
    ${FFMPEG_LIBRARIES}
    webrtc_apm
    SDL2::SDL2
)
```

**本节小结**：WebRTC APM 提供了工业级 3A 处理能力。下一节介绍采集参数配置。

---

## 7. 采集参数与优化

**本节概览**：介绍采集参数的配置策略，以及性能优化方法。

### 7.1 分辨率与帧率权衡

| 场景 | 分辨率 | 帧率 | 码率 | CPU |
|:---|:---|:---:|:---:|:---:|
| 屏幕共享 | 1920x1080 | 15 | 2Mbps | 低 |
| 标准直播 | 1280x720 | 30 | 4Mbps | 中 |
| 游戏直播 | 1920x1080 | 60 | 8Mbps | 高 |

### 7.2 音视频同步

采集时音视频可能不同步，需要打时间戳：

```cpp
// 获取当前时间（微秒）
int64_t GetTimestamp() {
    return av_gettime();
}

// 视频帧时间戳
AVFrame* video_frame = CaptureVideo();
video_frame->pts = GetTimestamp();

// 音频帧时间戳
AudioFrame* audio_frame = CaptureAudio();
audio_frame->pts = GetTimestamp();
```

### 7.3 性能优化

1. **使用硬件编码**：减轻 CPU 负担
2. **降低预览分辨率**：采集高分辨率，预览低分辨率
3. **异步处理**：3A 和编码在独立线程

**本节小结**：合理配置采集参数，确保音视频同步，使用异步处理优化性能。下一节总结本章。

---

## 8. 本章总结

### 8.1 本章回顾

本章实现了音视频采集：

1. **视频采集**：FFmpeg libavdevice，跨平台支持
2. **音频采集**：采样率 48000Hz，16-bit，双声道
3. **音频 3A**：AEC 消除回声，ANS 降噪，AGC 自动增益
4. **WebRTC APM**：工业级 3A 处理库
5. **参数配置**：分辨率、帧率、码率权衡
6. **同步优化**：时间戳对齐，异步处理

### 8.2 当前能力

```
采集设备 → 原始数据 → 3A处理 → 编码 → 推流
   ↑                                          
摄像头   720p/1080p    AEC/ANS/AGC   H.264   RTMP
麦克风   48kHz/16bit   WebRTC APM    AAC
```

### 8.3 下一步

第七章将实现**视频编码**，将原始 YUV 数据压缩为 H.264。

**第 7 章预告**：
- H.264 编码原理
- x264 编码器使用
- 码率控制（CBR/VBR）
- SVC 可伸缩编码

---

## 附录

### 参考资源

- [FFmpeg Device Documentation](https://ffmpeg.org/ffmpeg-devices.html)
- [WebRTC Audio Processing](https://webrtc.googlesource.com/src/+/refs/heads/main/modules/audio_processing/)
- [Video4Linux2 API](https://www.kernel.org/doc/html/v4.9/media/uapi/v4l/v4l2.html)

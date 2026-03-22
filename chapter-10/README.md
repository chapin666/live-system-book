# 第十章：音视频采集

> **本章目标**：实现音视频采集，获取摄像头和麦克风原始数据。

前九章完成了观众端播放器（本地播放、异步、网络、直播拉流）。从本章开始构建**主播端**。主播端的核心任务是**采集**——从硬件设备获取音视频原始数据。

---

## 目录

1. [从播放到直播：采集的重要性](#1-从播放到直播采集的重要性)
2. [视频采集原理](#2-视频采集原理)
3. [跨平台视频采集实现](#3-跨平台视频采集实现)
4. [音频采集基础](#4-音频采集基础)
5. [音视频同步](#5-音视频同步)
6. [采集参数优化](#6-采集参数优化)
7. [本章总结](#7-本章总结)

---

## 1. 从播放到直播：采集的重要性

### 1.1 直播系统架构

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

### 1.2 采集面临的挑战

| 挑战 | 影响 | 解决方案 |
|:---|:---|:---|
| **设备兼容性** | 不同摄像头参数差异大 | 统一抽象接口 |
| **回声问题** | 扬声器声音被麦克风采集 | AEC 回声消除（见Ch11）|
| **环境噪声** | 键盘声、空调声干扰 | ANS 降噪（见Ch11）|
| **音量不均** | 说话声音忽大忽小 | AGC 自动增益（见Ch11）|
| **音视频同步** | 画面和声音不同步 | 时间戳对齐 |
| **资源占用** | 采集+编码同时运行 | 异步处理 |

---

## 2. 视频采集原理

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

### 2.3 帧率与分辨率

| 场景 | 分辨率 | 帧率 |
|:---|:---|:---:|
| 屏幕共享 | 1920x1080 | 15fps |
| 标准直播 | 1280x720 | 30fps |
| 游戏直播 | 1920x1080 | 60fps |

---

## 3. 跨平台视频采集实现

### 3.1 FFmpeg 设备采集

FFmpeg 封装了各平台的设备访问：
- Linux：Video4Linux2 (V4L2) - `/dev/video0`
- macOS：AVFoundation - `0` (默认摄像头)
- Windows：DirectShow - `video=Camera Name`

### 3.2 打开摄像头

```cpp
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>

// 注册设备
avdevice_register_all();

// 打开摄像头
AVFormatContext* fmt_ctx = nullptr;
AVInputFormat* input_fmt = nullptr;

#ifdef __APPLE__
    input_fmt = av_find_input_format("avfoundation");
    const char* device = "0";  // 默认摄像头
#elif defined(__linux__)
    input_fmt = av_find_input_format("v4l2");
    const char* device = "/dev/video0";
#endif

AVDictionary* opts = nullptr;
av_dict_set(&opts, "video_size", "1280x720", 0);
av_dict_set(&opts, "framerate", "30", 0);
av_dict_set(&opts, "pixel_format", "nv12", 0);

int ret = avformat_open_input(&fmt_ctx, device, input_fmt, &opts);
av_dict_free(&opts);
```

### 3.3 读取视频帧

```cpp
AVPacket* packet = av_packet_alloc();

while (av_read_frame(fmt_ctx, packet) >= 0) {
    // packet-data 包含原始视频帧
    // 可以是 YUV、NV12、MJPEG 等格式
    
    av_packet_unref(packet);
}

av_packet_free(&packet);
avformat_close_input(&fmt_ctx);
```

---

## 4. 音频采集基础

### 4.1 音频参数

| 参数 | 常用值 | 说明 |
|:---|:---:|:---|
| 采样率 | 44100/48000 Hz | CD 质量/专业音频 |
| 位深 | 16-bit | 每个样本 2 字节 |
| 声道 | 1/2 | 单声道/立体声 |
| 帧长 | 10-20ms | 每帧样本数 |

### 4.2 FFmpeg 音频采集

```cpp
// macOS
const char* audio_device = ":0";  // 默认麦克风

av_dict_set(&opts, "sample_rate", "48000", 0);
av_dict_set(&opts, "channels", "2", 0);

ret = avformat_open_input(&fmt_ctx, audio_device, input_fmt, &opts);
```

---

## 5. 音视频同步

### 5.1 时间戳方案

```cpp
class AVSynchronizer {
public:
    int64_t GetCurrentTime() {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000000LL + tv.tv_usec;
    }
    
    void TimestampFrame(AVFrame* frame) {
        frame->pts = GetCurrentTime();
    }
    
    bool CheckSync(int64_t video_pts, int64_t audio_pts) {
        int64_t diff = video_pts - audio_pts;
        return std::abs(diff) < 40000;  // < 40ms
    }
};
```

---

## 6. 采集参数优化

### 6.1 分辨率选择

| 网络条件 | 分辨率 | 帧率 | 码率 |
|:---|:---|:---:|:---:|
| 良好 | 1280x720 | 30fps | 2.5 Mbps |
| 一般 | 854x480 | 30fps | 1.0 Mbps |
| 较差 | 640x360 | 24fps | 500 Kbps |

### 6.2 性能优化

- **零拷贝**：直接使用采集缓冲区，避免内存复制
- **GPU 采集**：使用 VideoToolbox/VAAPI 硬件采集
- **异步处理**：采集和编码分离到不同线程

---

## 7. 本章总结

### 核心概念

1. **视频采集**：FFmpeg libavdevice 跨平台封装
2. **音频采集**：采样率、位深、声道配置
3. **同步**：时间戳对齐，确保音视频同步

### 下一步

采集的原始数据需要**音频 3A 处理**（回声消除、降噪、自动增益）：

> **下一章：第十一章 - 音频 3A 处理**
> - AEC：消除扬声器回声
> - ANS：抑制环境噪声  
> - AGC：自动音量均衡

处理后的音视频将送入编码器，开始 RTMP 推流。

---

**本章代码**：完整实现见 `src/video_capture.cpp` 和 `src/audio_capture.cpp`

# 第一章：视频基础理论

> **本章目标**：理解视频播放的完整链路——从文件中的压缩数据，到屏幕上的清晰画面。

在开始编写代码之前，我们需要先理解几个根本问题：**为什么视频能压缩？压缩后的数据是什么样的？如何把这些数据还原成图像？** 本章将带你从零开始，理解视频技术的核心概念。

**阅读指南**：
- 第 1-2 节：视频压缩原理，建立直观感受
- 第 3-5 节：颜色空间、FFmpeg 架构、SDL2 渲染
- 第 6-7 节：调试技巧和常见问题

---

## 目录

1. [视频压缩原理：为什么 1 分钟视频只有 100MB](#1-视频压缩原理为什么-1-分钟视频只有-100mb)
2. [颜色空间：YUV 与 RGB 的区别](#2-颜色空间yuv-与-rgb-的区别)
3. [FFmpeg 架构：核心数据结构详解](#3-ffmpeg-架构核心数据结构详解)
4. [SDL2 渲染：从像素到屏幕](#4-sdl2-渲染从像素到屏幕)
5. [调试技巧：排查问题](#5-调试技巧排查问题)
6. [常见问题](#6-常见问题)
7. [本章总结与下一步](#7-本章总结与下一步)

---

## 1. 视频压缩原理：为什么 1 分钟视频只有 100MB

**本节概览**：为什么 1 分钟的 1080p 视频只需要 100MB，而不压缩的话需要 10GB？这一节将揭示视频压缩的三个核心技巧。

### 1.1 原始视频有多大

让我们先算一笔账。1080p 视频每帧有 1920×1080 = 2073600 个像素，如果每个像素用 3 字节（RGB）表示：

| 分辨率 | 每帧大小 | 1 秒 (30fps) | 1 分钟 | 1 小时 |
|:---|:---|:---|:---|:---|
| 1280×720 | 2.8 MB | 84 MB | **5.0 GB** | 300 GB |
| 1920×1080 | 6.2 MB | 186 MB | **11.2 GB** | 672 GB |
| 3840×2160 | 24.9 MB | 747 MB | **44.8 GB** | 2.7 TB |

**实际 1 分钟 1080p 视频约 100 MB**，压缩了 **100 倍以上**！

这是怎么做到的？视频数据中存在三种"冗余"，压缩就是去除这些冗余。

### 1.2 冗余一：空间冗余（帧内压缩）

**现象**：一张照片中，相邻的像素通常很相似。比如一片蓝天，像素值可能是 200, 201, 200, 199, 201... 变化不大。

**压缩思路**：不直接存储每个像素的值，而是存储"差值"。

```
原始数据：200, 201, 200, 199, 201, 202, 200, 199 （8 字节）
差分编码：200（基准）, +1, -1, -1, +2, +1, -2, -1

差值范围小（-2 到 +2），可以用更少的位存储
```

**核心算法：DCT 变换**

JPEG 和视频压缩都使用 DCT（离散余弦变换）。它的核心思想是：

```
把图像从"空间域"（像素值）转换到"频率域"（变化快慢）

- 低频 = 缓慢变化的区域（天空、墙面）→ 保留
- 高频 = 快速变化的区域（边缘、纹理）→ 适当丢弃
```

人眼对高频细节不敏感，因此可以丢弃部分高频信息，大幅减小数据量。

### 1.3 冗余二：时间冗余（帧间压缩）

**现象**：视频中连续帧之间变化很小。30fps 的视频，相邻帧只间隔 33 毫秒，画面通常只有微小变化。

**压缩思路**：不存储完整画面，只存储"变化的部分"。

```
第 1 帧：存储完整画面（关键帧）      50 KB
第 2 帧：只存储与第 1 帧的差异       10 KB
第 3 帧：只存储与第 2 帧的差异       8 KB
...
```

**帧类型设计**

为了高效利用时间冗余，视频编码定义了三种帧类型：

<img src="docs/images/frame-types.svg" width="100%"/>

| 类型 | 名称 | 大小 | 说明 |
|:---|:---|:---|:---|
| **I 帧** | 关键帧 | 40-60 KB | 完整编码，可独立解码 |
| **P 帧** | 预测帧 | 8-15 KB | 参考前一帧，只存变化 |
| **B 帧** | 双向帧 | 3-8 KB | 参考前后两帧，压缩率最高 |

**GOP 结构**

两个 I 帧之间的帧序列称为 GOP（Group of Pictures）：

```
时间线：  0ms   33ms   66ms   100ms  133ms  166ms  200ms
帧类型：   I     P      B      B      P      B      I
大小：    50K   12K    5K     4K     10K    5K    48K
```

**为什么需要 I 帧？**

如果只是 P/B 帧链，快进时会遇到问题——你要从最近的 I 帧开始解码才能看到画面。因此 I 帧间隔通常为 1-2 秒，平衡压缩率和随机访问性能。

### 1.4 冗余三：视觉冗余（色度子采样）

**人眼特性**：视网膜上的感光细胞（感知明暗）比感色细胞多得多。我们对亮度变化敏感，但对颜色变化不太敏感。

**YUV 颜色空间**

视频使用 YUV 而不是 RGB：
- **Y（Luma）**：亮度，决定明暗
- **U（Cb）**：蓝色色度
- **V（Cr）**：红色色度

**4:2:0 采样**：每 4 个像素共享 1 个 U 和 1 个 V

```
RGB：每个像素 3 字节
YUV420：平均每个像素 1.5 字节（省 50%）
```

### 1.5 压缩效果实测

理论讲完了，让我们看看实际的压缩效果。以 1920×1080 30fps 的视频为例（1 秒 = 30 帧）：

| 帧序列 | 类型 | 大小 | 累计 | 压缩来源 |
|:---|:---|:---|:---|:---|
| 帧 0 | I 帧 | 50 KB | 50 KB | DCT + 量化 |
| 帧 1 | P 帧 | 12 KB | 62 KB | 运动估计（参考帧 0）|
| 帧 2 | B 帧 | 5 KB | 67 KB | 双向预测（参考帧 0,4）|
| 帧 3 | B 帧 | 4 KB | 71 KB | 双向预测（参考帧 0,4）|
| 帧 4 | P 帧 | 10 KB | 81 KB | 运动估计（参考帧 1）|
| 帧 5 | B 帧 | 6 KB | 87 KB | 双向预测 |
| ... | ... | ... | ... | ... |
| 帧 29 | P 帧 | 11 KB | ~320 KB | 运动估计 |

**数据对比**：

| 指标 | 原始数据 | 压缩后 | 压缩率 |
|:---|:---|:---|:---|
| 单帧大小 | 6.2 MB | 10.7 KB（平均）| 99.8% |
| 1 秒数据（30fps）| 186 MB | ~320 KB | 99.8% |
| 1 分钟数据 | 11.2 GB | ~19 MB | 99.8% |
| 1 小时数据 | 672 GB | ~1.1 GB | 99.8% |

**各种压缩技术的贡献**：

```
原始数据：6.2 MB/帧
├── 空间冗余压缩（DCT+量化）：→ 约 300 KB（压缩 95%）
├── 时间冗余压缩（帧间预测）：→ 平均 10-50 KB（再压缩 80%）
└── 视觉冗余压缩（YUV420）：→ 平均 1.5 字节/像素（再压缩 50%）

最终：约 10 KB/帧，压缩率 99.8%
```

**实际观察**：
你可以用以下命令查看真实视频的帧类型分布：

```bash
# 查看每帧的类型和大小
ffprobe -v error -select_streams v:0 -show_frames \
    -show_entries frame=pkt_size,pict_type \
    -of csv test.mp4 | head -20

# 输出示例：
# frame,50000,I
# frame,12000,P
# frame,5000,B
# ...
```

**本节小结**：视频压缩利用三种冗余——空间冗余（DCT）、时间冗余（帧间预测）、视觉冗余（YUV 子采样）。实测显示压缩率可达 99.8%，让 11GB/分钟的原始数据减少到 19MB。

---

## 2. 颜色空间：YUV 与 RGB 的区别

**本节概览**：上一节提到视频使用 YUV 格式，这一节将详细解释 YUV 的内存布局，以及 FFmpeg 中如何处理像素数据。

### 2.1 为什么视频用 YUV

| 特性 | RGB | YUV |
|:---|:---|:---|
| 存储 | 3 字节/像素 | 1.5 字节/像素（YUV420）|
| 黑白兼容 | 需特殊处理 | Y 通道直接可用 |
| 压缩友好 | 不友好 | 可降采样 U/V |
| 用途 | 图像处理、游戏 | 视频编解码 |

### 2.2 YUV420 内存布局

<img src="docs/images/yuv-layout-new.svg" width="90%"/>

以 1920×1080 为例：

| 平面 | 分辨率 | 大小 | 占比 |
|:---|:---|:---|:---|
| Y | 1920 × 1080 | 2,073,600 B | 66.7% |
| U | 960 × 540 | 518,400 B | 16.7% |
| V | 960 × 540 | 518,400 B | 16.7% |
| **总计** | - | **3,110,400 B** | - |

对比 RGB：1920 × 1080 × 3 = **6,220,800 B**

**YUV420 比 RGB 节省 50% 空间**。

### 2.3 YUV 格式家族对比

视频处理中常见的 YUV 格式：

| 格式 | 采样方式 | Y:U:V | 字节/像素 | 特点 | 应用场景 |
|:---|:---|:---:|:---:|:---|:---|
| **YUV444** | 4:4:4 | 4:4:4 | 3.0 | 无压缩，质量最高 | 专业视频制作 |
| **YUV422** | 4:2:2 | 4:2:2 | 2.0 | 水平方向减半 | 广播级视频 |
| **YUV420** | 4:2:0 | 4:1:1 | 1.5 | 水平和垂直都减半 | **最常用**，H.264/H.265 |
| **NV12** | 4:2:0 | - | 1.5 | Y 平面 + UV 交错 | DirectX、Android 相机 |
| **NV21** | 4:2:0 | - | 1.5 | Y 平面 + VU 交错 | Android 相机默认 |

**平面格式 vs 打包格式**：

```
平面格式（YUV420P）：
┌──────────────┐
│ Y 平面       │
├──────────────┤
│ U 平面       │
├──────────────┤
│ V 平面       │
└──────────────┘
FFmpeg 默认，易于处理

打包格式（NV12）：
┌──────────────┐
│ Y 平面       │
├──────────────┤
│ UVUVUV...    │  ← U 和 V 交错存储
└──────────────┘
硬件友好，GPU 处理更快
```

### 2.4 YUV ↔ RGB 转换

有时需要将 YUV 转换为 RGB（如截图保存为 PNG）：

**转换公式**（BT.601 标准）：
```
R = Y + 1.402 * (V - 128)
G = Y - 0.344 * (U - 128) - 0.714 * (V - 128)
B = Y + 1.772 * (U - 128)
```

**FFmpeg 代码实现**：

```cpp
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// YUV420P 转 RGB24
void YUV420ToRGB(AVFrame* yuvFrame, uint8_t** rgbData, int* rgbLinesize) {
    int width = yuvFrame->width;
    int height = yuvFrame->height;
    
    // 创建转换上下文
    SwsContext* swsCtx = sws_getContext(
        width, height, AV_PIX_FMT_YUV420P,    // 源格式
        width, height, AV_PIX_FMT_RGB24,      // 目标格式
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    // 分配 RGB 缓冲区
    av_image_alloc(rgbData, rgbLinesize, width, height, 
                   AV_PIX_FMT_RGB24, 32);
    
    // 执行转换
    sws_scale(swsCtx, yuvFrame->data, yuvFrame->linesize,
              0, height, rgbData, rgbLinesize);
    
    // 清理
    sws_freeContext(swsCtx);
}
```

### 2.5 FFmpeg 中的像素访问

解码后的图像存储在 `AVFrame` 结构中：

```cpp
AVFrame* frame = av_frame_alloc();
// ... 解码 ...

// 访问 YUV 数据
uint8_t* y_data = frame->data[0];  // Y 平面指针
uint8_t* u_data = frame->data[1];  // U 平面指针
uint8_t* v_data = frame->data[2];  // V 平面指针

int y_stride = frame->linesize[0];  // Y 行宽（含对齐填充）
int u_stride = frame->linesize[1];  // U 行宽
int v_stride = frame->linesize[2];  // V 行宽
```

⚠️ **重要**：`linesize` 可能大于 `width`，因为某些 CPU 要求内存对齐。

```cpp
// 错误：可能读到填充区域
uint8_t y = frame->data[0][y * frame->width + x];

// 正确：使用 linesize
uint8_t y = frame->data[0][y * frame->linesize[0] + x];
```

**本节小结**：YUV420 通过降低色度分辨率节省空间，解码后的数据通过 `AVFrame` 的 `data` 和 `linesize` 访问。

---

## 3. FFmpeg 架构：核心数据结构详解

**本节概览**：前面我们了解了视频压缩的原理和像素格式，现在来看看 FFmpeg 如何用代码组织这些概念。

### 3.1 FFmpeg 库结构

```
┌─────────────────────────────────────────────┐
│  libavformat - 封装/解封装（MP4、FLV 等）     │
├─────────────────────────────────────────────┤
│  libavcodec - 编解码（H.264、H.265 等）       │
├─────────────────────────────────────────────┤
│  libavutil - 工具函数（内存、数学、时间）     │
├─────────────────────────────────────────────┤
│  libswscale - 图像转换（YUV ↔ RGB）           │
└─────────────────────────────────────────────┘
```

### 3.2 四大核心结构体

FFmpeg 的设计哲学是"分层抽象"，每个结构体负责一个层次：

#### AVFormatContext - 文件层

代表一个打开的文件或流，管理所有流的元信息：

```cpp
typedef struct AVFormatContext {
    unsigned int nb_streams;      // 流数量（可能有视频+音频+字幕）
    AVStream** streams;           // 流数组
    int64_t duration;             // 总时长（微秒）
    int64_t bit_rate;             // 总码率
} AVFormatContext;
```

#### AVStream - 流层

代表文件中的一个流（视频流、音频流等）：

```cpp
typedef struct AVStream {
    AVCodecParameters* codecpar;  // 编解码器参数（分辨率、码率等）
    AVRational time_base;         // 时间基（PTS 的单位）
    int64_t duration;             // 流时长
} AVStream;
```

**时间基**是理解时间戳的关键：

```cpp
AVRational tb = stream->time_base;  // 如 {1, 1000} = 毫秒
int64_t pts = 33000;                // 33 秒

// 转换为秒
double seconds = pts * av_q2d(tb);  // 33.0
```

#### AVCodecContext - 编解码层

编解码器的状态和配置：

```cpp
typedef struct AVCodecContext {
    int width, height;            // 分辨率
    AVPixelFormat pix_fmt;        // 像素格式（YUV420P 等）
    int thread_count;             // 解码线程数
} AVCodecContext;
```

#### AVPacket / AVFrame - 数据层

- **AVPacket**：压缩后的数据（从文件读取）
- **AVFrame**：解码后的原始图像

```cpp
typedef struct AVPacket {
    int64_t pts;                  // 显示时间戳
    int64_t dts;                  // 解码时间戳
    uint8_t* data;                // 压缩数据
    int size;                     // 数据大小
} AVPacket;

typedef struct AVFrame {
    uint8_t* data[8];             // 像素数据指针（Y/U/V）
    int linesize[8];              // 行宽
    int64_t pts;                  // 显示时间戳
} AVFrame;
```

**数据流总结**：

```
文件 → AVFormatContext → AVStream → AVPacket → AVCodecContext → AVFrame
       (文件层)          (流层)     (压缩数据)   (编解码层)     (原始图像)
```

### 3.3 错误处理最佳实践

FFmpeg 的函数大多返回整数表示状态，负数表示错误。正确处理错误是写出健壮代码的关键。

**错误码处理模式**：

```cpp
// 1. 基本错误检查
int ret = avcodec_send_packet(ctx, pkt);
if (ret < 0) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(ret, errbuf, sizeof(errbuf));
    fprintf(stderr, "错误: %s\n", errbuf);
    return -1;
}

// 2. 宏简化（推荐）
#define CHECK_ERROR(ret, msg) \
    do { if ((ret) < 0) { \
        char errbuf[256]; av_strerror((ret), errbuf, sizeof(errbuf)); \
        fprintf(stderr, "%s: %s\n", msg, errbuf); \
        return -1; \
    } } while(0)

// 使用
CHECK_ERROR(avcodec_send_packet(ctx, pkt), "发送 packet 失败");
```

**常见错误码对照表**：

| 错误码 | 宏定义 | 含义 | 常见原因 | 处理方式 |
|:---|:---|:---|:---|:---|
| `-11` | `AVERROR(EAGAIN)` | 资源暂不可用 | 缓冲区满，需先接收帧 | 先 `avcodec_receive_frame` |
| `-12` | `AVERROR(ENOMEM)` | 内存不足 | 分配失败 | 释放资源重试或退出 |
| `-1094995529` | `AVERROR_INVALIDDATA` | 数据无效 | 文件损坏或格式错误 | 跳过或终止 |
| `-1414092869` | `AVERROR_EOF` | 文件结束 | 正常结束 | 退出解码循环 |
| `-2` | `AVERROR(ENOENT)` | 文件不存在 | 路径错误 | 检查路径 |
| `-13` | `AVERROR(EACCES)` | 权限不足 | 无读取权限 | 检查权限 |

**特殊错误处理：EAGAIN**

`EAGAIN` 不是真正的错误，表示需要更多数据或需要先取出数据：

```cpp
// 发送 packet
ret = avcodec_send_packet(ctx, pkt);
if (ret == AVERROR(EAGAIN)) {
    // 解码器缓冲区满，需要先接收帧
    while (avcodec_receive_frame(ctx, frame) == 0) {
        // 处理帧
    }
    // 再次尝试发送
    ret = avcodec_send_packet(ctx, pkt);
}

// 刷新解码器（文件结尾时）
avcodec_send_packet(ctx, nullptr);  // 发送空 packet
while (avcodec_receive_frame(ctx, frame) == 0) {
    // 取出所有缓冲的帧
}
```

**本节小结**：FFmpeg 通过分层结构体管理视频数据，错误处理使用返回值检查，`av_strerror` 可转换错误码为可读信息。

---

## 4. SDL2 渲染：从像素到屏幕

**本节概览**：上一节我们得到了解码后的 YUV 数据（`AVFrame`），但这一堆数字如何变成屏幕上的画面？这一节介绍 SDL2 的渲染机制。

### 4.1 SDL2 三层架构

SDL2 提供三层抽象，将像素数据呈现到屏幕上：

```
┌─────────────────────────────────────────────┐
│  SDL_Window  ──→  窗口（标题栏、边框）       │
│       ↓                                     │
│  SDL_Renderer ──→ 渲染器（GPU/CPU 加速）    │
│       ↓                                     │
│  SDL_Texture ──→ 纹理（显存中的图像）       │
│       ↓                                     │
│  显示器                                      │
└─────────────────────────────────────────────┘
```

### 4.2 关键概念

| 概念 | 说明 | 用途 |
|:---|:---|:---|
| **渲染器** | GPU 或 CPU 负责绘制 | `SDL_RENDERER_ACCELERATED` |
| **纹理** | 显存中的图像数据 | `SDL_TEXTUREACCESS_STREAMING` |
| **VSync** | 垂直同步，防止画面撕裂 | `SDL_RENDERER_PRESENTVSYNC` |

### 4.3 YUV 纹理上传

SDL2 支持直接上传 YUV 数据，无需转换为 RGB：

```cpp
// 创建 YUV 纹理
SDL_Texture* texture = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_IYUV,           // YUV420 格式
    SDL_TEXTUREACCESS_STREAMING,    // 频繁更新
    width, height);

// 更新纹理（每帧调用）
SDL_UpdateYUVTexture(
    texture, nullptr,
    frame->data[0], frame->linesize[0],  // Y
    frame->data[1], frame->linesize[1],  // U
    frame->data[2], frame->linesize[2]); // V
```

**渲染循环**：

```cpp
SDL_RenderClear(renderer);           // 1. 清空
SDL_RenderCopy(renderer, tex, ...);  // 2. 复制纹理
SDL_RenderPresent(renderer);         // 3. 显示（带 VSync）
```

**本节小结**：SDL2 通过 Window → Renderer → Texture 三层架构将 YUV 数据呈现到屏幕。`SDL_UpdateYUVTexture` 可以直接上传 FFmpeg 解码后的数据，无需格式转换。

---

## 5. 调试技巧：排查问题

**本节概览**：开发过程中难免遇到问题，这一节介绍常用的调试工具和方法。

### 5.1 GDB 调试

```bash
gdb ./player
run test.mp4
bt              # 查看调用栈
print variable  # 打印变量
```

### 5.2 Valgrind 内存检测

```bash
valgrind --leak-check=full ./player test.mp4
```

### 5.3 FFmpeg 专用调试技巧

**查看视频文件信息（ffprobe）**：

```bash
# 查看文件整体信息
ffprobe -v error -show_format -show_streams test.mp4

# 查看视频流的详细信息
ffprobe -v error -select_streams v:0 \
    -show_entries stream=codec_name,width,height,pix_fmt,r_frame_rate,bit_rate \
    -of default=noprint_wrappers=1 test.mp4

# 查看每帧的类型和大小
ffprobe -v error -select_streams v:0 -show_frames \
    -show_entries frame=pkt_size,pict_type,pts \
    -of csv test.mp4 | head -20
```

**提取单帧保存为图片**：

```bash
# 提取第 1 秒的画面
ffmpeg -i test.mp4 -ss 00:00:01 -vframes 1 frame.png

# 提取所有 I 帧
ffmpeg -i test.mp4 -vf "select=eq(pict_type\,I)" -vsync vfr i_frame_%03d.png
```

**FFmpeg 日志级别**：

```cpp
// 设置 FFmpeg 日志级别
av_log_set_level(AV_LOG_DEBUG);  // DEBUG, INFO, WARNING, ERROR

// 级别说明：
// AV_LOG_QUIET   - 不输出
// AV_LOG_ERROR   - 仅错误
// AV_LOG_WARNING - 警告和错误
// AV_LOG_INFO    - 普通信息（默认）
// AV_LOG_DEBUG   - 调试信息
// AV_LOG_TRACE   - 最详细
```

**解码过程调试**：

```cpp
// 打印 packet 信息
printf("Packet: pts=%lld, dts=%lld, size=%d, flags=%d\n",
       packet->pts, packet->dts, packet->size, packet->flags);

// 判断是否为关键帧
if (packet->flags & AV_PKT_FLAG_KEY) {
    printf("关键帧\n");
}

// 打印 frame 信息
printf("Frame: pts=%lld, format=%s, %dx%d\n",
       frame->pts,
       av_get_pix_fmt_name((AVPixelFormat)frame->format),
       frame->width, frame->height);
```

**本节小结**：掌握调试工具可以快速定位问题。ffprobe 和 ffmpeg 命令行工具是分析视频文件的利器。

---

## 6. 常见问题

### Q1: 编译报错 `undefined reference to`

```bash
# 检查 pkg-config
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### Q2: 画面是绿色的

检查 YUV 格式是否匹配：
```cpp
printf("格式: %s\n", av_get_pix_fmt_name((AVPixelFormat)frame->format));
```

### Q3: 某些视频无法播放

```cpp
// 检查是否找到解码器
const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
if (!codec) {
    printf("不支持的编解码器: %s\n", 
           avcodec_get_name(stream->codecpar->codec_id));
}
```

### Q4: 花屏/马赛克画面

| 现象 | 原因 | 解决 |
|:---|:---|:---|
| 规律性花屏 | 丢包或文件损坏 | 检查文件完整性 |
| 底部绿线 | linesize 理解错误 | 使用 frame->linesize 而非 width |

---

## 7. 本章总结与下一步

### 7.1 本章回顾

本章我们学习了视频播放的理论基础：

1. **为什么能压缩**：利用空间冗余（DCT）、时间冗余（帧间预测）、视觉冗余（YUV）
2. **像素格式**：YUV420 比 RGB 省 50% 空间
3. **FFmpeg 架构**：AVFormatContext → AVStream → AVPacket → AVCodecContext → AVFrame
4. **SDL2 渲染**：Window → Renderer → Texture

### 🎯 本章里程碑

**学完本章，你理解了：**
1. ✅ 视频压缩的基本原理
2. ✅ YUV 颜色空间和内存布局
3. ✅ FFmpeg 核心数据结构
4. ✅ SDL2 渲染机制

### 7.2 下一步：编写第一个播放器

理论知识已经准备就绪，下一章我们将进入实战，用 100 行代码实现一个完整的视频播放器。

**第二章预告**：
- 环境安装（FFmpeg + SDL2）
- 100 行代码实现完整播放器
- 逐行代码详解

💡 **前置要求**：确保你的开发环境已安装 FFmpeg 和 SDL2。

---

## 附录

### 术语表

| 术语 | 解释 |
|:---|:---|
| **FFmpeg** | 开源音视频处理库 |
| **Demuxer** | 解封装器 |
| **Decoder** | 解码器 |
| **YUV** | 亮度+色度像素格式 |
| **YUV420** | 4:2:0 采样 |
| **I/P/B 帧** | 关键帧/预测帧/双向帧 |
| **GOP** | 图像组 |
| **PTS/DTS** | 显示/解码时间戳 |
| **DCT** | 离散余弦变换 |

### 参考资源

- FFmpeg 文档：https://ffmpeg.org/documentation.html
- SDL2 文档：https://wiki.libsdl.org/

---

**下一章**：[第二章：第一个播放器](../chapter-02/)

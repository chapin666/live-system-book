# 第一章：Pipeline 架构与本地播放

> **本章目标**：理解视频播放的完整链路——从文件中的压缩数据，到屏幕上的清晰画面。

在开始编写代码之前，我们需要先理解几个根本问题：**为什么视频能压缩？压缩后的数据是什么样的？如何把这些数据还原成图像？** 本章将带你从零开始，一步步搭建一个完整的视频播放器。

**阅读指南**：
- 第 1-2 节：先跑起来，建立直观感受
- 第 3-5 节：深入原理，理解视频压缩和 FFmpeg 架构
- 第 6-7 节：代码实践，掌握播放器开发
- 第 8-9 节：调试优化，提升工程能力

---

## 目录

1. [快速开始：先跑起来](#1-快速开始先跑起来)
2. [视频压缩原理：为什么 1 分钟视频只有 100MB](#2-视频压缩原理为什么-1-分钟视频只有-100mb)
3. [颜色空间：YUV 与 RGB 的区别](#3-颜色空间yuv-与-rgb-的区别)
4. [FFmpeg 架构：核心数据结构详解](#4-ffmpeg-架构核心数据结构详解)
5. [SDL2 渲染：从像素到屏幕](#5-sdl2-渲染从像素到屏幕)
6. [代码详解：实现一个完整的播放器](#6-代码详解实现一个完整的播放器)
7. [Pipeline 架构：工程化设计](#7-pipeline-架构工程化设计)
8. [性能优化：让播放更流畅](#8-性能优化让播放更流畅)
9. [调试技巧：排查问题](#9-调试技巧排查问题)
10. [常见问题](#10-常见问题)
11. [本章总结与下一步](#11-本章总结与下一步)

---

## 1. 快速开始：先跑起来

**本节概览**：在深入原理之前，让我们先安装环境，运行一个最简单的播放器，建立直观感受。这 100 行代码将是本章的基础，后续所有内容都是围绕它展开。

### 1.1 安装依赖

FFmpeg 和 SDL2 是开发视频应用的两大基石：
- **FFmpeg**：负责音视频的所有底层处理（解封装、解码、滤镜等）
- **SDL2**：负责跨平台的窗口创建和图像渲染

**macOS**（使用 Homebrew）：
```bash
brew install ffmpeg sdl2 cmake
```

**Ubuntu/Debian**：
```bash
sudo apt-get update
sudo apt-get install -y ffmpeg \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev \
    libsdl2-dev cmake pkg-config
```

**为什么用 pkg-config？**

FFmpeg 有很多库文件和头文件路径，手动指定很繁琐。pkg-config 可以自动返回正确的编译参数：
```bash
pkg-config --cflags --libs libavformat libavcodec libavutil sdl2
# 输出：-I/usr/include ... -lavformat -lavcodec -lavutil -lSDL2
```

### 1.2 100 行播放器

这是本章的核心代码，后续的架构设计、性能优化都是围绕它展开：

```cpp
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件>\n", argv[0]);
        return 1;
    }

    // ========== 1. 打开输入文件 ==========
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "无法打开文件: %s\n", errbuf);
        return 1;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "无法获取流信息\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // ========== 2. 查找视频流 ==========
    int video_stream_idx = av_find_best_stream(
        fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx < 0) {
        fprintf(stderr, "未找到视频流\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    AVStream* video_stream = fmt_ctx->streams[video_stream_idx];

    printf("视频信息: %dx%d, 时长: %.2f 秒\n", 
           video_stream->codecpar->width,
           video_stream->codecpar->height,
           fmt_ctx->duration / (double)AV_TIME_BASE);

    // ========== 3. 初始化解码器 ==========
    const AVCodec* codec = avcodec_find_decoder(
        video_stream->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);

    // ========== 4. 创建 SDL2 窗口 ==========
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codec_ctx->width, codec_ctx->height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        codec_ctx->width, codec_ctx->height);

    // ========== 5. 解码循环 ==========
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int64_t start_time = av_gettime();

    while (av_read_frame(fmt_ctx, packet) >= 0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto cleanup;
        }

        if (packet->stream_index == video_stream_idx) {
            avcodec_send_packet(codec_ctx, packet);
            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                // 同步
                int64_t pts_us = frame->pts * av_q2d(video_stream->time_base) * 1000000;
                int64_t elapsed = av_gettime() - start_time;
                if (pts_us > elapsed) av_usleep(pts_us - elapsed);

                // 渲染
                SDL_UpdateYUVTexture(texture, nullptr,
                    frame->data[0], frame->linesize[0],
                    frame->data[1], frame->linesize[1],
                    frame->data[2], frame->linesize[2]);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
        }
        av_packet_unref(packet);
    }

cleanup:
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    SDL_Quit();
    return 0;
}
```

### 1.3 编译运行

```bash
# 创建测试视频
ffmpeg -f lavfi -i testsrc=duration=5:size=640x480:rate=30 \
       -pix_fmt yuv420p test.mp4

# 编译
g++ -std=c++14 -O2 player.cpp -o player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 运行
./player test.mp4
```

**这 100 行代码做了什么？**

简单来说，它完成了视频播放的五个核心步骤：
1. **解封装**：从 MP4 文件中提取压缩的视频数据
2. **解码**：将 H.264 压缩数据还原成 YUV 图像
3. **同步**：根据时间戳控制播放速度
4. **渲染**：将 YUV 图像显示到屏幕上
5. **清理**：释放所有资源

**接下来的问题**：
- 为什么视频能压缩？（第 2 节）
- YUV 是什么？（第 3 节）
- FFmpeg 是怎么组织的？（第 4 节）
- 怎么显示到屏幕上？（第 5 节）

---

## 2. 视频压缩原理：为什么 1 分钟视频只有 100MB

**本节概览**：上一节我们成功播放了视频，但你是否想过——为什么 1 分钟的 1080p 视频只需要 100MB，而不压缩的话需要 10GB？这一节将揭示视频压缩的三个核心技巧，这些技巧也是理解 FFmpeg 解码流程的基础。

### 2.1 原始视频有多大

让我们先算一笔账。1080p 视频每帧有 1920×1080 = 2073600 个像素，如果每个像素用 3 字节（RGB）表示：

| 分辨率 | 每帧大小 | 1 秒 (30fps) | 1 分钟 | 1 小时 |
|:---|:---|:---|:---|:---|
| 1280×720 | 2.8 MB | 84 MB | **5.0 GB** | 300 GB |
| 1920×1080 | 6.2 MB | 186 MB | **11.2 GB** | 672 GB |
| 3840×2160 | 24.9 MB | 747 MB | **44.8 GB** | 2.7 TB |

**实际 1 分钟 1080p 视频约 100 MB**，压缩了 **100 倍以上**！

这是怎么做到的？视频数据中存在三种"冗余"，压缩就是去除这些冗余。

### 2.2 冗余一：空间冗余（帧内压缩）

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

### 2.3 冗余二：时间冗余（帧间压缩）

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

### 2.4 冗余三：视觉冗余（色度子采样）

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

### 2.5 压缩效果实测

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

**本节小结**：视频压缩利用三种冗余——空间冗余（DCT）、时间冗余（帧间预测）、视觉冗余（YUV 子采样）。实测显示压缩率可达 99.8%，让 11GB/分钟的原始数据减少到 19MB。这些概念将在下一节"颜色空间"和第四节"FFmpeg 架构"中继续展开。

---

## 3. 颜色空间：YUV 与 RGB 的区别

**本节概览**：上一节提到视频使用 YUV 格式，这一节将详细解释 YUV 的内存布局，以及 FFmpeg 中如何处理像素数据。这是理解解码后图像数据的关键。

### 3.1 为什么视频用 YUV

| 特性 | RGB | YUV |
|:---|:---|:---|
| 存储 | 3 字节/像素 | 1.5 字节/像素（YUV420）|
| 黑白兼容 | 需特殊处理 | Y 通道直接可用 |
| 压缩友好 | 不友好 | 可降采样 U/V |
| 用途 | 图像处理、游戏 | 视频编解码 |

### 3.2 YUV420 内存布局

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

### 3.3 YUV 格式家族对比

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

### 3.4 YUV ↔ RGB 转换

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

// 保存为 BMP（示例）
void SaveAsBMP(uint8_t* rgbData[], int rgbLinesize[], 
               int width, int height, const char* filename) {
    // BMP 文件头 + 信息头 + 像素数据
    FILE* fp = fopen(filename, "wb");
    if (!fp) return;
    
    int rowSize = ((24 * width + 31) / 32) * 4;  // 4 字节对齐
    int dataSize = rowSize * height;
    
    // BMP 文件头 (14 字节)
    uint8_t fileHeader[14] = {
        'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0
    };
    *(int*)&fileHeader[2] = 54 + dataSize;
    fwrite(fileHeader, 1, 14, fp);
    
    // BMP 信息头 (40 字节)
    uint8_t infoHeader[40] = {0};
    *(int*)&infoHeader[0] = 40;
    *(int*)&infoHeader[4] = width;
    *(int*)&infoHeader[8] = height;
    *(short*)&infoHeader[12] = 1;
    *(short*)&infoHeader[14] = 24;
    *(int*)&infoHeader[20] = dataSize;
    fwrite(infoHeader, 1, 40, fp);
    
    // 像素数据（RGB24 需要翻转行）
    for (int y = height - 1; y >= 0; y--) {
        fwrite(rgbData[0] + y * rgbLinesize[0], 1, rowSize, fp);
    }
    
    fclose(fp);
}
```

### 3.5 FFmpeg 中的像素访问

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

**本节小结**：YUV420 通过降低色度分辨率节省空间，解码后的数据通过 `AVFrame` 的 `data` 和 `linesize` 访问。下一节将介绍 FFmpeg 的核心数据结构。

---

## 4. FFmpeg 架构：核心数据结构详解

**本节概览**：前面我们了解了视频压缩的原理和像素格式，现在来看看 FFmpeg 如何用代码组织这些概念。FFmpeg 的核心是几个结构体，理解它们是掌握视频开发的关键。

### 4.1 FFmpeg 库结构

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

### 4.2 四大核心结构体

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

### 4.3 错误处理最佳实践

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

**资源清理的 RAII 模式**：

C++ 中推荐使用 RAII 管理 FFmpeg 资源，避免泄漏：

```cpp
struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        if (p) av_packet_free(&p);
    }
};
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

// 使用
PacketPtr pkt(av_packet_alloc());
// ... 使用 pkt ...
// 自动释放，即使发生异常
```

**本节小结**：FFmpeg 通过分层结构体管理视频数据，错误处理使用返回值检查，`av_strerror` 可转换错误码为可读信息。理解这些结构体的关系是后续开发的基础。下一节将介绍如何把 `AVFrame` 显示到屏幕上。

---

## 5. SDL2 渲染：从像素到屏幕

**本节概览**：上一节我们得到了解码后的 YUV 数据（`AVFrame`），但这一堆数字如何变成屏幕上的画面？这一节介绍 SDL2 的渲染机制。

### 5.1 SDL2 三层架构

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

### 5.2 关键概念

| 概念 | 说明 | 本章使用 |
|:---|:---|:---|
| **渲染器** | GPU 或 CPU 负责绘制 | `SDL_RENDERER_ACCELERATED` |
| **纹理** | 显存中的图像数据 | `SDL_TEXTUREACCESS_STREAMING` |
| **VSync** | 垂直同步，防止画面撕裂 | `SDL_RENDERER_PRESENTVSYNC` |

### 5.3 YUV 纹理上传

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

**本节小结**：SDL2 通过 Window → Renderer → Texture 三层架构将 YUV 数据呈现到屏幕。`SDL_UpdateYUVTexture` 可以直接上传 FFmpeg 解码后的数据，无需格式转换。下一节将整合所有内容，实现完整播放器。

---

## 6. 代码详解：实现一个完整的播放器

**本节概览**：前面四节分别介绍了压缩原理、像素格式、FFmpeg 架构和 SDL2 渲染。这一节将它们整合起来，逐行分析第 1 节的 100 行代码。

### 6.1 代码流程图

<img src="docs/images/video-pipeline.svg" width="100%"/>

### 6.2 分阶段详解

**阶段 1：打开文件（解封装）**

```cpp
AVFormatContext* fmt_ctx = nullptr;
avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
avformat_find_stream_info(fmt_ctx, nullptr);
```

- `avformat_open_input`：检测文件格式，初始化解封装器
- `avformat_find_stream_info`：读取文件头，获取流信息

**阶段 2：查找视频流**

```cpp
int idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
AVStream* st = fmt_ctx->streams[idx];
```

文件可能有多个流（视频+音频+字幕），这行找到"最好的"视频流。

**阶段 3：初始化解码器**

```cpp
const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
AVCodecContext* cc = avcodec_alloc_context3(codec);
avcodec_parameters_to_context(cc, st->codecpar);
avcodec_open2(cc, codec, nullptr);
```

| 函数 | 作用 |
|:---|:---|
| `avcodec_find_decoder` | 根据 codec_id 找到解码器 |
| `avcodec_alloc_context3` | 创建解码器上下文 |
| `avcodec_open2` | 打开解码器，初始化内部状态 |

**阶段 4：创建窗口**

```cpp
SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow(...);
SDL_Renderer* rend = SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED);
SDL_Texture* tex = SDL_CreateTexture(..., SDL_PIXELFORMAT_IYUV, ...);
```

**阶段 5：解码循环**

```cpp
while (av_read_frame(fmt_ctx, pkt) >= 0) {      // 读取压缩数据
    avcodec_send_packet(cc, pkt);               // 送入解码器
    while (avcodec_receive_frame(cc, frm) == 0) {  // 获取解码后的帧
        // 同步和渲染
    }
}
```

**阶段 6：清理资源**

```cpp
av_frame_free(&frm);
av_packet_free(&pkt);
avcodec_free_context(&cc);
avformat_close_input(&fmt_ctx);
SDL_Quit();
```

### 6.3 关键 API 详解

为了让代码更清晰，以下是主要 API 的详细参数说明：

#### avformat_open_input

```cpp
int avformat_open_input(
    AVFormatContext **ps,      // 输出：格式上下文指针的地址
    const char *url,           // 输入：文件路径或 URL
    AVInputFormat *fmt,        // 输入：指定格式（nullptr 表示自动检测）
    AVDictionary **options     // 输入：额外选项
);
```

**返回值**：0 表示成功，负数表示错误码。

**示例：设置超时和网络选项**：
```cpp
AVDictionary* opts = nullptr;
av_dict_set(&opts, "timeout", "5000000", 0);       // 5 秒超时（微秒）
av_dict_set(&opts, "rtsp_transport", "tcp", 0);    // RTSP 使用 TCP

ret = avformat_open_input(&ctx, "rtsp://...", nullptr, &opts);
av_dict_free(&opts);
CHECK_ERROR(ret, "打开文件失败");
```

#### avcodec_find_decoder / avcodec_find_decoder_by_name

```cpp
// 根据 codec_id 查找（推荐）
const AVCodec* codec = avcodec_find_decoder(codec_id);

// 根据名称查找（用于硬件解码）
const AVCodec* codec = avcodec_find_decoder_by_name("h264_videotoolbox");
// macOS: "h264_videotoolbox"
// Linux VAAPI: "h264_vaapi"
// NVIDIA: "h264_nvdec"
```

#### avcodec_send_packet / avcodec_receive_frame

这两个函数实现了**异步解码**模式：

```cpp
// 发送压缩数据到解码器
// packet 可以为 nullptr，表示刷新缓冲区（文件结尾时）
int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt);

// 接收解码后的帧
// 返回值：
//   0：成功获取一帧
//   AVERROR(EAGAIN)：需要更多输入数据
//   AVERROR_EOF：解码器已刷新完毕
int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);
```

**典型使用模式**：
```cpp
// 1. 发送 packet
ret = avcodec_send_packet(ctx, pkt);
if (ret < 0 && ret != AVERROR(EAGAIN)) {
    // 真正的错误
    return -1;
}

// 2. 循环接收所有可用的帧（一个 packet 可能产生多个 frame）
while (ret >= 0) {
    ret = avcodec_receive_frame(ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
    if (ret < 0) return -1;  // 错误
    
    // 处理 frame
    render(frame);
}

// 3. 文件结尾时刷新解码器
avcodec_send_packet(ctx, nullptr);  // 发送空 packet
while (avcodec_receive_frame(ctx, frame) == 0) {
    render(frame);
}
```

#### SDL_UpdateYUVTexture

```cpp
int SDL_UpdateYUVTexture(
    SDL_Texture *texture,      // 目标纹理
    const SDL_Rect *rect,      // 更新区域（nullptr 表示整个纹理）
    const Uint8 *Yplane,       // Y 平面数据指针
    int Ypitch,                // Y 行宽（linesize）
    const Uint8 *Uplane,       // U 平面数据指针
    int Upitch,                // U 行宽
    const Uint8 *Vplane,       // V 平面数据指针
    int Vpitch                 // V 行宽
);
```

**性能提示**：如果只需要更新部分区域（如局部刷新），可以指定 `rect` 参数，减少数据传输。

#### av_gettime 与同步

```cpp
// 获取当前时间（微秒）
int64_t av_gettime(void);

// 休眠（微秒）
void av_usleep(unsigned int usec);
```

**同步示例**：
```cpp
int64_t start_time = av_gettime();

// 解码循环中
int64_t pts_us = frame->pts * av_q2d(stream->time_base) * 1000000;
int64_t elapsed = av_gettime() - start_time;

if (pts_us > elapsed) {
    av_usleep(pts_us - elapsed);  // 等待到正确的时间
}
```

**本节小结**：100 行代码完成了从文件到屏幕的完整链路。理解每个 API 的参数和返回值，是写出健壮代码的基础。接下来两节将介绍如何工程化这个代码，以及如何调试优化。

---

## 7. Pipeline 架构：工程化设计

**本节概览**：第 6 节的代码虽然能工作，但把所有逻辑写在 `main` 函数里难以维护和扩展。这一节介绍如何将代码重构为模块化的 Pipeline 架构。

### 7.1 为什么需要架构

**问题代码**：
```cpp
int main() {
    // 300 行混乱代码
    // - 改了这里，那里出问题
    // - 不敢重构，只能继续堆
}
```

**Pipeline 架构**：
```
输入 → Demuxer → Decoder → Renderer → 输出
         ↑           ↑          ↑
      解封装      解码       渲染
```

### 7.2 接口设计

```cpp
// Demuxer 接口
class IDemuxer {
public:
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual ErrorCode ReadPacket(AVPacket* packet) = 0;
    virtual AVStream* GetVideoStream() const = 0;
};

// Decoder 接口
class IDecoder {
public:
    virtual ErrorCode Init(const AVCodecParameters* params) = 0;
    virtual ErrorCode SendPacket(const AVPacket* packet) = 0;
    virtual ErrorCode ReceiveFrame(AVFrame* frame) = 0;
};

// Renderer 接口
class IRenderer {
public:
    virtual ErrorCode Init(int width, int height) = 0;
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
};
```

### 7.3 项目结构

```
chapter-01/
├── CMakeLists.txt
├── include/
│   └── live/
│       ├── interfaces/       # 接口定义
│       │   ├── idemuxer.h
│       │   ├── idecoder.h
│       │   ├── irenderer.h
│       │   └── ipipeline.h
│       └── impl/             # 实现头文件
│           ├── ffmpeg_demuxer.h
│           ├── ffmpeg_decoder.h
│           └── sdl_renderer.h
├── src/
│   ├── impl/                 # 具体实现
│   │   ├── ffmpeg_demuxer.cpp
│   │   ├── ffmpeg_decoder.cpp
│   │   └── sdl_renderer.cpp
│   └── main.cpp
└── tests/
```

### 7.4 完整 CMakeLists.txt

以下是一个完整的 CMake 配置，包含库目标、可执行目标、测试和安装规则：

```cmake
cmake_minimum_required(VERSION 3.14)
project(player VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # 生成 compile_commands.json

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra)
else()
    add_compile_options(-O3 -DNDEBUG)
endif()

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat>=58.0
    libavcodec>=58.0
    libavutil>=56.0
    libswscale>=5.0)
find_package(SDL2 REQUIRED)

# 库目标
add_library(player_lib STATIC
    src/impl/ffmpeg_demuxer.cpp
    src/impl/ffmpeg_decoder.cpp
    src/impl/sdl_renderer.cpp)

target_include_directories(player_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS})

target_link_libraries(player_lib PUBLIC
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2)

# 可执行文件
add_executable(player src/main.cpp)
target_link_libraries(player PRIVATE player_lib)

# 测试
enable_testing()
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# 安装
install(TARGETS player DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
```

**使用示例**：

```bash
# 创建构建目录
mkdir build && cd build

# 配置（Debug 模式）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 编译
make -j$(nproc)

# 运行
./player ../test.mp4

# 安装（可选）
sudo make install
```

**本节小结**：Pipeline 架构将播放器分解为独立的模块，配合 CMake 可以方便地管理依赖、编译和测试。下一节将介绍如何优化性能。

---

## 8. 性能优化：让播放更流畅

**本节概览**：代码能工作后，下一步是优化性能。本节介绍如何测量性能、分析瓶颈，以及实用的优化策略。从软件优化到硬件加速，让你的播放器流畅播放 4K 视频。

### 8.1 性能测量

**在代码中统计解码耗时**：

```cpp
#include <chrono>

class PerformanceMonitor {
public:
    void StartDecode() {
        decode_start_ = std::chrono::high_resolution_clock::now();
    }
    
    void EndDecode() {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end - decode_start_).count();
        
        decode_times_.push_back(duration);
        if (decode_times_.size() > 30) decode_times_.erase(decode_times_.begin());
        
        // 计算平均解码时间
        int64_t avg = 0;
        for (auto t : decode_times_) avg += t;
        avg /= decode_times_.size();
        
        printf("解码耗时: %ld μs (%.1f fps)\n", avg, 1000000.0 / avg);
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> decode_start_;
    std::vector<int64_t> decode_times_;
};

// 使用
PerformanceMonitor monitor;
monitor.StartDecode();
avcodec_receive_frame(ctx, frame);
monitor.EndDecode();
```

**性能指标目标**：

| 指标 | 目标值 | 测量方法 | 说明 |
|:---|:---|:---|:---|
| 解码帧率 | ≥ 30fps | 统计解码时间 | 软解 1080p 应达 60fps+ |
| 渲染帧率 | ≥ 30fps | SDL 回调统计 | 与显示器刷新率同步 |
| CPU 占用 | < 50% | top/htop | 单核占用 |
| 内存占用 | < 200MB | RSS | 包括缓冲区和纹理 |
| 延迟 | < 33ms | 解码到显示 | 30fps 下每帧预算 |

### 8.2 火焰图分析

火焰图可以直观地显示性能热点：

```bash
# 1. 安装火焰图工具
git clone https://github.com/brendangregg/FlameGraph.git

# 2. 记录性能数据
perf record -g --call-graph=dwarf ./player test_1080p.mp4

# 3. 生成火焰图
perf script | ./FlameGraph/stackcollapse-perf.pl | \
    ./FlameGraph/flamegraph.pl > flamegraph.svg

# 4. 浏览器查看
firefox flamegraph.svg
```

**典型播放器火焰图分析**：

```
解码层 (40-50%): avcodec_receive_frame 及相关函数
├── 运动补偿
├── IDCT 变换
└── 环路滤波

渲染层 (20-30%): SDL_UpdateYUVTexture, SDL_RenderPresent
├── 纹理上传
└── GPU 等待 VSync

系统层 (10-20%): 驱动、内存拷贝、文件 IO
```

**优化方向判断**：
- 解码占比 > 50% → 启用多线程或硬件解码
- 纹理上传占比 > 20% → 使用零拷贝或硬件解码
- VSync 等待占比高 → 正常，无需优化

### 8.3 软件优化

**多线程解码**：

```cpp
// 启用帧级多线程（推荐）
codec_ctx->thread_count = 4;  // 4 线程，0 表示自动
codec_ctx->thread_type = FF_THREAD_FRAME;

// 效果对比（1080p H.264）：
// 单线程：25ms/帧 → 40fps
// 4 线程：8ms/帧  → 125fps（大幅余量）
```

**内存池优化**：

频繁分配释放 AVFrame 影响性能，使用内存池：

```cpp
class FramePool {
public:
    ~FramePool() {
        for (auto f : pool_) av_frame_free(&f);
    }
    
    AVFrame* Acquire() {
        if (pool_.empty()) return av_frame_alloc();
        AVFrame* f = pool_.back();
        pool_.pop_back();
        return f;
    }
    
    void Release(AVFrame* f) {
        av_frame_unref(f);  // 清除数据
        pool_.push_back(f); // 保留结构复用
    }
    
private:
    std::vector<AVFrame*> pool_;
};

// 使用
FramePool pool;
AVFrame* frame = pool.Acquire();
// ... 使用 ...
pool.Release(frame);
```

### 8.4 硬件解码

硬件解码可将 CPU 占用从 40-50% 降至 5-10%。

**各平台硬件解码器名称**：

| 平台 | API | H.264 解码器名称 | H.265 解码器名称 |
|:---|:---|:---|:---|
| **macOS/iOS** | VideoToolbox | `h264_videotoolbox` | `hevc_videotoolbox` |
| **Linux (AMD)** | VAAPI | `h264_vaapi` | `hevc_vaapi` |
| **Linux (NVIDIA)** | NVDEC | `h264_nvdec` | `hevc_nvdec` |
| **Linux (Intel)** | VAAPI/QuickSync | `h264_qsv` | `hevc_qsv` |

**macOS VideoToolbox 示例**：

```cpp
// 1. 查找硬件解码器
const AVCodec* codec = avcodec_find_decoder_by_name("h264_videotoolbox");
if (!codec) {
    // 回退到软件解码
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
}

// 2. 创建解码器上下文
AVCodecContext* ctx = avcodec_alloc_context3(codec);

// 3. 设置硬件加速（可选，VideoToolbox 通常自动处理）
// 某些平台需要显式创建硬件设备上下文

// 4. 打开解码器
avcodec_open2(ctx, codec, nullptr);

// 注意：硬件解码输出通常是 GPU 纹理格式
// 可能需要 sws_scale 转换或直接使用 GPU 渲染
```

**Linux VAAPI 示例**：

```cpp
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vaapi.h>

// 1. 创建硬件设备上下文
AVBufferRef* hw_device_ctx = nullptr;
int ret = av_hwdevice_ctx_create(&hw_device_ctx, 
                                  AV_HWDEVICE_TYPE_VAAPI,
                                  "/dev/dri/renderD128",  // 设备路径
                                  nullptr, 0);
if (ret < 0) {
    // 硬件解码不可用，回退软件
}

// 2. 查找解码器
const AVCodec* codec = avcodec_find_decoder_by_name("h264_vaapi");
AVCodecContext* ctx = avcodec_alloc_context3(codec);

// 3. 关联硬件设备
ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

// 4. 打开解码器
avcodec_open2(ctx, codec, nullptr);

// 5. 解码后的 frame->format 会是 AV_PIX_FMT_VAAPI
// 需要使用 av_hwframe_transfer_data 转回系统内存
// 或直接使用 VAAPI 渲染
```

**硬件解码注意事项**：

1. **并非总是更快**：小分辨率视频（< 720p）软解可能更快
2. **格式限制**：硬件解码器支持的格式有限
3. **内存拷贝**：某些情况下 GPU→CPU 拷贝会抵消优势
4. **兼容性**：旧设备可能不支持新编码格式

### 8.5 零拷贝渲染

传统流程：GPU 解码 → 系统内存 → GPU 纹理（两次拷贝）
零拷贝：GPU 解码 → 直接渲染（无拷贝）

```cpp
// 使用 OpenGL/DirectX 直接渲染硬件解码输出
// 需要 SDL2 + OpenGL 或直接使用平台 API

// 简化的零拷贝流程（概念）：
// 1. 硬件解码输出 GPU 纹理
// 2. 直接使用该纹理渲染，不读回 CPU
// 3. 省掉 SDL_UpdateYUVTexture 的上传步骤
```

**本节小结**：性能优化需要测量先行，火焰图定位瓶颈。多线程解码提升明显，硬件解码适合高分辨率视频。下一节将介绍调试技巧。

---

## 9. 调试技巧：排查问题

**本节概览**：开发过程中难免遇到问题，这一节介绍常用的调试工具和方法。

### 9.1 GDB 调试

```bash
gdb ./player
run test.mp4
bt              # 查看调用栈
print variable  # 打印变量
```

### 9.2 Valgrind 内存检测

```bash
valgrind --leak-check=full ./player test.mp4
```

### 9.3 FFmpeg 专用调试技巧

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

**自定义日志回调**：

```cpp
void my_log_callback(void* ptr, int level, const char* fmt, va_list vl) {
    if (level > AV_LOG_WARNING) return;  // 只显示警告及以上
    
    char line[1024];
    vsnprintf(line, sizeof(line), fmt, vl);
    fprintf(stderr, "[FFmpeg] %s", line);
}

// 注册回调
av_log_set_callback(my_log_callback);
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

### 9.4 常见问题速查

| 问题 | 原因 | 解决 |
|:---|:---|:---|
| 绿色画面 | YUV 格式错误 | 检查 `SDL_PIXELFORMAT_IYUV` |
| 播放太快 | 没有 PTS 同步 | 根据时间戳延迟 |
| 内存泄漏 | 未释放资源 | 使用 Valgrind 检测 |
| 解码失败 | 不支持的编解码器 | 检查 codec_id 或尝试硬件解码 |
| 花屏/马赛克 | 数据损坏或丢包 | 检查 packet 是否完整 |

---

## 10. 常见问题

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

### Q3: 音画不同步

```cpp
int64_t audio_pts = ...;  // 获取音频时间
int64_t video_pts = frame->pts * av_q2d(stream->time_base) * 1000;
int64_t diff = video_pts - audio_pts;

if (diff > 40) av_usleep(diff * 1000);  // 视频超前，等待
```

### Q4: 播放卡顿/不流畅

**可能原因与排查**：

1. **解码性能不足**
```bash
# 检查 CPU 占用
top -p $(pgrep player)

# 如果 CPU 接近 100%，启用多线程解码
codec_ctx->thread_count = 4;
```

2. **同步逻辑问题**
```cpp
// 检查 PTS 计算是否正确
printf("PTS: %ld, elapsed: %ld, diff: %ld\n", 
       pts_ms, elapsed_ms, pts_ms - elapsed_ms);

// 如果 diff 一直是负数，说明同步逻辑有问题
```

3. **渲染帧率低于视频帧率**
```cpp
// 检查 SDL 渲染耗时
auto start = std::chrono::high_resolution_clock::now();
SDL_RenderPresent(renderer);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
printf("渲染耗时: %ld ms\n", duration);  // 应 < 33ms
```

### Q5: 花屏/马赛克画面

**原因分析**：

| 现象 | 原因 | 解决 |
|:---|:---|:---|
| 规律性花屏 | 丢包或文件损坏 | 检查文件完整性 |
| 随机色块 | 解码器错误 | 检查 codec_id 是否匹配 |
| 底部绿线 | linesize 理解错误 | 使用 frame->linesize 而非 width |
| 画面撕裂 | 无 VSync | 启用 SDL_RENDERER_PRESENTVSYNC |

**排查代码**：
```cpp
// 检查 packet 完整性
if (packet->flags & AV_PKT_FLAG_CORRUPT) {
    printf("警告：损坏的 packet\n");
    continue;  // 跳过损坏的数据
}

// 检查解码器是否打开
if (!codec_ctx->codec) {
    printf("错误：编解码器未打开\n");
}

// 检查像素格式
printf("Codec pix_fmt: %s\n", av_get_pix_fmt_name(codec_ctx->pix_fmt));
printf("Frame pix_fmt: %s\n", av_get_pix_fmt_name((AVPixelFormat)frame->format));
```

### Q6: 某些视频无法播放

**排查步骤**：

```cpp
// 1. 检查是否找到解码器
const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
if (!codec) {
    printf("不支持的编解码器: %s\n", 
           avcodec_get_name(stream->codecpar->codec_id));
    // 尝试查找备用解码器
}

// 2. 列出系统支持的所有解码器
void ListDecoders() {
    const AVCodec* codec = nullptr;
    void* iter = nullptr;
    printf("支持的 H.264 解码器:\n");
    while ((codec = av_codec_iterate(&iter))) {
        if (codec->id == AV_CODEC_ID_H264 && 
            av_codec_is_decoder(codec)) {
            printf("  %s (%s)\n", codec->name, 
                   codec->long_name ? codec->long_name : "");
        }
    }
}
```

**常见不支持格式处理**：
- **HEVC/H.265**：需要较新 FFmpeg 版本或硬件解码
- **AV1**：需要 FFmpeg 4.4+ 或硬件解码
- **10-bit 视频**：某些解码器只支持 8-bit

### Q7: 网络流播放超时/卡顿

**RTMP/HTTP 流超时设置**：

```cpp
AVDictionary* opts = nullptr;

// 设置打开超时（微秒）
av_dict_set(&opts, "timeout", "10000000", 0);  // 10 秒

// 设置读取超时
av_dict_set(&opts, "rw_timeout", "5000000", 0);  // 5 秒

// 设置 RTMP 缓冲区
av_dict_set(&opts, "rtmp_buffer", "1000", 0);  // 1000ms

// 设置 TCP 连接超时
av_dict_set(&opts, "tcp_nodelay", "1", 0);

int ret = avformat_open_input(&ctx, url, nullptr, &opts);
av_dict_free(&opts);
```

**网络流错误恢复**：
```cpp
// 网络断开时尝试重连
int retry_count = 0;
const int MAX_RETRY = 3;

while (retry_count < MAX_RETRY) {
    ret = av_read_frame(fmt_ctx, packet);
    if (ret == AVERROR_EOF || ret == AVERROR_EXIT) {
        printf("连接断开，尝试重连...\n");
        // 重新打开流
        avformat_close_input(&fmt_ctx);
        ret = avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
        if (ret >= 0) {
            retry_count = 0;
            continue;
        }
        retry_count++;
        av_usleep(1000000);  // 等待 1 秒
    }
}
```

### Q8: 内存占用持续增长

**排查内存泄漏**：

```cpp
// 1. 确保配对释放
AVPacket* pkt = av_packet_alloc();
// ... 使用 ...
av_packet_unref(pkt);  // 每次读取后必须 unref
av_packet_free(&pkt);  // 最后释放

// 2. 检查解码器刷新
// 文件结束时需要发送空 packet 刷新
avcodec_send_packet(ctx, nullptr);
while (avcodec_receive_frame(ctx, frame) == 0) {
    // 处理剩余帧
}

// 3. SDL 纹理释放
SDL_DestroyTexture(texture);  // 程序结束前
```

**使用 Valgrind 定位**：
```bash
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./player test.mp4
```

### Q9: 编译警告处理

**C++ 混合编译警告**：
```cpp
// FFmpeg 是 C 库，需要用 extern "C" 包裹
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

// 或者在头文件中
#ifdef __cplusplus
extern "C" {
#endif

#include "ffmpeg_headers.h"

#ifdef __cplusplus
}
#endif
```

**弃用 API 警告**：
```cpp
// 某些 API 已弃用，使用新 API
// 旧：avcodec_decode_video2
// 新：avcodec_send_packet + avcodec_receive_frame
```

---

## 11. 本章总结与下一步

### 11.1 本章回顾

我们从零开始，一步步搭建了一个完整的视频播放器：

1. **为什么能压缩**：利用空间冗余（DCT）、时间冗余（帧间预测）、视觉冗余（YUV）
2. **像素格式**：YUV420 比 RGB 省 50% 空间
3. **FFmpeg 架构**：AVFormatContext → AVStream → AVPacket → AVCodecContext → AVFrame
4. **SDL2 渲染**：Window → Renderer → Texture
5. **Pipeline 架构**：模块化设计，便于维护和扩展

### 11.2 本章的局限

当前实现是**同步单线程**，存在性能瓶颈：

```
播放 4K 视频时：
- 文件读取：5ms
- 解码：25ms
- 渲染：8ms
- 总时间（串行）：38ms → 26fps（卡顿！）
```

### 11.3 下一步：异步多线程架构

解决方案是**多线程并行**：

```
线程 A：读取文件 ──→ 队列 ──→ 线程 B：解码 ──→ 队列 ──→ 线程 C：渲染
         5ms                      25ms                      8ms

并行后瓶颈是解码 25ms → 40fps（流畅）
```

**第 2 章预告**：
- 生产者-消费者队列设计
- 线程安全的数据结构
- 帧队列管理（固定大小、超时丢帧）
- 多线程同步原语（mutex、condition_variable）

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

**本章代码仓库**：https://github.com/chapin666/live-system-book
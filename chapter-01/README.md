# 第一章：Pipeline 架构与本地播放

> **目标**：从零开始，用 100 行代码实现视频播放，深入理解视频压缩原理、FFmpeg 架构和播放器 Pipeline 设计。

**预计时间**：4 小时

**你将学到**：
- 视频为什么能压缩 100 倍（空间/时间/视觉冗余）
- YUV 颜色空间与 RGB 的区别
- FFmpeg 核心数据结构（AVFormatContext、AVCodecContext、AVPacket、AVFrame）
- SDL2 GPU 渲染原理
- Pipeline 架构设计模式
- 性能分析与调试技巧

---

## 目录

1. [快速开始](#1-快速开始) — 先跑起来
2. [视频压缩原理](#2-视频压缩原理) — 为什么 1 分钟视频只有 100MB
3. [颜色空间与像素格式](#3-颜色空间与像素格式) — YUV 详解
4. [FFmpeg 架构解析](#4-ffmpeg-架构解析) — 核心数据结构源码级分析
5. [SDL2 渲染原理](#5-sdl2-渲染原理) — GPU 加速与纹理上传
6. [代码详解](#6-代码详解) — 逐行分析
7. [Pipeline 架构设计](#7-pipeline-架构设计) — 工程化实践
8. [性能分析与优化](#8-性能分析与优化) — 火焰图与热点分析
9. [调试技巧实战](#9-调试技巧实战) — gdb、valgrind、perf
10. [常见问题](#10-常见问题)
11. [下一步](#11-下一步)

---

## 1. 快速开始

### 1.1 安装依赖

**macOS:**
```bash
brew install ffmpeg sdl2 cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y ffmpeg \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev \
    libsdl2-dev cmake pkg-config
```

**验证安装**：
```bash
# 检查 FFmpeg
ffmpeg -version | head -1
# ffmpeg version 6.1.1

# 检查 SDL2
sdl2-config --version
# 2.30.0

# 检查 pkg-config
pkg-config --cflags --libs libavformat libavcodec libavutil sdl2
```

### 1.2 100 行播放器

创建 `player.cpp`：

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
    
    // 读取流信息（时长、码率等）
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

    // 打印视频信息
    printf("视频信息:\n");
    printf("  分辨率: %dx%d\n", 
           video_stream->codecpar->width,
           video_stream->codecpar->height);
    printf("  时长: %.2f 秒\n", 
           fmt_ctx->duration / (double)AV_TIME_BASE);

    // ========== 3. 初始化解码器 ==========
    const AVCodec* codec = avcodec_find_decoder(
        video_stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "不支持的编解码器\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);
    
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        fprintf(stderr, "无法打开解码器\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // ========== 4. 创建 SDL2 窗口 ==========
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL 初始化失败: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "FFmpeg 播放器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codec_ctx->width, codec_ctx->height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,           // YUV420P 格式
        SDL_TEXTUREACCESS_STREAMING,    // 频繁更新
        codec_ctx->width, codec_ctx->height);

    // ========== 5. 解码循环 ==========
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool running = true;
    int64_t start_time = av_gettime();  // 微秒

    while (running) {
        // 处理事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        // 读取一帧
        ret = av_read_frame(fmt_ctx, packet);
        if (ret < 0) break;  // 文件结束或错误

        // 只处理视频流
        if (packet->stream_index == video_stream_idx) {
            // 发送到解码器
            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                fprintf(stderr, "发送 packet 失败\n");
                continue;
            }

            // 接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) {
                    fprintf(stderr, "接收 frame 失败\n");
                    break;
                }

                // 计算延迟实现同步
                int64_t pts_us = frame->pts * av_q2d(video_stream->time_base) * 1000000;
                int64_t elapsed = av_gettime() - start_time;
                if (pts_us > elapsed) {
                    av_usleep(pts_us - elapsed);
                }

                // 更新纹理并渲染
                SDL_UpdateYUVTexture(
                    texture, nullptr,
                    frame->data[0], frame->linesize[0],  // Y
                    frame->data[1], frame->linesize[1],  // U
                    frame->data[2], frame->linesize[2]); // V
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
        }
        
        av_packet_unref(packet);
    }

    // ========== 6. 清理资源 ==========
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
```

### 1.3 编译运行

```bash
# 创建测试视频（彩色条纹，5秒，30fps）
ffmpeg -f lavfi -i testsrc=duration=5:size=640x480:rate=30 \
       -pix_fmt yuv420p -c:v libx264 -preset fast -crf 23 \
       test.mp4

# 编译
g++ -std=c++14 -O2 player.cpp -o player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 运行
./player test.mp4
```

看到彩色条纹在滚动？成功了！🎉

**扩展功能**：
```cpp
// 按空格暂停/继续
if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
    paused = !paused;
}

// 按 ESC 退出
if (event.key.keysym.sym == SDLK_ESCAPE) running = false;

// 窗口大小改变时保持比例
SDL_RenderSetLogicalSize(renderer, width, height);
```

---

## 2. 视频压缩原理

### 2.1 原始视频有多大

不压缩的视频数据量惊人：

| 分辨率 | 每帧大小 (RGB24) | 1 秒 (30fps) | 1 分钟 | 1 小时 |
|:---|:---|:---|:---|:---|
| 1280×720 (720p) | 2.8 MB | 84 MB | **5.0 GB** | 300 GB |
| 1920×1080 (1080p) | 6.2 MB | 186 MB | **11.2 GB** | 672 GB |
| 3840×2160 (4K) | 24.9 MB | 747 MB | **44.8 GB** | 2.7 TB |

**实际 1 分钟 1080p 视频约 100 MB**，压缩了 **100 倍以上**！

### 2.2 视频为什么能压缩

视频数据存在三种冗余，压缩就是去除这些冗余：

#### 冗余 1：空间冗余（帧内压缩）

**现象**：相邻像素通常很相似。看一张天空照片，大片区域的像素值几乎相同。

**示例**：
```
一行像素值（蓝天的蓝色通道）：
200, 201, 200, 199, 201, 202, 200, 199, 201, 200 ...

直接存储：10 字节

差分编码：
200（基准值）, +1, -1, -1, +2, +1, -2, -1, +2, -1 ...

差值范围小（-2 到 +2），可以用更少的位存储
压缩后：约 5 字节
```

**核心技术**：

1. **DCT 变换（离散余弦变换）**
   
   将图像从"空间域"（像素值）转换到"频率域"（变化快慢）。
   
   ```
   空间域：每个像素的具体值
   频率域：图像中变化的快慢
   
   低频 = 缓慢变化的区域（天空、墙面）
   高频 = 快速变化的区域（边缘、纹理）
   ```
   
   DCT 把图像分解成不同频率的「基图像」组合：
   ```
   图像 = a×(低频基图) + b×(中频基图) + c×(高频基图) + ...
   ```
   
   大部分能量集中在低频，高频系数很小。

2. **量化**
   
   丢弃人眼不敏感的高频信息：
   ```
   原始系数：[100, 50, 20, 10, 5, 2, 1, 0.5]
   
   量化表：  [  1,  2,  4,  8, 16, 32, 64, 128]
   
   量化后：  [100, 25,  5,  1,  0,  0,  0,   0]  ← 很多变成 0
   ```
   
   量化步长越大，压缩率越高，画质损失越大。

3. **熵编码**
   
   用变长编码表示量化后的系数：
   - 频繁出现的值（如 0）用短码表示
   - 罕见出现的值用长码表示
   
   类似莫尔斯电码：E 用 `·`，Q 用 `--·-`。

#### 冗余 2：时间冗余（帧间压缩）

**现象**：连续帧之间变化很小。30fps 视频中，相邻帧间隔仅 33ms，画面变化通常不大。

**示例**：
```
第 1 帧（I 帧）：[完整画面]          50 KB
第 2 帧（P 帧）：[变化区域]          10 KB  （只存与第 1 帧的差异）
第 3 帧（P 帧）：[变化区域]          8 KB   （只存与第 2 帧的差异）
第 4 帧（P 帧）：[变化区域]          9 KB
...
```

**运动估计**：

找到当前帧的块在上一帧的哪个位置。只编码位置偏移（运动向量）和残差。

```
当前帧：  [汽车在某个位置]
上一帧：  [汽车在左边一点]

编码：
- 运动向量：(-16, 0)  ← 向左移动 16 像素
- 残差：    [细微差异]  ← 由于光照变化等

比直接编码整个块节省大量数据
```

#### 冗余 3：视觉冗余（色度子采样）

**人眼特性**：视网膜上的视锥细胞（感色）比视杆细胞（感光）少得多。

```
人眼敏感度：
- 亮度（明暗变化）：非常敏感
- 色度（颜色变化）：相对不敏感
```

**YUV 颜色空间**：
- **Y (Luma)**：亮度，决定图像明暗
- **U (Cb)**：蓝色色度
- **V (Cr)**：红色色度

**4:2:0 采样**：
```
RGB 格式：每个像素 3 字节（R、G、B）

YUV420 格式：
- Y：每个像素 1 字节（完整分辨率）
- U：每 4 个像素 1 字节（1/4 分辨率）
- V：每 4 个像素 1 字节（1/4 分辨率）

平均：1.5 字节/像素，比 RGB 省 50%
```

### 2.3 帧类型详解

视频编码使用三种帧类型配合压缩：

<img src="../docs/images/frame-types.svg" width="100%"/>

| 帧类型 | 名称 | 典型大小 | 编码方式 | 用途 |
|:---|:---|:---|:---|:---|
| **I 帧** | 关键帧 (Intra-coded) | 40-60 KB | 完整编码，类似 JPEG | 随机访问点，快进定位 |
| **P 帧** | 预测帧 (Predicted) | 8-15 KB | 参考前一帧 | 正常播放，中等压缩率 |
| **B 帧** | 双向帧 (Bi-directional) | 3-8 KB | 参考前后两帧 | 高压缩率 |

**为什么 B 帧压缩率最高？**

B 帧可以同时参考前面和后面的帧，找到最匹配的块：

```
场景：球从左边飞到右边

第 1 帧 (I)：球在左边
第 2 帧 (B)：球在中间
第 3 帧 (P)：球在右边

编码第 2 帧时：
- 可以看到第 1 帧（球在左边）
- 也可以看到第 3 帧（球在右边）
- 更好地预测中间位置

而 P 帧只能看前面的帧
```

**GOP (Group of Pictures)**：

两个 I 帧之间的帧序列称为一个 GOP。

```
典型 GOP 结构：
帧号：  0    1    2    3    4    5    6    7    8    9
类型：  I    P    B    B    P    B    B    P    B    I
大小：  50K  12K  5K   4K   10K  6K   5K   11K  5K   48K

GOP 大小 = 9 帧
I 帧间隔 = 约 300ms（30fps 下）
```

**GOP 大小的权衡**：
- **GOP 太小**（I 帧频繁）：压缩率低，但快进响应快
- **GOP 太大**（I 帧稀疏）：压缩率高，但快进时需要解码很多帧

### 2.4 编码标准演进

| 标准 | 年份 | 压缩效率 | 特点 | 应用场景 |
|:---|:---|:---|:---|:---|
| MPEG-2 | 1995 | 基准 | 最早的数字视频标准 | DVD、数字电视 |
| **H.264/AVC** | 2003 | 2× MPEG-2 | 最广泛支持，软硬件成熟 | 在线视频、直播、监控 |
| H.265/HEVC | 2013 | 2× H.264 | 专利费高 | 4K/8K 视频 |
| **AV1** | 2018 | 比 H.265 省 30% | 开源免专利费，编码慢 | YouTube、Netflix |
| H.266/VVC | 2020 | 2× H.265 | 最新标准，尚未普及 | 未来 8K 视频 |

**为什么 H.264 仍是主流？**

1. 硬件支持最广泛（手机、电脑、电视都支持）
2. 专利费相对合理
3. 编码速度快，适合实时场景
4. 生态成熟（FFmpeg、浏览器、播放器都支持）

---

## 3. 颜色空间与像素格式

### 3.1 RGB 与 YUV

**RGB**：红绿蓝三原色，每个像素 3 字节。

```cpp
struct RGBPixel {
    uint8_t r;  // 红色 0-255
    uint8_t g;  // 绿色 0-255
    uint8_t b;  // 蓝色 0-255
};
// 1920×1080 RGB 图像：6,220,800 字节
```

**YUV**：亮度 + 色度分离。

```cpp
struct YUVPixel {
    uint8_t y;  // 亮度 0-255
    uint8_t u;  // 色度（蓝）-128~127
    uint8_t v;  // 色度（红）-128~127
};
```

**YUV vs RGB 对比**：

| 特性 | RGB | YUV |
|:---|:---|:---|
| 存储效率 | 低（3 字节/像素） | 高（1.5 字节/像素，YUV420）|
| 兼容性 | 显示器原生 | 需转换 |
| 黑白兼容 | 需特殊处理 | Y 通道就是黑白信号 |
| 压缩友好 | 不友好 | 友好（可降采样 U/V）|
| 应用场景 | 图像处理、游戏 | 视频编解码、传输 |

**为什么视频用 YUV？**

1. **兼容黑白电视**：早期电视是黑白的，Y 通道可以直接给黑白电视用
2. **利用人眼特性**：对亮度敏感，对色度不敏感 → 可以压缩 U/V
3. **压缩友好**：色度可以降采样，亮度保持完整

### 3.2 YUV 格式家族

YUV 有多种采样格式：

| 格式 | Y:U:V | U/V 分辨率 | 字节/像素 | 说明 |
|:---|:---|:---|:---|:---|
| YUV444 | 4:4:4 | 完整 | 3.0 | 无损，专业制作 |
| YUV422 | 4:2:2 | 宽度 1/2 | 2.0 | 广播级 |
| **YUV420** | 4:2:0 | 宽/高各 1/2 | 1.5 | 最常用，视频编码 |
| YUV411 | 4:1:1 | 宽度 1/4 | 1.5 | 老格式，少用 |

**4:2:0 采样详解**：

```
对于 4×4 像素的块：

Y 平面（完整）：    U 平面（1/4）：    V 平面（1/4）：
┌─┬─┬─┬─┐         ┌─┬─┐            ┌─┬─┐
│Y│Y│Y│Y│         │U│ │            │V│ │
├─┼─┼─┼─┤         ├─┼─┤            ├─┼─┤
│Y│Y│Y│Y│         │ │ │            │ │ │
├─┼─┼─┼─┤         └─┴─┘            └─┴─┘
│Y│Y│Y│Y│         2×2 = 4 字节      2×2 = 4 字节
├─┼─┼─┼─┤         
│Y│Y│Y│Y│         
└─┴─┴─┴─┘         
16 字节

总计：16 + 4 + 4 = 24 字节 / 16 像素 = 1.5 字节/像素
```

**YUV420P 内存布局**：

<img src="../docs/images/yuv-layout-new.svg" width="90%"/>

**计算示例（1920×1080）**：

| 平面 | 分辨率 | 行宽 (linesize) | 大小 | 占比 |
|:---|:---|:---|:---|:---|
| Y | 1920 × 1080 | 1920（或 2048 对齐） | 2,073,600 B | 66.7% |
| U | 960 × 540 | 960（或 1024 对齐） | 518,400 B | 16.7% |
| V | 960 × 540 | 960（或 1024 对齐） | 518,400 B | 16.7% |
| **总计** | - | - | **3,110,400 B** | 100% |

对比 RGB：1920 × 1080 × 3 = **6,220,800 B**

**YUV420P 比 RGB24 节省 50% 空间**。

### 3.3 FFmpeg 中的像素格式

**AVPixelFormat 枚举**：

```cpp
enum AVPixelFormat {
    AV_PIX_FMT_NONE = -1,
    AV_PIX_FMT_YUV420P,    // YUV 4:2:0 平面格式（本章使用）
    AV_PIX_FMT_YUYV422,    // YUV 4:2:2 打包格式
    AV_PIX_FMT_RGB24,      // RGB 打包格式
    AV_PIX_FMT_BGR24,      // BGR 打包格式
    AV_PIX_FMT_GRAY8,      // 8 位灰度
    AV_PIX_FMT_NV12,       // YUV 4:2:0，UV 交错
    AV_PIX_FMT_NV21,       // YUV 4:2:0，VU 交错（Android 相机）
    // ... 更多格式
};
```

**访问 YUV 数据**：

```cpp
AVFrame* frame = av_frame_alloc();
// ... 解码得到 frame ...

// 平面指针
uint8_t* y_data = frame->data[0];  // Y 平面
uint8_t* u_data = frame->data[1];  // U 平面
uint8_t* v_data = frame->data[2];  // V 平面

// 行宽（每行的字节数，包含对齐填充）
int y_stride = frame->linesize[0];
int u_stride = frame->linesize[1];
int v_stride = frame->linesize[2];

// 访问像素 (x, y)
int y_val = y_data[y * y_stride + x];
int u_val = u_data[(y/2) * u_stride + (x/2)];  // U/V 分辨率减半
int v_val = v_data[(y/2) * v_stride + (x/2)];

printf("Pixel(%d,%d): Y=%d, U=%d, V=%d\n", x, y, y_val, u_val, v_val);
```

⚠️ **重要**：`linesize` 可能大于 `width`！

某些 CPU（如 ARM NEON）要求内存 32 字节对齐，行末会有填充字节：

```cpp
// 假设 width = 1920，但 linesize = 2048（64 字节对齐）
// 每行末尾有 128 字节的填充
// 访问时必须用 linesize，不能用 width！

// 错误！可能读到填充区域
uint8_t y = frame->data[0][y * frame->width + x];

// 正确
uint8_t y = frame->data[0][y * frame->linesize[0] + x];
```

### 3.4 YUV 转 RGB

有时需要将 YUV 转为 RGB（如截图保存）：

```cpp
#include <libswscale/swscale.h>

// 创建转换上下文
SwsContext* sws_ctx = sws_getContext(
    width, height, AV_PIX_FMT_YUV420P,    // 源格式
    width, height, AV_PIX_FMT_RGB24,      // 目标格式
    SWS_BILINEAR, nullptr, nullptr, nullptr);

// 准备 RGB 缓冲区
uint8_t* rgb_data[4] = {nullptr};
int rgb_linesize[4] = {0};
av_image_alloc(rgb_data, rgb_linesize, width, height, AV_PIX_FMT_RGB24, 32);

// 转换
sws_scale(sws_ctx, 
    frame->data, frame->linesize,  // 源
    0, height,                      // 起始行、高度
    rgb_data, rgb_linesize);        // 目标

// 保存为 BMP/PNG
// ...

// 清理
av_freep(&rgb_data[0]);
sws_freeContext(sws_ctx);
```

---

## 4. FFmpeg 架构解析

### 4.1 FFmpeg 库结构

FFmpeg 由多个库组成，各司其职：

```
┌─────────────────────────────────────────────────────────────┐
│  libavformat - 封装/解封装（Container 处理）                 │
│  ├─ 读取文件头，提取流信息                                   │
│  ├─ 分离音视频流（Demux）                                    │
│  ├─ 网络协议支持（HTTP、RTMP、HLS 等）                       │
│  └─ 主要文件：libavformat/demuxer 列表                       │
├─────────────────────────────────────────────────────────────┤
│  libavcodec - 编解码器（Codec 处理）                         │
│  ├─ 解码：压缩数据 → 原始帧（YUV/PCM）                       │
│  ├─ 编码：原始帧 → 压缩数据                                  │
│  ├─ 支持硬件加速（VAAPI、VideoToolbox、NVDEC 等）            │
│  └─ 主要文件：libavcodec/codec 列表                          │
├─────────────────────────────────────────────────────────────┤
│  libavutil - 工具函数（基础设施）                            │
│  ├─ 内存管理（av_malloc、av_free）                          │
│  ├─ 数学运算、时间处理                                       │
│  ├─ 数据结构（AVBuffer、AVDictionary、AVOption）             │
│  └─ 日志系统（av_log）                                       │
├─────────────────────────────────────────────────────────────┤
│  libswscale - 图像转换（像素格式转换、缩放）                 │
│  └─ sws_scale() - YUV ↔ RGB 转换                           │
├─────────────────────────────────────────────────────────────┤
│  libswresample - 音频重采样                                  │
│  └─ 采样率转换、声道布局转换                                 │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 核心数据结构详解

#### AVFormatContext - 格式上下文

这是 FFmpeg 中最重要的结构体之一，代表一个打开的文件或流：

```cpp
typedef struct AVFormatContext {
    const AVClass* av_class;           // 用于日志和反射
    
    // 输入输出格式
    struct AVInputFormat* iformat;     // 输入格式（如 mp4、flv）
    struct AVOutputFormat* oformat;    // 输出格式（编码时使用）
    
    // IO 上下文
    AVIOContext* pb;                   // IO 上下文（文件/网络句柄）
    
    // 流信息
    unsigned int nb_streams;           // 流数量（可能有视频+音频+字幕）
    AVStream** streams;                // 流数组
    
    // 文件信息
    char* url;                         // 文件路径或 URL
    int64_t start_time;                // 起始时间（微秒）
    int64_t duration;                  // 总时长（微秒）
    int64_t bit_rate;                  // 总码率（bps）
    
    // 元数据
    AVDictionary* metadata;            // 键值对元数据
} AVFormatContext;
```

**常用操作**：

```cpp
// 打开文件
AVFormatContext* fmt_ctx = nullptr;
avformat_open_input(&fmt_ctx, "video.mp4", nullptr, nullptr);

// 获取流信息
printf("文件: %s\n", fmt_ctx->url);
printf("时长: %.2f 秒\n", fmt_ctx->duration / (double)AV_TIME_BASE);
printf("码率: %lld kbps\n", fmt_ctx->bit_rate / 1000);
printf("流数: %d\n", fmt_ctx->nb_streams);

// 遍历流
for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
    AVStream* st = fmt_ctx->streams[i];
    printf("流 #%d: 类型=%s\n", i, 
        st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? "视频" :
        st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ? "音频" : "其他");
}

// 关闭文件
avformat_close_input(&fmt_ctx);
```

#### AVStream - 媒体流

代表文件中的一个流（视频流、音频流等）：

```cpp
typedef struct AVStream {
    int index;                         // 流索引（0, 1, 2...）
    int id;                            // 流 ID（格式相关）
    
    AVCodecParameters* codecpar;       // 编解码器参数（分辨率、码率等）
    AVRational time_base;              // 时间基（分数形式）
    
    int64_t start_time;                // 起始时间（以 time_base 为单位）
    int64_t duration;                  // 流时长（以 time_base 为单位）
    
    AVRational avg_frame_rate;         // 平均帧率（可能不准确）
    AVRational r_frame_rate;           // 实际帧率（最可靠）
    
    AVDictionary* metadata;            // 流级元数据
} AVStream;
```

**时间基详解**：

时间基是 PTS/DTS 的单位，是一个分数：

```cpp
AVRational tb = stream->time_base;  // 如 {1, 1000} 表示 1/1000 秒 = 毫秒

// 转换函数
static inline double av_q2d(AVRational a) {
    return a.num / (double) a.den;
}

// 使用示例
int64_t pts = 33000;                    // 33 秒（以 time_base 为单位）
double seconds = pts * av_q2d(tb);      // 33.0 秒
int64_t ms = seconds * 1000;            // 33000 毫秒
```

**实际应用（同步）**：

```cpp
// 计算帧应该显示的时间（毫秒）
int64_t get_frame_ms(AVFrame* frame, AVStream* stream) {
    return frame->pts * av_q2d(stream->time_base) * 1000;
}

// 同步播放
int64_t frame_ms = get_frame_ms(frame, video_stream);
int64_t elapsed_ms = av_gettime() / 1000 - start_ms;

if (frame_ms > elapsed_ms) {
    av_usleep((frame_ms - elapsed_ms) * 1000);  // 等待
}
```

#### AVCodecParameters - 编解码器参数

存储流的编码参数：

```cpp
typedef struct AVCodecParameters {
    enum AVMediaType codec_type;       // 媒体类型（视频/音频/字幕）
    enum AVCodecID codec_id;           // 编解码器 ID（如 AV_CODEC_ID_H264）
    
    // 视频参数
    int width, height;                 // 分辨率
    AVRational sample_aspect_ratio;    // 像素宽高比
    
    // 音频参数
    int sample_rate;                   // 采样率
    int channels;                      // 声道数
    uint64_t channel_layout;           // 声道布局
    enum AVSampleFormat format;        // 采样格式
    
    // 通用参数
    int bit_rate;                      // 码率（bps）
    int profile;                       // 编码档次（如 High Profile）
    int level;                         // 编码级别（如 4.1）
} AVCodecParameters;
```

#### AVCodecContext - 编解码器上下文

编解码器的状态和配置：

```cpp
typedef struct AVCodecContext {
    const AVClass* av_class;
    const struct AVCodec* codec;       // 编解码器
    
    // 视频参数
    int width, height;                 // 分辨率
    AVPixelFormat pix_fmt;             // 像素格式
    
    // 音频参数
    int sample_rate;
    int channels;
    
    // 编码参数
    int bit_rate;                      // 目标码率
    int gop_size;                      // GOP 大小（I 帧间隔）
    int max_b_frames;                  // 最大 B 帧数
    
    // 解码参数
    int thread_count;                  // 解码线程数（0=自动）
    int thread_type;                   // 线程类型（帧级/切片级）
    
    // 回调函数（用于获取帧时通知）
    int (*get_buffer2)(struct AVCodecContext* s, AVFrame* frame, int flags);
} AVCodecContext;
```

**解码器选项**：

```cpp
// 启用多线程解码（大幅提升性能）
codec_ctx->thread_count = 4;  // 使用 4 个线程
codec_ctx->thread_type = FF_THREAD_FRAME;  // 帧级并行

// 低延迟模式（直播场景）
av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
```

#### AVPacket - 压缩数据包

从文件读取的原始压缩数据：

```cpp
typedef struct AVPacket {
    AVBufferRef* buf;                  // 数据缓冲区（引用计数管理）
    int64_t pts;                       // 显示时间戳（Presentation TS）
    int64_t dts;                       // 解码时间戳（Decoding TS）
    uint8_t* data;                     // 数据指针
    int size;                          // 数据大小（字节）
    int stream_index;                  // 所属流索引
    int flags;                         // 标志位
#define AV_PKT_FLAG_KEY     0x0001    // 关键帧
#define AV_PKT_FLAG_CORRUPT 0x0002    // 数据损坏
    int64_t pos;                       // 在文件中的位置
    int64_t duration;                  // 包时长
} AVPacket;
```

**为什么需要两个时间戳（PTS 和 DTS）？**

因为 B 帧的存在，解码顺序和显示顺序可能不同：

```
显示顺序：I0  B1  B2  P3  B4  B5  P6
           ↓   ↓   ↓   ↓   ↓   ↓   ↓
PTS:      0   1   2   3   4   5   6

解码顺序：I0  P3  B1  B2  P6  B4  B5  （B 帧需要参考后面的 P 帧）
           ↓   ↓   ↓   ↓   ↓   ↓   ↓
DTS:      0   1   2   3   4   5   6

PTS 用于显示同步，DTS 用于解码顺序
```

#### AVFrame - 原始帧

解码后的原始图像数据：

```cpp
typedef struct AVFrame {
    uint8_t* data[8];                  // 各平面数据指针
    int linesize[8];                   // 各平面行宽
    
    int width, height;                 // 分辨率
    int nb_samples;                    // 音频采样数
    
    AVPixelFormat format;              // 像素格式（视频）
    AVSampleFormat sample_fmt;         // 采样格式（音频）
    
    int64_t pts;                       // 显示时间戳
    int64_t pkt_dts;                   // 对应 packet 的 DTS
    
    int key_frame;                     // 是否关键帧
    enum AVPictureType pict_type;      // 帧类型（I/P/B）
    
    AVRational sample_aspect_ratio;    // 像素宽高比
    
    int64_t best_effort_timestamp;     // 最佳时间戳估计
    
    AVDictionary* metadata;            // 帧级元数据
} AVFrame;
```

### 4.3 FFmpeg 内存管理

FFmpeg 使用引用计数管理内存，避免拷贝：

```cpp
// AVBufferRef - 引用计数缓冲区
typedef struct AVBufferRef {
    AVBuffer* buffer;                  // 实际缓冲区
    uint8_t* data;                     // 数据指针
    int size;                          // 大小
} AVBufferRef;

// 引用计数操作
AVBufferRef* av_buffer_alloc(int size);           // 分配
AVBufferRef* av_buffer_ref(AVBufferRef* buf);     // 引用计数 +1
void av_buffer_unref(AVBufferRef** buf);          // 引用计数 -1
```

**为什么使用引用计数？**

```cpp
// 场景：视频帧需要同时显示和保存缩略图
// 不使用引用计数：需要复制一份数据（耗时、耗内存）
// 使用引用计数：两个地方指向同一份数据，计数=2

AVFrame* frame = av_frame_alloc();
// ... 解码填充数据 ...

AVFrame* thumbnail = av_frame_alloc();
av_frame_ref(thumbnail, frame);  // 引用计数 +1，不拷贝数据

// 两个 frame 指向同一份数据，计数=2

av_frame_free(&frame);      // 计数=1，不释放
av_frame_free(&thumbnail);  // 计数=0，真正释放
```

---

## 5. SDL2 渲染原理

### 5.1 SDL2 架构

```
┌─────────────────────────────────────────────┐
│           SDL2 应用程序                      │
├─────────────────────────────────────────────┤
│  SDL_Window  ──→  窗口（标题栏、边框）       │
│       ↓                                     │
│  SDL_Renderer ──→ 渲染器（GPU/CPU 加速）    │
│       ↓                                     │
│  SDL_Texture ──→ 纹理（显存中的图像）       │
│       ↓                                     │
│  GPU/显示器                                  │
└─────────────────────────────────────────────┘
```

### 5.2 渲染器类型

| 类型 | 标志 | 特点 | 适用场景 |
|:---|:---|:---|:---|
| **硬件加速** | `SDL_RENDERER_ACCELERATED` | 使用 GPU，快 | 默认首选 |
| **软件渲染** | `SDL_RENDERER_SOFTWARE` | 使用 CPU，兼容性好 | GPU 不支持时 |
| **垂直同步** | `SDL_RENDERER_PRESENTVSYNC` | 同步显示器刷新率 | 避免画面撕裂 |
| **目标纹理** | `SDL_RENDERER_TARGETTEXTURE` | 渲染到纹理而非屏幕 | 后期处理 |

**创建渲染器**：

```cpp
SDL_Renderer* renderer = SDL_CreateRenderer(
    window, -1,  // -1 表示让 SDL 选择最合适的驱动
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

// 查询实际使用的驱动
SDL_RendererInfo info;
SDL_GetRendererInfo(renderer, &info);
printf("渲染器: %s\n", info.name);  // "opengl", "metal", "direct3d", "software"
```

### 5.3 纹理格式

SDL2 支持多种纹理格式：

| 格式 | 说明 | 字节/像素 | 用途 |
|:---|:---|:---|:---|
| `SDL_PIXELFORMAT_RGB888` | RGB，24位 | 3 | 简单 RGB 图像 |
| `SDL_PIXELFORMAT_RGBA8888` | RGBA，32位 | 4 | 带透明通道 |
| `SDL_PIXELFORMAT_YV12` | YUV420，平面 | 1.5 | **视频播放** |
| `SDL_PIXELFORMAT_IYUV` | YUV420，平面 | 1.5 | 与 YV12 UV 顺序相反 |
| `SDL_PIXELFORMAT_NV12` | YUV420，UV 交错 | 1.5 | 某些硬件解码输出 |

**YV12 vs IYUV**：

```cpp
// YV12: Y, V, U 顺序（FFmpeg 默认）
// IYUV: Y, U, V 顺序

// FFmpeg 解码输出通常是 YUV420P（Y,U,V 顺序）
// 使用 IYUV 可以直接上传，不需要交换

SDL_Texture* tex = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_IYUV,  // 与 FFmpeg YUV420P 匹配
    SDL_TEXTUREACCESS_STREAMING,
    width, height);
```

### 5.4 纹理更新方式

| 访问模式 | 说明 | 适用场景 |
|:---|:---|:---|
| `SDL_TEXTUREACCESS_STATIC` | 更新少，GPU 显存 | 背景图、精灵图 |
| `SDL_TEXTUREACCESS_STREAMING` | 频繁更新，CPU 可写 | **视频播放** |
| `SDL_TEXTUREACCESS_TARGET` | 可渲染目标 | 离屏渲染 |

**视频播放使用 STREAMING 模式**：

```cpp
SDL_Texture* tex = SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_IYUV,
    SDL_TEXTUREACCESS_STREAMING,  // 频繁更新
    width, height);

// 每帧更新
SDL_UpdateYUVTexture(tex, nullptr,
    frame->data[0], frame->linesize[0],  // Y
    frame->data[1], frame->linesize[1],  // U
    frame->data[2], frame->linesize[2]); // V
```

### 5.5 VSync 与画面撕裂

**画面撕裂现象**：

显示器刷新率和视频帧率不匹配时，可能看到画面上下两部分不一致。

```
帧率 60fps，显示器 60Hz：完美匹配 ✓
帧率 30fps，显示器 60Hz：每帧显示 2 次 ✓
帧率 24fps，显示器 60Hz：不均匀（2, 3, 2, 3...）可能有卡顿
帧率 无限制，显示器 60Hz：撕裂 ✗
```

**解决方案**：

1. **启用 VSync**（推荐）：
```cpp
SDL_Renderer* renderer = SDL_CreateRenderer(
    window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
```

2. **Triple Buffering**（三重缓冲）：
   - 减少 VSync 带来的延迟
   - 需要 GPU 驱动支持

### 5.6 渲染流程优化

**基本流程**：

```cpp
// 每帧渲染
SDL_RenderClear(renderer);        // 1. 清空
SDL_RenderCopy(renderer, tex, ...); // 2. 复制纹理
SDL_RenderPresent(renderer);      // 3. 显示
```

**优化技巧**：

```cpp
// 1. 只更新变化区域（如果知道的话）
SDL_Rect update_rect = {x, y, w, h};
SDL_UpdateYUVTexture(tex, &update_rect, ...);

// 2. 批量渲染多个元素
SDL_RenderCopy(renderer, bg_tex, ...);    // 背景
SDL_RenderCopy(renderer, video_tex, ...); // 视频
SDL_RenderCopy(renderer, ui_tex, ...);    // UI
SDL_RenderPresent(renderer);               // 一次显示

// 3. 保持显示比例
SDL_RenderSetLogicalSize(renderer, width, height);
// 窗口大小改变时自动缩放，保持比例
```

---

## 6. 代码详解

### 6.1 视频播放流程

<img src="../docs/images/video-pipeline.svg" width="100%"/>

### 6.2 完整代码结构

```cpp
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

// ========== 错误处理宏 ==========
#define CHECK_NULL(ptr, msg) \
    do { if (!(ptr)) { fprintf(stderr, "%s\n", msg); return 1; } } while(0)

#define CHECK_ERROR(ret, msg) \
    do { if ((ret) < 0) { \
        char errbuf[256]; av_strerror((ret), errbuf, sizeof(errbuf)); \
        fprintf(stderr, "%s: %s\n", msg, errbuf); return 1; \
    } } while(0)

// ========== 播放器类 ==========
class SimplePlayer {
public:
    SimplePlayer();
    ~SimplePlayer();
    
    bool Open(const char* filename);
    bool Play();
    void Close();
    
private:
    // FFmpeg
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    int video_stream_idx_ = -1;
    AVStream* video_stream_ = nullptr;
    
    // SDL
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    
    // 状态
    int width_ = 0, height_ = 0;
    bool running_ = false;
    int64_t start_time_ = 0;
};

// 实现...
```

### 6.3 逐模块详解

#### 初始化 FFmpeg

```cpp
bool SimplePlayer::Open(const char* filename) {
    // 1. 打开输入
    int ret = avformat_open_input(&fmt_ctx_, filename, nullptr, nullptr);
    CHECK_ERROR(ret, "无法打开文件");
    
    // 2. 获取流信息
    ret = avformat_find_stream_info(fmt_ctx_, nullptr);
    CHECK_ERROR(ret, "无法获取流信息");
    
    // 3. 查找视频流
    video_stream_idx_ = av_find_best_stream(
        fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    CHECK_ERROR(video_stream_idx_, "未找到视频流");
    
    video_stream_ = fmt_ctx->streams[video_stream_idx_];
    width_ = video_stream_>codecpar->width;
    height_ = video_stream_>codecpar->height;
    
    // 4. 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(
        video_stream_>codecpar->codec_id);
    CHECK_NULL(codec, "不支持的编解码器");
    
    codec_ctx_ = avcodec_alloc_context3(codec);
    CHECK_NULL(codec_ctx_, "无法分配解码器上下文");
    
    ret = avcodec_parameters_to_context(codec_ctx_, video_stream_>codecpar);
    CHECK_ERROR(ret, "无法复制编解码器参数");
    
    // 启用多线程解码
    codec_ctx_>thread_count = 4;
    codec_ctx_>thread_type = FF_THREAD_FRAME;
    
    ret = avcodec_open2(codec_ctx_, codec, nullptr);
    CHECK_ERROR(ret, "无法打开解码器");
    
    return true;
}
```

#### 初始化 SDL

```cpp
bool SimplePlayer::InitSDL() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL 初始化失败: %s\n", SDL_GetError());
        return false;
    }
    
    window_ = SDL_CreateWindow(
        "FFmpeg 播放器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    CHECK_NULL(window_, "无法创建窗口");
    
    renderer_ = SDL_CreateRenderer(
        window_, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    CHECK_NULL(renderer_, "无法创建渲染器");
    
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        width_, height_);
    CHECK_NULL(texture_, "无法创建纹理");
    
    // 保持显示比例
    SDL_RenderSetLogicalSize(renderer_, width_, height_);
    
    return true;
}
```

#### 解码循环

```cpp
bool SimplePlayer::Play() {
    if (!InitSDL()) return false;
    
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    running_ = true;
    start_time_ = av_gettime();
    
    while (running_) {
        // 处理事件
        HandleEvents();
        
        // 读取 packet
        int ret = av_read_frame(fmt_ctx_, packet);
        if (ret < 0) break;  // EOF 或错误
        
        if (packet->stream_index == video_stream_idx_) {
            // 发送到解码器
            ret = avcodec_send_packet(codec_ctx_, packet);
            if (ret < 0) {
                fprintf(stderr, "发送 packet 失败\n");
                continue;
            }
            
            // 接收所有可用的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx_, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                CHECK_ERROR(ret, "接收 frame 失败");
                
                // 同步和渲染
                SyncAndRender(frame);
            }
        }
        
        av_packet_unref(packet);
    }
    
    av_frame_free(&frame);
    av_packet_free(&packet);
    return true;
}
```

#### 同步与渲染

```cpp
void SimplePlayer::SyncAndRender(AVFrame* frame) {
    // 计算应该显示的时间
    int64_t pts_us = frame->pts * av_q2d(video_stream_>time_base) * 1000000;
    int64_t elapsed = av_gettime() - start_time_;
    
    // 等待到正确的时间
    if (pts_us > elapsed) {
        av_usleep(pts_us - elapsed);
    }
    
    // 更新纹理
    SDL_UpdateYUVTexture(
        texture_, nullptr,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]);
    
    // 渲染
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}
```

---

## 7. Pipeline 架构设计

### 7.1 为什么需要架构

**问题代码**：

```cpp
int main() {
    // 300 行混乱代码
    // - 改了这里，那里出问题
    // - 不敢重构，只能继续堆
    // - 无法单元测试
    // - 无法复用模块
}
```

**Pipeline 架构**：

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│   Demuxer    │────→│   Decoder    │────→│   Renderer   │
│   (解封装)    │     │   (解码)      │     │   (渲染)      │
└──────────────┘     └──────────────┘     └──────────────┘
       ↑                                           ↓
   读取文件                                    显示画面
```

**好处**：
1. **单一职责**：每个模块只做一件事
2. **可测试**：模块独立测试
3. **可替换**：实现相同接口即可替换
4. **可扩展**：添加新功能不修改旧代码

### 7.2 接口设计

```cpp
// ========== 错误码 ==========
enum class ErrorCode {
    OK = 0,
    INVALID_ARGUMENT,
    FILE_NOT_FOUND,
    FORMAT_NOT_SUPPORTED,
    CODEC_NOT_FOUND,
    DECODER_ERROR,
    RENDERER_ERROR,
    OUT_OF_MEMORY,
    UNKNOWN
};

// ========== Demuxer 接口 ==========
class IDemuxer {
public:
    virtual ~IDemuxer() = default;
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual ErrorCode ReadPacket(AVPacket* packet) = 0;
    virtual AVStream* GetVideoStream() const = 0;
    virtual int GetVideoStreamIndex() const = 0;
    virtual int64_t GetDurationMs() const = 0;
};

// ========== Decoder 接口 ==========
class IDecoder {
public:
    virtual ~IDecoder() = default;
    virtual ErrorCode Init(const AVCodecParameters* params) = 0;
    virtual ErrorCode SendPacket(const AVPacket* packet) = 0;
    virtual ErrorCode ReceiveFrame(AVFrame* frame) = 0;
    virtual void Flush() = 0;
};

// ========== Renderer 接口 ==========
class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual ErrorCode Init(int width, int height) = 0;
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    virtual ErrorCode Present() = 0;
    virtual void SetWindowTitle(const std::string& title) = 0;
};

// ========== Pipeline 接口 ==========
class IPipeline {
public:
    virtual ~IPipeline() = default;
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
    virtual ErrorCode Seek(int64_t ms) = 0;
    virtual bool IsPlaying() const = 0;
};
```

### 7.3 项目结构

```
chapter-01/
├── CMakeLists.txt
├── include/
│   └── live/
│       ├── base/
│       │   ├── error_code.h
│       │   └── ffmpeg_utils.h
│       ├── interfaces/
│       │   ├── idemuxer.h
│       │   ├── idecoder.h
│       │   ├── irenderer.h
│       │   └── ipipeline.h
│       └── impl/
│           ├── ffmpeg_demuxer.h
│           ├── ffmpeg_decoder.h
│           ├── sdl_renderer.h
│           └── simple_pipeline.h
├── src/
│   ├── base/
│   │   └── ffmpeg_utils.cpp
│   ├── impl/
│   │   ├── ffmpeg_demuxer.cpp
│   │   ├── ffmpeg_decoder.cpp
│   │   ├── sdl_renderer.cpp
│   │   └── simple_pipeline.cpp
│   └── main.cpp
└── tests/
    ├── test_demuxer.cpp
    ├── test_decoder.cpp
    └── test_pipeline.cpp
```

### 7.4 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(live-player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat>=58.0
    libavcodec>=58.0
    libavutil>=56.0
    libswscale>=5.0)
find_package(SDL2 REQUIRED)

# 库目标
add_library(live_core STATIC
    src/base/ffmpeg_utils.cpp
    src/impl/ffmpeg_demuxer.cpp
    src/impl/ffmpeg_decoder.cpp
    src/impl/sdl_renderer.cpp
    src/impl/simple_pipeline.cpp)

target_include_directories(live_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS})

target_link_libraries(live_core PUBLIC
    ${FFMPEG_LIBRARIES}
    SDL2::SDL2)

# 可执行目标
add_executable(player src/main.cpp)
target_link_libraries(player PRIVATE live_core)

# 测试
enable_testing()
add_subdirectory(tests)
```

---

## 8. 性能分析与优化

### 8.1 性能基准测试

#### 创建测试视频

```bash
# 1080p 30fps 测试视频
ffmpeg -f lavfi -i testsrc=duration=60:size=1920x1080:rate=30 \
       -pix_fmt yuv420p -c:v libx264 -preset medium -crf 23 \
       test_1080p.mp4

# 4K 30fps 测试视频
ffmpeg -f lavfi -i testsrc=duration=60:size=3840x2160:rate=30 \
       -pix_fmt yuv420p -c:v libx264 -preset medium -crf 23 \
       test_4k.mp4
```

#### 性能指标

| 指标 | 定义 | 目标值 |
|:---|:---|:---|
| **解码帧率** | 每秒解码帧数 | ≥ 目标帧率（如 30fps）|
| **渲染帧率** | 每秒显示帧数 | ≥ 目标帧率 |
| **CPU 占用** | 进程 CPU 使用率 | < 50% |
| **内存占用** | RSS 常驻内存 | < 200MB |
| **延迟** | 解码到显示耗时 | < 33ms（30fps）|
| **丢帧率** | 丢帧数/总帧数 | < 1% |

### 8.2 火焰图分析

#### 生成火焰图

```bash
# 1. 使用 perf 记录
perf record -g --call-graph=dwarf ./player test_1080p.mp4

# 2. 生成火焰图
perf script | ./stackcollapse-perf.pl | ./flamegraph.pl > flamegraph.svg

# 3. 浏览器打开分析
firefox flamegraph.svg
```

#### 典型播放器火焰图

```
典型的视频播放器性能分布：

┌──────────────────────────────────────────────────────────────┐
│  用户代码层                     5%                           │
│  ├─ main() 循环                                               │
│  ├─ 事件处理                                                  │
│  └─ 同步逻辑                                                  │
├──────────────────────────────────────────────────────────────┤
│  SDL2 渲染层                   25%                           │
│  ├─ SDL_UpdateYUVTexture()     15%  ← 纹理上传                │
│  ├─ SDL_RenderPresent()         8%  ← VSync 等待              │
│  └─ SDL_RenderCopy()            2%                            │
├──────────────────────────────────────────────────────────────┤
│  FFmpeg 解码层                 45%                           │
│  ├─ avcodec_receive_frame()    40%  ← 解码核心               │
│  │   ├─ h264_decode_mb()                                    │
│  │   ├─ idct_add()                                          │
│  │   └─ motion_compensation()                               │
│  └─ av_read_frame()             5%  ← 读取文件               │
├──────────────────────────────────────────────────────────────┤
│  系统层                        25%                           │
│  ├─ GPU 驱动                    15%  ← 纹理上传、显示          │
│  └─ 内核/文件系统               10%                           │
└──────────────────────────────────────────────────────────────┘
```

#### 优化方向

| 热点 | 占比 | 优化方案 |
|:---|:---|:---|
| 解码 | 40% | 启用硬件解码、多线程 |
| 纹理上传 | 15% | 使用零拷贝、GPU 解码 |
| VSync 等待 | 8% | 正常，无需优化 |

### 8.3 优化策略

#### 硬件解码

硬件解码可将 CPU 占用从 40-50% 降至 5-10%：

```cpp
// 查找硬件解码器
const AVCodec* codec = avcodec_find_decoder_by_name("h264_videotoolbox");  // macOS
// 或 "h264_vaapi"（Linux）
// 或 "h264_nvdec"（NVIDIA）

// 创建硬件设备
AVBufferRef* hw_device_ctx = nullptr;
av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);

codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
```

#### 多线程解码

```cpp
// 启用帧级多线程
codec_ctx->thread_count = 4;        // 4 个线程
codec_ctx->thread_type = FF_THREAD_FRAME;  // 帧级并行

// 对比（1080p 视频）：
// 单线程：解码 25ms/帧 → 40fps
// 4线程：解码 8ms/帧  → 125fps（大幅余量）
```

#### 内存池

频繁分配/释放内存影响性能，使用内存池：

```cpp
class FramePool {
public:
    AVFrame* Acquire() {
        if (!pool_.empty()) {
            AVFrame* frame = pool_.back();
            pool_.pop_back();
            return frame;
        }
        return av_frame_alloc();
    }
    
    void Release(AVFrame* frame) {
        av_frame_unref(frame);  // 清除数据，保留结构
        pool_.push_back(frame);
    }
    
private:
    std::vector<AVFrame*> pool_;
};
```

---

## 9. 调试技巧实战

### 9.1 GDB 调试

#### 基本命令

```bash
# 编译带调试信息
g++ -g -O0 player.cpp -o player $(pkg-config --cflags --libs ...)

# 启动调试
gdb ./player

# 常用命令
(gdb) break main              # 在 main 设置断点
(gdb) break Demuxer::Open     # 在类方法设置断点
(gdb) run test.mp4            # 运行程序
(gdb) next                    # 下一行（不进入函数）
(gdb) step                    # 下一步（进入函数）
(gdb) print variable          # 打印变量
(gdb) print *pointer          # 打印指针内容
(gdb) bt                      # 查看调用栈
(gdb) continue                # 继续运行
(gdb) quit                    # 退出
```

#### 调试 FFmpeg 错误

```cpp
// FFmpeg 函数返回负值表示错误
int ret = avcodec_send_packet(ctx, pkt);
if (ret < 0) {
    // 在 GDB 中：
    // (gdb) p ret
    // $1 = -1094995529
    
    char errbuf[256];
    av_strerror(ret, errbuf, sizeof(errbuf));
    printf("错误: %s\n", errbuf);  // "Invalid data found when processing input"
}
```

### 9.2 Valgrind 内存检测

#### 检测内存泄漏

```bash
# 基本检测
valgrind --leak-check=full ./player test.mp4

# 详细输出
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./player test.mp4 2>&1 | tee valgrind.log
```

#### 输出解读

```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 72,704 bytes in 1 blocks
==12345==   total heap usage: 10,234 allocs, 10,233 frees, 50,456,789 bytes allocated
==12345== 
==12345== 4,320 bytes in 3 blocks are definitely lost
==12345==    at 0x4C2FB55: calloc (vg_replace_malloc.c:711)
==12345==    by 0x4F12345: av_mallocz (mem.c:...)
==12345==    by 0x4F23456: av_frame_alloc (frame.c:...)
==12345==    by 0x401234: Player::Init (player.cpp:45)
```

**修复**：确保 `av_frame_free()` 被调用。

#### 检测内存越界

```bash
valgrind --tool=memcheck \
         --error-exitcode=1 \
         ./player test.mp4
```

### 9.3 Perf 性能分析

```bash
# 记录性能数据
perf record -g ./player test.mp4

# 查看热点函数
perf report

# 实时查看
perf top

# 统计缓存命中率
perf stat -e cache-misses,cache-references ./player test.mp4
```

---

## 10. 常见问题

### Q1: 编译报错 `undefined reference to`

**原因**：链接器找不到 FFmpeg 库。

**解决**：
```bash
# 检查 pkg-config
pkg-config --libs libavformat

# 如果为空，手动指定路径
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH

# 或手动编译
g++ player.cpp -o player \
    -I/usr/local/include \
    -L/usr/local/lib \
    -lavformat -lavcodec -lavutil -lSDL2
```

### Q2: 运行时崩溃（Segmentation fault）

**调试**：
```bash
# 使用 gdb
gdb ./player
run test.mp4
bt  # 查看崩溃位置

# 常见原因：
# 1. 文件不存在
# 2. 不是视频文件
# 3. 编解码器不支持
# 4. 内存越界访问
```

### Q3: 画面是绿色的

**原因 1**：YUV 数据没填对。
```cpp
// 检查数据指针
printf("Y: %p, U: %p, V: %p\n", 
    frame->data[0], frame->data[1], frame->data[2]);

// 检查格式
printf("格式: %s\n", av_get_pix_fmt_name((AVPixelFormat)frame->format));
// 应该是 "yuv420p"
```

**原因 2**：UV 顺序反了（YV12 vs IYUV）。
```cpp
// FFmpeg 输出 YUV420P（Y,U,V 顺序）
// SDL_PIXELFORMAT_IYUV = Y,U,V（匹配）
// SDL_PIXELFORMAT_YV12 = Y,V,U（不匹配，颜色异常）

// 修正
SDL_Texture* tex = SDL_CreateTexture(
    renderer, SDL_PIXELFORMAT_IYUV, ...);
```

### Q4: 视频播放太快或太慢

**原因**：没有根据 PTS 同步。

**解决**：
```cpp
// 计算帧应该显示的时间
int64_t pts_ms = frame->pts * av_q2d(stream->time_base) * 1000;
int64_t elapsed_ms = av_gettime() / 1000 - start_ms;

// 等待或丢帧
if (pts_ms > elapsed_ms + 40) {
    // 超前太多，等待
    av_usleep((pts_ms - elapsed_ms) * 1000);
} else if (pts_ms < elapsed_ms - 40) {
    // 落后太多，丢帧
    continue;
}
```

### Q5: 音频视频不同步

**简单同步策略**（音频为主时钟）：
```cpp
// 获取音频当前播放时间（需要音频播放模块）
int64_t audio_pts = audio_player.GetCurrentPts();

// 计算视频应该显示的帧
int64_t video_pts = frame->pts * av_q2d(video_stream->time_base) * 1000;
int64_t diff = video_pts - audio_pts;

if (diff > 40) {
    // 视频超前，等待
    av_usleep(diff * 1000);
} else if (diff < -40) {
    // 视频落后，丢帧
    skip_frame = true;
}
```

### Q6: 如何支持更多格式

FFmpeg 会自动检测格式，无需修改代码：
```cpp
// 支持 MP4、FLV、AVI、MKV、MOV 等
// 支持 H.264、H.265、VP9、AV1 等编码

// 如果需要限制格式
AVInputFormat* fmt = av_find_input_format("mp4");
avformat_open_input(&ctx, url.c_str(), fmt, nullptr);
```

---

## 11. 下一步

本章完成了**同步单线程**播放器，存在的问题：

```
播放 4K 视频时（假设）：
- 文件读取：5ms
- 解码：25ms
- 渲染：8ms
- 总时间（串行）：38ms → 26fps（卡顿！）

解决方案：多线程并行
- 线程 A：读取文件（5ms）
- 线程 B：解码（25ms）
- 线程 C：渲染（8ms）
- 并行后帧间隔：25ms → 40fps（流畅）
```

**第 2 章预告：异步多线程架构**
- 生产者-消费者队列设计
- 线程安全的数据结构
- 帧队列管理（固定大小、超时丢帧）
- 多线程同步原语（mutex、condition_variable）

---

## 附录

### A. 完整术语表

| 术语 | 解释 | 首次出现 |
|:---|:---|:---|
| **FFmpeg** | 开源音视频处理库 | 1.1 |
| **Demuxer** | 解封装器，从文件提取压缩数据 | 2.2 |
| **Decoder** | 解码器，解压数据为原始图像 | 2.2 |
| **YUV** | 亮度+色度像素格式 | 2.2.3 |
| **YUV420** | 4:2:0 采样，U/V 分辨率减半 | 3.2 |
| **I/P/B 帧** | 关键帧/预测帧/双向帧 | 2.3 |
| **GOP** | Group of Pictures，图像组 | 2.3 |
| **PTS** | Presentation Time Stamp，显示时间戳 | 4.2.4 |
| **DTS** | Decoding Time Stamp，解码时间戳 | 4.2.5 |
| **DCT** | 离散余弦变换，压缩核心技术 | 2.2.1 |
| **量化** | 丢弃不重要信息，有损压缩 | 2.2.1 |
| **运动估计** | 寻找帧间运动向量 | 2.2.2 |
| **time_base** | 时间基，PTS 的单位 | 4.2.3 |
| **linesize** | 行宽，包含对齐填充 | 3.3 |

### B. 参考资源

**官方文档**
- FFmpeg 文档：https://ffmpeg.org/documentation.html
- SDL2 Wiki：https://wiki.libsdl.org/
- H.264 规范：ISO/IEC 14496-10

**书籍**
- 《数字视频处理》— 米尔福特
- 《H.264/AVC 视频编解码技术详解》

**在线课程**
- Stanford EE398A: Video Processing

### C. 源码仓库

本书配套代码：https://github.com/chapin666/live-system-book

---

**本章完**
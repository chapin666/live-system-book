# 第一章：Pipeline 架构与本地播放（深度版）

> **目标**：从零开始，理解视频播放的每一个细节——从数学原理到代码实现，从 FFmpeg 源码到性能优化。

**预计时间**：
- 快速开始（30 分钟）：先跑起来
- 理论深度（3 小时）：读懂原理
- 源码解析（2 小时）：理解实现
- 动手实践（2 小时）：自己实现

---

## 目录

1. [快速开始](#1-快速开始) — 先看到画面
2. [视频压缩的数学原理](#2-视频压缩的数学原理) — 从源头理解
3. [FFmpeg 架构解析](#3-ffmpeg-架构解析) — 数据结构全解
4. [解码器内部机制](#4-解码器内部机制) — 状态机与缓冲区
5. [SDL2 渲染原理](#5-sdl2-渲染原理) — GPU 与显示
6. [代码解剖](#6-代码解剖) — 逐行深入
7. [Pipeline 架构设计](#7-pipeline-架构设计) — 工程实践
8. [完整实现](#8-完整实现) — 每一行代码
9. [性能分析与优化](#9-性能分析与优化) — 数据说话
10. [调试技巧实战](#10-调试技巧实战) — 问题定位
11. [常见问题](#11-常见问题)
12. [掌握度检查与进阶](#12-掌握度检查与进阶)
13. [下一步](#13-下一步)

---

## 1. 快速开始

### 1.1 环境要求

| 组件 | 版本 | 说明 |
|:---|:---|:---|
| FFmpeg | **4.4.x** | 本章基于 4.4.2 版本编写 |
| SDL2 | 2.0.16+ | |
| CMake | 3.16+ | |
| 编译器 | GCC 9+ / Clang 10+ / MSVC 2019+ | 完整 C++17 支持 |
| 系统 | macOS 11+ / Ubuntu 20.04+ / Windows 10+ | |

**验证 FFmpeg 版本**：
```bash
ffmpeg -version | head -3
# 应输出类似：ffmpeg version 4.4.2
```

**macOS 安装**：
```bash
# 使用 Homebrew 安装指定版本
brew install ffmpeg@4 sdl2 cmake

# 如果安装了多个版本，设置路径
export PATH="/usr/local/opt/ffmpeg@4/bin:$PATH"
export PKG_CONFIG_PATH="/usr/local/opt/ffmpeg@4/lib/pkgconfig"
```

**Ubuntu 安装**：
```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev \
    libavutil-dev libswscale-dev libsdl2-dev cmake gdb valgrind

# 验证安装
pkg-config --modversion libavformat  # 应输出 58.x.x (FFmpeg 4.x)
```

**Windows (MSYS2) 安装**：
```bash
# 在 MSYS2 MinGW 64-bit 终端执行
pacman -Syu
pacman -S mingw-w64-x86_64-ffmpeg \
          mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-gdb

# 添加到环境变量（添加到 ~/.bashrc）
export PATH="/mingw64/bin:$PATH"
```

### 1.2 最简播放器（50 行）

创建 `minimal_player.cpp`：

```cpp
// minimal_player.cpp - 最简视频播放器
#include <SDL2/SDL.h>
#include <cstdio>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <video_file>\n", argv[0]);
        return 1;
    }

    // ========== 1. 打开文件 ==========
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
        return 1;
    }

    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error: Cannot find stream info\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // ========== 2. 找到视频流 ==========
    int video_stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, 
                                                -1, -1, nullptr, 0);
    if (video_stream_idx < 0) {
        fprintf(stderr, "Error: No video stream found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    AVStream* stream = fmt_ctx->streams[video_stream_idx];
    printf("Video: %dx%d @ %.2ffps\n", 
           stream->codecpar->width, 
           stream->codecpar->height,
           av_q2d(stream->avg_frame_rate));

    // ========== 3. 初始化解码器 ==========
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Error: Codec not found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Error: Cannot open codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // ========== 4. 创建 SDL 窗口 ==========
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codec_ctx->width, codec_ctx->height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);

    // ========== 5. 解码播放循环 ==========
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool running = true;
    int frame_count = 0;

    while (running && av_read_frame(fmt_ctx, packet) >= 0) {
        // 处理窗口事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && 
                event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        // 只处理视频包
        if (packet->stream_index != video_stream_idx) {
            av_packet_unref(packet);
            continue;
        }

        // 发送到解码器
        ret = avcodec_send_packet(codec_ctx, packet);
        av_packet_unref(packet);
        if (ret < 0) continue;

        // 接收解码后的帧
        while (avcodec_receive_frame(codec_ctx, frame) == 0) {
            SDL_UpdateYUVTexture(texture, nullptr,
                frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            frame_count++;
        }
    }

    printf("Total frames: %d\n", frame_count);

    // ========== 6. 清理资源 ==========
    av_frame_free(&frame);
    av_packet_free(&packet);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}
```

### 1.3 编译与运行

```bash
# 1. 创建测试视频（彩色条纹）
ffmpeg -f lavfi -i testsrc=duration=10:size=1280x720:rate=30 \
       -pix_fmt yuv420p -c:v libx264 -preset fast \
       -crf 23 test_video.mp4

# 2. 编译播放器
g++ -O2 -std=c++17 minimal_player.cpp -o minimal_player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 3. 运行
./minimal_player test_video.mp4
```

**预期效果**：
```
Video: 1280x720 @ 30.00fps
Total frames: 300
```

窗口显示彩色条纹，按 ESC 或关闭窗口退出。

---

## 2. 视频压缩的数学原理

### 2.1 信息论基础

#### 2.1.1 信息熵

**香农熵（Shannon Entropy）**：衡量信息的不确定性。

$$H(X) = -\sum_{i=1}^{n} p(x_i) \log_2 p(x_i)$$

其中：
- $p(x_i)$ 是符号 $x_i$ 出现的概率
- 单位是比特（bits）

**例子**：
```
场景 A：抛公平硬币，P(正面)=0.5, P(反面)=0.5
H = -(0.5 × log₂0.5 + 0.5 × log₂0.5) = 1 bit

场景 B：抛不公平硬币，P(正面)=0.9, P(反面)=0.1
H = -(0.9 × log₂0.9 + 0.1 × log₂0.1) ≈ 0.47 bit
```

**结论**：概率分布越不均匀，熵越小，压缩潜力越大。

#### 2.1.2 视频数据的统计特性

自然视频的像素值分布：
- **空间相关性**：相邻像素值相近
- **时间相关性**：连续帧变化小
- **色度相关性**：U/V 通道比 Y 通道变化小

这导致视频数据的熵远低于随机数据，为压缩提供理论基础。

### 2.2 离散余弦变换（DCT）

#### 2.2.1 从傅里叶变换到 DCT

**傅里叶变换**：将信号从时域转换到频域。

**离散余弦变换（DCT）**：只使用余弦分量，更适合图像（实数、对称）。

**一维 DCT-II 公式**（图像压缩使用）：

$$X_k = \alpha(k) \sum_{n=0}^{N-1} x_n \cos\left[\frac{\pi}{N}\left(n + \frac{1}{2}\right)k\right]$$

其中：
- $x_n$：输入信号（像素值）
- $X_k$：变换后的系数（频率分量）
- $\alpha(k)$：归一化系数，$\alpha(0) = \sqrt{\frac{1}{N}}$，$\alpha(k) = \sqrt{\frac{2}{N}}$（k>0）

**二维 DCT**（图像使用）：

$$X_{u,v} = \alpha(u)\alpha(v) \sum_{x=0}^{N-1}\sum_{y=0}^{N-1} x_{x,y} 
\cos\left[\frac{\pi}{N}\left(x + \frac{1}{2}\right)u\right]
\cos\left[\frac{\pi}{N}\left(y + \frac{1}{2}\right)v\right]$$

#### 2.2.2 DCT 的物理意义

**例子**：8×8 块的 DCT 变换

```
原始 8×8 像素块（亮度值 0-255）：
┌────┬────┬────┬────┬────┬────┬────┬────┐
│140 │142 │143 │141 │139 │138 │140 │141 │
│141 │143 │144 │142 │140 │139 │141 │142 │
│142 │144 │145 │143 │141 │140 │142 │143 │
│... │... │... │... │... │... │... │... │
└────┴────┴────┴────┴────┴────┴────┴────┘

DCT 变换后的系数：
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│ 1145.0  │  -12.3  │    3.2  │   -0.8  │    0.5  │   -0.3  │    0.2  │   -0.1  │  ← DC 分量 + 低频 AC
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤
│  -15.2  │    2.1  │   -0.9  │    0.4  │   -0.2  │    0.1  │   -0.1  │    0.0  │
│    4.8  │   -1.3  │    0.6  │   -0.3  │    0.1  │    0.0  │    0.0  │    0.0  │
│   -2.1  │    0.8  │   -0.4  │    0.2  │   -0.1  │    0.0  │    0.0  │    0.0  │
│    1.0  │   -0.5  │    0.3  │   -0.1  │    0.0  │    0.0  │    0.0  │    0.0  │
│   -0.6  │    0.3  │   -0.2  │    0.1  │    0.0  │    0.0  │    0.0  │    0.0  │
│    0.4  │   -0.2  │    0.1  │    0.0  │    0.0  │    0.0  │    0.0  │    0.0  │
│   -0.2  │    0.1  │   -0.1  │    0.0  │    0.0  │    0.0  │    0.0  │    0.0  │
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
```

**关键观察**：
1. **DC 分量**（左上角）：代表块的平均亮度，值最大
2. **低频 AC**：相邻几个值较大，代表缓慢变化
3. **高频 AC**：右下角接近 0，代表快速变化（细节）

**能量集中**：大部分能量集中在左上角，右下角可以丢弃。

#### 2.2.3 H.264 中的整数 DCT

为了硬件实现效率，H.264 使用**整数 DCT**近似：

```
H.264 4×4 整数变换矩阵：

┌────┬────┬────┬────┐
│ 1  │ 1  │ 1  │ 1  │
│ 2  │ 1  │ -1 │ -2 │
│ 1  │ -1 │ -1 │ 1  │
│ 1  │ -2 │ 2  │ -1 │
└────┴────┴────┴────┘
```

**优势**：
- 只用整数加减和移位，硬件实现简单
- 完全可逆，无浮点误差累积
- 与量化结合，减少计算量

### 2.3 量化

#### 2.3.1 量化原理

**量化**：将连续值映射到离散值，引入可控的信息损失。

**标量量化公式**：

$$Q(x) = \text{round}\left(\frac{x}{q}\right) \times q$$

其中 $q$ 是量化步长。

**例子**（$q=10$）：
```
原始值:  142  148  151  143  ...
量化后:  140  150  150  140  ...
误差:    -2   +2   -1   -3   ...
```

#### 2.3.2 H.264 量化矩阵

H.264 使用不同的量化步长对不同频率分量：

```
H.264 默认量化矩阵（亮度）：

┌────┬────┬────┬────┐
│ 16 │ 11 │ 10 │ 16 │  ← DC 分量用较小步长（保护）
│ 12 │ 12 │ 14 │ 19 │
│ 14 │ 13 │ 16 │ 24 │
│ 14 │ 17 │ 22 │ 29 │  ← 高频用较大步长（丢弃）
└────┴────┴────┴────┘
```

**规律**：
- 左上角（低频）：步长小，精度高
- 右下角（高频）：步长大，精度低甚至丢弃

**量化参数 QP（Quantization Parameter）**：
- QP 增加 6 → 比特率减半
- QP 范围：0-51，常用 20-40
- QP=0：无损（仍有 DCT 带来的微小误差）

### 2.4 运动估计与补偿

#### 2.4.1 时间冗余

视频帧间通常只有小部分区域变化：

```
帧 N（参考帧）          帧 N+1（当前帧）
┌──────────────┐       ┌──────────────┐
│              │       │              │
│   🚗  →      │       │      🚗     │  汽车右移
│              │       │              │
│    ☁️        │       │       ☁️    │  云朵漂移
│              │       │              │
└──────────────┘       └──────────────┘
```

**不是重新编码整个帧，而是编码"差异"！**

#### 2.4.2 运动向量

**宏块（Macroblock）**：16×16 像素的处理单元

**运动估计**：在参考帧中找到最匹配的块。

```
当前帧的宏块 (x, y) ────────┐
                            │ 运动向量 (mv_x, mv_y)
                            ↓
参考帧的宏块 (x+mv_x, y+mv_y)
```

**搜索算法**：
- **全搜索**：尝试所有可能位置，最优但慢
- **三步搜索**：快速近似，适合实时编码
- **钻石搜索**：H.264 常用，平衡速度与精度

#### 2.4.3 运动补偿残差

编码的不是原始像素，而是：

$$
\text{残差} = \text{当前块} - \text{预测块（参考帧对应位置）}$$

残差通常很小（变化区域），DCT 后大部分系数为 0，极大提高压缩率。

### 2.5 熵编码

#### 2.5.1 变长编码（VLC）

**思想**：高频符号用短编码，低频符号用长编码。

**霍夫曼编码示例**：
```
符号频率:  A:50%  B:25%  C:15%  D:10%

编码分配:
A: 0        (1 bit)
B: 10       (2 bits)
C: 110      (3 bits)
D: 111      (3 bits)

平均码长 = 0.5×1 + 0.25×2 + 0.15×3 + 0.10×3 = 1.75 bits
定长编码需要 2 bits，节省 12.5%
```

#### 2.5.2 CABAC（上下文自适应二进制算术编码）

H.264 高级特性，比霍夫曼编码效率更高。

**算术编码**：将整个消息编码为一个浮点数。

**上下文自适应**：根据周围已编码符号调整概率模型。

**压缩增益**：相比 VLC 节省 10-20% 比特率，但计算复杂度高。

### 2.6 本章节的编码示例

**一个 4×4 块在 H.264 中的编码流程**：

```
原始像素
    ↓
整数 DCT 变换 ───────→ 16 个系数 (DC + 15 AC)
    ↓
量化（QP=28）────────→ 大部分 AC 变为 0
    ↓
Zigzag 扫描 ─────────→ 一维序列 (DC 在前，高频在后)
    ↓
游程编码 ────────────→ (跳过0的个数, 非零值) 对
    ↓
熵编码 (CABAC/VLC) ──→ 最终比特流
```

---

## 3. FFmpeg 架构解析

### 3.1 FFmpeg 整体架构

```
FFmpeg 库结构：

┌─────────────────────────────────────────────────────────────┐
│                     libavformat (封装)                       │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │   MP4      │  │   FLV      │  │   AVI      │  ...       │
│  │  demuxer   │  │  demuxer   │  │  demuxer   │            │
│  └────────────┘  └────────────┘  └────────────┘            │
│  解封装：从容器格式提取音视频流                              │
├─────────────────────────────────────────────────────────────┤
│                     libavcodec (编解码)                      │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │   H.264    │  │   H.265    │  │    AAC     │  ...       │
│  │  decoder   │  │  decoder   │  │  decoder   │            │
│  └────────────┘  └────────────┘  └────────────┘            │
│  编解码：压缩数据 ↔ 原始数据                                 │
├─────────────────────────────────────────────────────────────┤
│                     libavutil (工具)                         │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │   内存     │  │   数学     │  │   时间     │            │
│  │  管理      │  │  运算      │  │  处理      │            │
│  └────────────┘  └────────────┘  └────────────┘            │
│  通用工具函数                                                │
├─────────────────────────────────────────────────────────────┤
│                     libswscale (转换)                        │
│  YUV ↔ RGB, 缩放, 格式转换                                   │
├─────────────────────────────────────────────────────────────┤
│                     libswresample (重采样)                   │
│  音频采样率/格式转换                                          │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 AVFormatContext 详解

#### 3.2.1 结构体定义（FFmpeg 4.4）

```c
// libavformat/avformat.h

typedef struct AVFormatContext {
    // ========== 基本属性 ==========
    const AVClass *av_class;           // 用于日志和选项
    struct AVInputFormat *iformat;     // 输入格式（demuxer）
    struct AVOutputFormat *oformat;    // 输出格式（muxer）
    
    void *priv_data;                   // 格式的私有数据
    
    // ========== IO 相关 ==========
    AVIOContext *pb;                   // IO 上下文（文件/网络）
    int io_flags;                      // IO 标志（读取/写入）
    
    // ========== 流信息 ==========
    unsigned int nb_streams;           // 流数量
    AVStream **streams;                // 流数组指针
    
    // ========== 元数据 ==========
    char *url;                         // 文件/流 URL
    int64_t start_time;                // 第一帧的 PTS
    int64_t duration;                  // 总时长（AV_TIME_BASE）
    int64_t bit_rate;                  // 总码率（bits/s）
    
    AVDictionary *metadata;            // 元数据（标题、作者等）
    
    // ========== 内部状态 ==========
    int flags;                         // 格式标志
    int64_t data_offset;               // 数据起始偏移
    
    // ... 更多字段省略
} AVFormatContext;
```

#### 3.2.2 关键字段详解

| 字段 | 类型 | 用途 | 生命周期 |
|:---|:---|:---|:---|
| `av_class` | `AVClass*` | 用于反射和日志分类 | 初始化后不变 |
| `iformat` | `AVInputFormat*` | 指向具体的 demuxer（如 mov_demuxer） | `avformat_open_input` 设置 |
| `pb` | `AVIOContext*` | 抽象 IO 层，支持文件/网络/内存 | 打开时创建，关闭时释放 |
| `nb_streams` | `unsigned int` | 音视频流总数 | 打开文件后确定 |
| `streams` | `AVStream**` | 流数组，streams[i] 是第 i 个流 | 与 fmt_ctx 同生命周期 |
| `duration` | `int64_t` | 总时长，单位 AV_TIME_BASE（1/1,000,000 秒）| 调用 `avformat_find_stream_info` 后 |
| `metadata` | `AVDictionary*` | 键值对形式的元数据 | 可选，可能为空 |

#### 3.2.3 内存布局

```
AVFormatContext 内存结构：

┌─────────────────────────────────────┐
│          AVFormatContext            │  ← 主结构体（约 400 字节）
├─────────────────────────────────────┤
│  AVInputFormat (mov_demuxer)        │  ← 指向全局的 demuxer 定义
├─────────────────────────────────────┤
│  AVIOContext (文件 IO)              │  ← 文件句柄、缓冲区等
│  ├── buffer (32KB 默认)             │
│  └── 文件描述符 / 网络 socket        │
├─────────────────────────────────────┤
│  AVStream[0] (视频流)               │  ← 动态分配
│  ├── codecpar (编码参数)            │
│  ├── time_base (时间基)             │
│  └── metadata (流级元数据)          │
├─────────────────────────────────────┤
│  AVStream[1] (音频流)               │
├─────────────────────────────────────┤
│  AVStream[2] (字幕流，可选)         │
└─────────────────────────────────────┘
```

### 3.3 AVStream 详解

```c
typedef struct AVStream {
    int index;                         // 流索引（0, 1, 2...）
    int id;                            // 格式特定的流 ID
    
    AVCodecParameters *codecpar;       // 编码参数（新 API）
    
    // ========== 时间 ==========
    AVRational time_base;              // 时间基（分数形式）
    int64_t start_time;                // 第一帧 PTS
    int64_t duration;                  // 流时长（以 time_base 为单位）
    
    // ========== 帧率 ==========
    AVRational r_frame_rate;           // 实际帧率（最可靠）
    AVRational avg_frame_rate;         // 平均帧率
    AVRational sample_aspect_ratio;    // 采样宽高比（SAR）
    
    // ========== 显示 ==========
    int disposition;                   // 显示属性（默认/隐藏等）
    
    // ========== 解码器（旧 API，已废弃）==========
    struct AVCodecContext *codec;      // 不要直接使用！
    
    // ... 更多字段
} AVStream;
```

**关键字段：time_base**

```c
typedef struct AVRational {
    int num;  // 分子
    int den;  // 分母
} AVRational;

// time_base = 1/1000 表示 PTS 单位为毫秒
// time_base = 1/90000 表示 PTS 单位为 1/90000 秒（90kHz 时钟）

// 转换函数
double av_q2d(AVRational a);  // 分数转 double
```

### 3.4 AVCodecContext 详解

```c
typedef struct AVCodecContext {
    // ========== 编解码器 ==========
    const AVClass *av_class;
    struct AVCodec *codec;             // 编解码器
    void *priv_data;                   // 私有数据（如 x264 参数）
    
    // ========== 视频参数 ==========
    int width, height;                 // 视频分辨率
    AVRational sample_aspect_ratio;    // 采样宽高比
    AVPixelFormat pix_fmt;             // 像素格式（YUV420P 等）
    AVRational framerate;              // 帧率
    
    // ========== 编码参数 ==========
    int bit_rate;                      // 目标码率
    int rc_buffer_size;                // 码率控制缓冲区
    float rc_max_rate;                 // 最大码率
    
    // ========== 解码状态 ==========
    int thread_count;                  // 解码线程数
    int thread_type;                   // 线程类型（帧/切片）
    
    // ========== 内部缓冲区 ==========
    uint8_t *extradata;                // 编解码器额外数据（如 SPS/PPS）
    int extradata_size;
    
    // ... 更多字段
} AVCodecContext;
```

### 3.5 AVPacket 详解

```c
typedef struct AVPacket {
    AVBufferRef *buf;                  // 数据缓冲区引用（引用计数）
    int64_t pts;                       // 显示时间戳
    int64_t dts;                       // 解码时间戳
    uint8_t *data;                     // 数据指针
    int size;                          // 数据大小（字节）
    int stream_index;                  // 所属流索引
    int flags;                         // 标志（关键帧等）
    AVPacketSideData *side_data;       // 附加数据
    int side_data_elems;
    int64_t duration;                  // 包持续时间
    int64_t pos;                       // 文件位置
} AVPacket;
```

**内存管理**：
- `buf` 使用引用计数，多个 Packet 可共享同一缓冲区
- `av_packet_alloc()` 只分配结构体，不分配数据缓冲区
- `av_packet_unref()` 减少引用计数，为 0 时释放

### 3.6 AVFrame 详解

```c
typedef struct AVFrame {
    // ========== 数据 ==========
    uint8_t *data[AV_NUM_DATA_POINTERS];      // 各平面数据指针
    int linesize[AV_NUM_DATA_POINTERS];       // 各平面行字节数
    
    // ========== 格式 ==========
    int width, height;                         // 分辨率
    int nb_samples;                            // 音频：采样数
    AVPixelFormat format;                      // 像素/采样格式
    
    // ========== 时间 ==========
    int64_t pts;                               // 显示时间戳
    AVRational time_base;
    int64_t best_effort_timestamp;             // 估计的 PTS
    
    // ========== 引用计数 ==========
    uint8_t **extended_data;                   // 扩展数据指针
    int nb_extended_buf;
    AVBufferRef **extended_buf;
    
    // ========== 帧属性 ==========
    int key_frame;                             // 是否关键帧
    AVPictureType pict_type;                   // 帧类型（I/P/B）
    AVRational sample_aspect_ratio;            // SAR
    int64_t pkt_pos;                           // 来源 packet 位置
    
    // ... 更多字段
} AVFrame;
```

**YUV420P 内存布局**：

```
AVFrame for YUV420P 1920x1080:

┌─────────────────────────────────────────────────────────────┐
│ data[0] ───────────────────────────────────────────────┐    │
│   Y 平面: 1920 × 1080 = 2,073,600 字节                 │    │
│   linesize[0] = 1920 (可能填充到 2048 等)              │    │
├────────────────────────────────────────────────────────┤    │
│ data[1] ───────────────────────────────────────────┐   │    │
│   U 平面: 960 × 540 = 518,400 字节                 │   │    │
│   linesize[1] = 960                                │   │    │
├────────────────────────────────────────────────────┤   │    │
│ data[2] ───────────────────────────────────────┐   │   │    │
│   V 平面: 960 × 540 = 518,400 字节             │   │   │    │
│   linesize[2] = 960                            │   │   │    │
└────────────────────────────────────────────────┘   │   │    │
                                                     │   │    │
buffer (AVBufferRef) ←───────────────────────────────┘   │    │
  ↓ 引用计数管理                                         │    │
av_buffer_alloc() ───────────────────────────────────────┘    │
                                                              │
total_size = 2,073,600 + 518,400 + 518,400 = 3,110,400 字节   │
                                                              │
AVFrame 结构体本身只占约 200 字节，数据通过 buf 引用 ──────────┘
```

---

（由于内容过长，我将继续编写后续章节）

## 4. 解码器内部机制

### 4.1 解码器状态机

FFmpeg 解码器采用**异步推拉模型**：

```
解码器状态：

      ┌─────────────────────────────────────────────────────────┐
      │                                                         ↓
┌─────────┐    SendPacket    ┌─────────────┐    ReceiveFrame    ┌─────────┐
│  UNINIT │ ───────────────→ │  RECEIVING  │ ─────────────────→ │ OUTPUT  │
│  未初始化│                  │  接收包中   │                    │ 输出帧  │
└─────────┘                  └─────────────┘                    └─────────┘
                                  │                                   │
                                  │         SendPacket (EAGAIN)       │
                                  │         缓冲区满                  │
                                  │←──────────────────────────────────┘
                                  │
                                  │         ReceiveFrame (EAGAIN)
                                  │         无帧可输出
                                  ↓
                            ┌─────────────┐
                            │   DRAINING  │  ← 发送空包 (flush)
                            │   冲刷中    │
                            └─────────────┘
```

### 4.2 内部缓冲区

```
解码器内部结构：

┌──────────────────────────────────────────────────────────────┐
│                     AVCodecContext                           │
├──────────────────────────────────────────────────────────────┤
│  Codec (H.264 decoder)                                       │
│  ├── Parser：解析 NALU，提取 SPS/PPS/Slice 信息              │
│  ├── DSP：解压缩，反变换，运动补偿                           │
│  └── Frame Pool：解码后的帧缓冲池                          │
├──────────────────────────────────────────────────────────────┤
│  输入缓冲区（压缩数据）                                      │
│  ┌─────────┬─────────┬─────────┬─────────┐                   │
│  │ Packet0 │ Packet1 │ Packet2 │ Packet3 │ ...               │
│  │ SPS+PPS │   IDR   │    P    │    P    │                   │
│  └─────────┴─────────┴─────────┴─────────┘                   │
│       ↑                                       ↓               │
│   avcodec_send_packet()              avcodec_receive_frame() │
├──────────────────────────────────────────────────────────────┤
│  输出缓冲区（原始帧）                                        │
│  ┌─────────┬─────────┬─────────┬─────────┐                   │
│  │ Frame0  │ Frame1  │ Frame2  │ Frame3  │ ...               │
│  │  YUV    │   YUV   │   YUV   │   YUV   │                   │
│  └─────────┴─────────┴─────────┴─────────┘                   │
└──────────────────────────────────────────────────────────────┘
```

### 4.3 多线程解码

**FFmpeg 支持三种多线程模式**：

| 模式 | 说明 | 适用场景 |
|:---|:---|:---|
| **FF_THREAD_FRAME** | 帧级并行，多个帧同时解码 | 大多数编码器 |
| **FF_THREAD_SLICE** | 切片级并行，单帧内多线程 | H.264 支持 |
| **FF_THREAD_NONE** | 单线程 | 调试/兼容性 |

**启用多线程**：
```cpp
codec_ctx->thread_count = 4;  // 使用 4 个线程
codec_ctx->thread_type = FF_THREAD_FRAME;
avcodec_open2(codec_ctx, codec, nullptr);
```

---

（继续编写更多内容...）

## 8. 完整实现

### 8.1 目录结构

```
chapter-01/
├── CMakeLists.txt
├── include/
│   └── live/
│       ├── base/
│       │   ├── pipeline.h
│       │   └── ffmpeg_utils.h
│       └── core/
│           ├── demuxer.h
│           ├── decoder.h
│           ├── renderer.h
│           └── simple_pipeline.h
├── src/
│   ├── base/
│   │   └── ffmpeg_utils.cpp
│   └── core/
│       ├── demuxer.cpp
│       ├── decoder.cpp
│       ├── renderer.cpp
│       └── simple_pipeline.cpp
│   └── main.cpp
└── tests/
    └── test_pipeline.cpp
```

### 8.2 完整头文件

（此处展示所有头文件的完整内容，每个都有详细注释）

### 8.3 完整实现文件

（此处展示所有 cpp 文件的完整内容）

---

## 9. 性能分析与优化

（火焰图分析、热点优化等）

---

## 10. 调试技巧实战

（gdb、valgrind、perf 实战）

---

由于篇幅限制，完整版本需要分多次编写。以上是深度扩充的大纲和部分内容，预计最终将达到 **4000+ 行**，涵盖：

1. 完整的数学原理（DCT、量化、运动估计公式推导）
2. FFmpeg 核心数据结构完全解析（每个字段的用途）
3. 解码器内部状态机和缓冲区管理
4. GPU 渲染原理（SDL2 底层实现）
5. 每一行代码的详细注释
6. 性能分析的实战案例（火焰图解读）

需要我继续完成剩余内容吗？
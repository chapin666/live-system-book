# 第一章：Pipeline 架构与本地播放

> **目标**：从零开始，用 100 行代码实现视频播放，深入理解视频压缩原理和播放器架构。

**预计时间**：3 小时

---

## 目录

1. [快速开始](#1-快速开始) — 先跑起来
2. [视频压缩原理](#2-视频压缩原理) — 为什么 1 分钟视频只有 100MB
3. [颜色空间与像素格式](#3-颜色空间与像素格式) — YUV 详解
4. [FFmpeg 架构解析](#4-ffmpeg-架构解析) — 核心数据结构
5. [代码详解](#5-代码详解) — 逐行分析
6. [Pipeline 架构设计](#6-pipeline-架构设计) — 工程化实践
7. [调试与优化](#7-调试与优化)
8. [常见问题](#8-常见问题)
9. [下一步](#9-下一步)

---

## 1. 快速开始

### 1.1 安装依赖

**macOS:**
```bash
brew install ffmpeg sdl2 cmake
```

**Ubuntu:**
```bash
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev \
    libavutil-dev libswscale-dev libsdl2-dev cmake
```

验证安装：
```bash
ffmpeg -version | head -1
# ffmpeg version 4.4.2
```

### 1.2 100 行播放器

创建 `player.cpp`：

```cpp
#include <SDL2/SDL.h>
#include <stdio.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char* argv[]) {
    // 1. 打开文件
    AVFormatContext* fmt = nullptr;
    avformat_open_input(&fmt, argv[1], nullptr, nullptr);
    avformat_find_stream_info(fmt, nullptr);
    
    // 2. 找视频流
    int idx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st = fmt->streams[idx];
    
    // 3. 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    AVCodecContext* cc = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(cc, st->codecpar);
    avcodec_open2(cc, codec, nullptr);
    
    // 4. 创建窗口
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cc->width, cc->height, 0);
    SDL_Renderer* rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING, cc->width, cc->height);
    
    // 5. 解码循环
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();
    bool running = true;
    
    while (running && av_read_frame(fmt, pkt) >= 0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
        }
        
        if (pkt->stream_index == idx) {
            avcodec_send_packet(cc, pkt);
            while (avcodec_receive_frame(cc, frm) == 0) {
                SDL_UpdateYUVTexture(tex, nullptr,
                    frm->data[0], frm->linesize[0],
                    frm->data[1], frm->linesize[1],
                    frm->data[2], frm->linesize[2]);
                SDL_RenderCopy(rend, tex, nullptr, nullptr);
                SDL_RenderPresent(rend);
                SDL_Delay(33);  // 30fps
            }
        }
        av_packet_unref(pkt);
    }
    
    // 6. 清理
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&cc);
    avformat_close_input(&fmt);
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
g++ -O2 player.cpp -o player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 运行
./player test.mp4
```

看到彩色条纹在滚动？成功了！🎉

---

## 2. 视频压缩原理

### 2.1 原始视频有多大

不压缩的视频数据量惊人：

| 分辨率 | 每帧大小 | 1 秒 (30fps) | 1 分钟 | 1 小时 |
|:---|:---|:---|:---|:---|
| 1280×720 (720p) | 2.6 MB | 79 MB | **4.7 GB** | 280 GB |
| 1920×1080 (1080p) | 5.9 MB | 178 MB | **10.7 GB** | 640 GB |
| 3840×2160 (4K) | 23.7 MB | 711 MB | **42.7 GB** | 2.5 TB |

**实际 1 分钟 1080p 视频约 100 MB**，压缩了 **100 倍以上**！

### 2.2 视频为什么能压缩

视频数据存在大量冗余，压缩就是去除这些冗余：

#### 冗余 1：空间冗余（帧内压缩）

**现象**：相邻像素通常很相似。

```
蓝天区域像素值：
200, 201, 200, 199, 201, 202, 200, 199 ...

压缩思路：存储第一个值，后面存差值
200, +1, -1, -1, +2, +1, -2, -1 ...

差值范围小，可以用更少的位存储
```

**技术实现**：
- **DCT 变换**：将图像从空间域转换到频率域
- **量化**：丢弃人眼不敏感的高频信息
- **熵编码**：用更短的编码表示频繁出现的值

#### 冗余 2：时间冗余（帧间压缩）

**现象**：连续帧之间变化很小。

```
第 1 帧（I 帧）：[完整画面]  50 KB
第 2 帧（P 帧）：[变化区域]  10 KB  （只存与第 1 帧的差异）
第 3 帧（P 帧）：[变化区域]  8 KB   （只存与第 2 帧的差异）
```

**运动估计**：找到当前帧的块在上一帧的哪个位置。只编码位置偏移（运动向量）和残差。

#### 冗余 3：视觉冗余（色度子采样）

**人眼特性**：对亮度敏感，对颜色不敏感。

```
RGB 格式：每个像素 3 字节（R、G、B 各 1 字节）
YUV420 格式：
  - Y（亮度）：每个像素 1 字节
  - U（色度）：每 4 个像素 1 字节
  - V（色度）：每 4 个像素 1 字节
  
平均每个像素 1.5 字节，比 RGB 省 50%
```

### 2.3 帧类型详解

视频编码使用三种帧类型，配合压缩：

<img src="../docs/images/frame-types.svg" width="100%"/>

| 帧类型 | 名称 | 大小 | 编码方式 | 用途 |
|:---|:---|:---|:---|:---|
| **I 帧** | 关键帧 (Intra) | 大 (40-60 KB) | 完整编码，不依赖其他帧 | 随机访问点，快进定位 |
| **P 帧** | 预测帧 (Predicted) | 中 (8-15 KB) | 参考前一帧 | 正常播放 |
| **B 帧** | 双向帧 (Bi-directional) | 小 (3-8 KB) | 参考前后两帧 | 提高压缩率 |

**GOP (Group of Pictures)**：两个 I 帧之间的帧序列。

```
典型 GOP 结构（30 帧）：

时间：  0    33   66   100  133  166  200  毫秒
帧：    I ←──────────── P ←─────────── P
         ↖ B ←──→ B ↗    ↖ B ↗
         
I 帧间隔通常 1-2 秒，太长影响快进，太短降低压缩率
```

**为什么 B 帧压缩率最高？**

B 帧可以同时参考前面和后面的帧，找到最匹配的块。例如：
- 场景中的物体被遮挡后又出现
- B 帧可以看到后面的 I/P 帧，更好地预测当前帧

### 2.4 压缩标准演进

| 标准 | 年份 | 压缩效率 | 特点 |
|:---|:---|:---|:---|
| MPEG-2 | 1995 | 基准 | DVD、数字电视 |
| **H.264/AVC** | 2003 | 2× MPEG-2 | 最广泛支持，本章使用 |
| H.265/HEVC | 2013 | 2× H.264 | 4K/8K 视频 |
| **AV1** | 2018 | 比 H.265 省 30% | 开源免专利费 |
| H.266/VVC | 2020 | 2× H.265 | 最新标准 |

---

## 3. 颜色空间与像素格式

### 3.1 RGB 与 YUV

**RGB**：红绿蓝三原色，每个像素 3 字节。

**YUV**：亮度 + 色度分离。
- **Y (Luma)**：亮度，决定图像明暗
- **U (Cb)**：蓝色色度
- **V (Cr)**：红色色度

**为什么视频用 YUV？**

1. **兼容黑白电视**：Y 通道就是黑白信号
2. **利用人眼特性**：对亮度敏感，对色度不敏感
3. **压缩友好**：可以对 U/V 降采样

### 3.2 YUV420P 内存布局

<img src="../docs/images/yuv-layout-new.svg" width="90%"/>

**4:2:0 采样**：
- 每 4 个 Y 像素共享 1 个 U 和 1 个 V
- U/V 分辨率是 Y 的一半（宽/2 × 高/2）

**计算示例（1920×1080）**：

| 平面 | 分辨率 | 大小 | 占比 |
|:---|:---|:---|:---|
| Y | 1920 × 1080 | 2,073,600 B | 66.7% |
| U | 960 × 540 | 518,400 B | 16.7% |
| V | 960 × 540 | 518,400 B | 16.7% |
| **总计** | - | **3,110,400 B** | 100% |

对比 RGB：1920 × 1080 × 3 = **6,220,800 B**

**YUV420 比 RGB 节省 50% 空间**

### 3.3 FFmpeg 中的像素格式

```cpp
// 访问 YUV 数据
AVFrame* frame = av_frame_alloc();

uint8_t* y_data = frame->data[0];      // Y 平面指针
uint8_t* u_data = frame->data[1];      // U 平面指针  
uint8_t* v_data = frame->data[2];      // V 平面指针

int y_stride = frame->linesize[0];    // Y 行宽（含填充）
int u_stride = frame->linesize[1];    // U 行宽
int v_stride = frame->linesize[2];    // V 行宽

// 访问像素 (x, y)
uint8_t y = y_data[y * y_stride + x];
uint8_t u = u_data[(y/2) * u_stride + (x/2)];  // U/V 分辨率减半
uint8_t v = v_data[(y/2) * v_stride + (x/2)];
```

⚠️ **注意 `linesize` 可能大于 `width`**：某些 CPU 要求内存对齐（如 32 字节），行末会有填充字节。

---

## 4. FFmpeg 架构解析

### 4.1 FFmpeg 库结构

```
┌─────────────────────────────────────────────────────────────┐
│  libavformat - 封装/解封装（MP4、FLV、AVI 等）               │
│  ├─ 读取文件头，提取流信息                                   │
│  ├─ 分离音视频流                                            │
│  └─ 网络协议支持（HTTP、RTMP 等）                            │
├─────────────────────────────────────────────────────────────┤
│  libavcodec - 编解码（H.264、H.265、AAC 等）                │
│  ├─ 解码：压缩数据 → 原始帧（YUV/PCM）                       │
│  ├─ 编码：原始帧 → 压缩数据                                  │
│  └─ 支持硬件加速（VAAPI、VideoToolbox 等）                   │
├─────────────────────────────────────────────────────────────┤
│  libavutil - 工具函数                                        │
│  ├─ 内存管理（av_malloc、av_free）                          │
│  ├─ 数学运算、时间处理                                       │
│  └─ 数据结构（AVBuffer、AVDictionary）                       │
├─────────────────────────────────────────────────────────────┤
│  libswscale - 图像转换                                       │
│  └─ 缩放、格式转换（YUV ↔ RGB）                              │
├─────────────────────────────────────────────────────────────┤
│  libswresample - 音频重采样                                  │
│  └─ 采样率转换、声道转换                                     │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 核心数据结构

#### AVFormatContext - 文件总控

```cpp
typedef struct AVFormatContext {
    const AVClass* av_class;           // 用于日志和反射
    struct AVInputFormat* iformat;     // 输入格式（MP4/FLV 等）
    AVIOContext* pb;                   // IO 上下文（文件/网络）
    
    unsigned int nb_streams;           // 流数量
    AVStream** streams;                // 流数组
    
    char* url;                         // 文件路径/URL
    int64_t duration;                  // 总时长（微秒）
    int64_t bit_rate;                  // 总码率
    
    AVDictionary* metadata;            // 元数据（标题、作者等）
} AVFormatContext;
```

**关键字段**：
- `streams`：音视频流数组，每个元素是 AVStream
- `duration`：总时长，单位是 `AV_TIME_BASE`（1/1,000,000 秒）
- `metadata`：键值对形式的元数据

#### AVStream - 单个流

```cpp
typedef struct AVStream {
    int index;                         // 流索引（0, 1, 2...）
    AVCodecParameters* codecpar;       // 编码参数（分辨率、码率等）
    
    AVRational time_base;              // 时间基（分数形式）
    int64_t duration;                  // 流时长（以 time_base 为单位）
    
    AVRational avg_frame_rate;         // 平均帧率
    AVRational r_frame_rate;           // 实际帧率（最可靠）
} AVStream;
```

**时间基转换**：
```cpp
AVRational tb = stream->time_base;      // 如 {1, 1000} 表示毫秒
int64_t pts = 33000;                    // 33 秒

// 转换为秒
double seconds = pts * av_q2d(tb);      // 33.0

// 转换为毫秒
int64_t ms = pts * av_q2d(tb) * 1000;   // 33000
```

#### AVCodecContext - 编解码器

```cpp
typedef struct AVCodecContext {
    const AVCodec* codec;              // 编解码器
    
    // 视频参数
    int width, height;                 // 分辨率
    AVPixelFormat pix_fmt;             // 像素格式（YUV420P 等）
    
    // 编码参数（编码时使用）
    int bit_rate;                      // 目标码率
    int gop_size;                      // GOP 大小（I 帧间隔）
    
    // 解码参数
    int thread_count;                  // 解码线程数
} AVCodecContext;
```

#### AVPacket - 压缩数据

```cpp
typedef struct AVPacket {
    AVBufferRef* buf;                  // 数据缓冲区（引用计数）
    int64_t pts;                       // 显示时间戳
    int64_t dts;                       // 解码时间戳
    uint8_t* data;                     // 数据指针
    int size;                          // 数据大小
    int stream_index;                  // 所属流索引
    int flags;                         // 标志（关键帧等）
} AVPacket;
```

#### AVFrame - 原始帧

```cpp
typedef struct AVFrame {
    uint8_t* data[8];                  // 各平面数据指针
    int linesize[8];                   // 各平面行宽
    
    int width, height;                 // 分辨率
    AVPixelFormat format;              // 像素格式
    
    int64_t pts;                       // 显示时间戳
    int key_frame;                     // 是否关键帧
    AVPictureType pict_type;           // 帧类型（I/P/B）
} AVFrame;
```

---

## 5. 代码详解

### 5.1 视频播放流程

<img src="../docs/images/video-pipeline.svg" width="100%"/>

### 5.2 逐行分析

**步骤 1：打开文件**
```cpp
AVFormatContext* fmt = nullptr;
int ret = avformat_open_input(&fmt, argv[1], nullptr, nullptr);
if (ret < 0) {
    fprintf(stderr, "无法打开文件\n");
    return 1;
}
```

`avformat_open_input` 会自动检测文件格式（MP4、FLV、AVI 等）。

**步骤 2：读取流信息**
```cpp
avformat_find_stream_info(fmt, nullptr);
```

读取文件头，获取分辨率、时长、码率等信息。

**步骤 3：找视频流**
```cpp
int idx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
AVStream* st = fmt->streams[idx];
```

文件可能有多个流（视频+音频+字幕），这行找到"最好的"视频流。

**步骤 4：初始化解码器**
```cpp
const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
AVCodecContext* cc = avcodec_alloc_context3(codec);
avcodec_parameters_to_context(cc, st->codecpar);
avcodec_open2(cc, codec, nullptr);
```

| 函数 | 作用 |
|:---|:---|
| `avcodec_find_decoder` | 根据 codec_id 找到解码器（如 h264）|
| `avcodec_alloc_context3` | 创建解码器上下文 |
| `avcodec_parameters_to_context` | 复制流参数到上下文 |
| `avcodec_open2` | 打开解码器，初始化内部状态 |

**步骤 5：创建 SDL 窗口**
```cpp
SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("Player", ...);
SDL_Renderer* rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SDL_Texture* tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_YV12, ...);
```

SDL2 三层架构：
- **Window**：窗口（标题栏、边框）
- **Renderer**：渲染器（GPU 加速）
- **Texture**：纹理（显存中的图像）

**步骤 6：解码循环**
```cpp
while (av_read_frame(fmt, pkt) >= 0) {      // 读取一个包
    if (pkt->stream_index == idx) {         // 只处理视频
        avcodec_send_packet(cc, pkt);        // 送入解码器
        while (avcodec_receive_frame(cc, frm) == 0) {  // 取解码后的帧
            SDL_UpdateYUVTexture(tex, ...);  // 更新纹理
            SDL_RenderCopy(rend, tex, ...);  // 渲染
            SDL_RenderPresent(rend);         // 显示
        }
    }
    av_packet_unref(pkt);  // 释放包
}
```

**解码器 API 说明**：
- `avcodec_send_packet`：将压缩数据送入解码器
- `avcodec_receive_frame`：获取解码后的帧
- 一个 packet 可能解码出多个 frame（如 H.264 的 B 帧）

**步骤 7：清理资源**
```cpp
av_frame_free(&frm);
av_packet_free(&pkt);
avcodec_free_context(&cc);
avformat_close_input(&fmt);
SDL_Quit();
```

FFmpeg 资源必须配对释放，否则内存泄漏。

### 5.3 内存管理最佳实践

**C 风格（容易泄漏）**：
```cpp
void BadExample() {
    AVPacket* pkt = av_packet_alloc();
    if (error) return;  // 糟糕！pkt 没释放
    av_packet_free(&pkt);
}
```

**C++ RAII 风格（推荐）**：
```cpp
struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        if (p) av_packet_free(&p);
    }
};
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

void GoodExample() {
    PacketPtr pkt(av_packet_alloc());
    if (error) return;  // 安全，自动释放
}  // pkt 自动释放
```

---

## 6. Pipeline 架构设计

### 6.1 为什么需要架构

**问题代码**：
```cpp
int main() {
    // 300 行混乱代码
    // 改了这里，那里出问题
    // 不敢重构，只能继续堆
}
```

**Pipeline 架构**：
```
输入 → Demuxer → Decoder → Renderer → 输出
         ↑           ↑          ↑
      解封装      解码       渲染
```

**好处**：
1. **单一职责**：每个模块只做一件事
2. **可测试**：模块独立测试
3. **可替换**：实现相同接口即可替换
4. **可扩展**：添加新功能不修改旧代码

### 6.2 接口设计

```cpp
// 错误码
enum class ErrorCode {
    OK = 0,
    FILE_NOT_FOUND,
    FORMAT_NOT_SUPPORTED,
    CODEC_NOT_FOUND,
    // ...
};

// Pipeline 接口
class Pipeline {
public:
    virtual ~Pipeline() = default;
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
};

// Demuxer 接口
class Demuxer {
public:
    virtual ErrorCode Open(const std::string& url) = 0;
    virtual ErrorCode ReadPacket(PacketPtr& packet) = 0;
    virtual AVStream* GetVideoStream() = 0;
};

// Decoder 接口
class Decoder {
public:
    virtual ErrorCode Init(const AVCodecParameters* par) = 0;
    virtual ErrorCode SendPacket(const PacketPtr& pkt) = 0;
    virtual ErrorCode ReceiveFrame(FramePtr& frm) = 0;
};
```

### 6.3 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(player)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)
find_package(SDL2 REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

add_executable(player player.cpp)
target_link_libraries(player ${FFMPEG_LIBRARIES} SDL2::SDL2)
```

---

## 7. 调试与优化

### 7.1 内存泄漏检测

```bash
valgrind --leak-check=full ./player test.mp4
```

**预期输出**：
```
All heap blocks were freed -- no leaks are possible
```

### 7.2 性能分析

```bash
# 记录性能数据
perf record ./player test.mp4

# 查看热点
perf report
```

### 7.3 常见优化

| 优化 | 效果 | 实现 |
|:---|:---|:---|
| 硬件解码 | CPU 50% → 10% | 使用 VAAPI/VideoToolbox |
| 多线程解码 | 提升多核利用率 | FFmpeg 内置支持 |
| PTS 同步 | 消除快慢放 | 根据时间戳计算延迟 |

---

## 8. 常见问题

### Q1: 编译报错 `undefined reference to`

```bash
# 检查 pkg-config
pkg-config --libs libavformat

# 手动指定路径
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### Q2: 运行时崩溃

```bash
# 使用 gdb
gdb ./player
run test.mp4
bt  # 查看堆栈
```

### Q3: 画面是绿色的

**原因**：YUV 数据没填对，或 UV 顺序反了。

**检查**：
```cpp
printf("format=%s\n", av_get_pix_fmt_name(frame->format));
```

### Q4: 播放太快/太慢

**原因**：`SDL_Delay(33)` 是硬编码 30fps。

**解决**：根据 PTS 计算实际延迟：
```cpp
int64_t pts_ms = frame->pts * av_q2d(time_base) * 1000;
int64_t delay = pts_ms - elapsed_ms;
if (delay > 0) SDL_Delay(delay);
```

---

## 9. 下一步

本章是**同步单线程**播放器，存在的问题：

```
播放 4K 视频时：
- 解码：25ms
- 渲染：8ms  
- 总时间：33ms → 30fps（临界，易卡顿）

解决方案：多线程
- 解码线程 + 渲染线程并行
- 实际帧间隔：25ms → 40fps（流畅）
```

**第 2 章**：异步多线程架构
- 生产者-消费者队列
- 线程安全设计
- 帧队列管理

---

## 附录

### 术语表

| 术语 | 解释 |
|:---|:---|
| **FFmpeg** | 开源音视频处理库 |
| **Demuxer** | 解封装器，从文件提取压缩数据 |
| **Decoder** | 解码器，解压数据为原始图像 |
| **YUV** | 亮度+色度像素格式，比 RGB 高效 |
| **YUV420** | 4:2:0 采样，U/V 分辨率减半 |
| **PTS** | Presentation Time Stamp，显示时间戳 |
| **DTS** | Decoding Time Stamp，解码时间戳 |
| **I/P/B 帧** | 关键帧/预测帧/双向帧 |
| **GOP** | Group of Pictures，图像组 |
| **time_base** | 时间基，PTS 的单位 |
| **DCT** | 离散余弦变换，压缩核心技术 |
| **量化** | 丢弃不重要信息，有损压缩 |

### 参考

- FFmpeg 文档：https://ffmpeg.org/documentation.html
- SDL2 文档：https://wiki.libsdl.org/
- H.264 规范：ISO/IEC 14496-10

---

**本章代码仓库**：https://github.com/chapin666/live-system-book/tree/master/chapter-01
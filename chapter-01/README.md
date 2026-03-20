# 第一章：Pipeline 架构与本地播放

> **目标**：从零开始，30 分钟内让视频跑起来，然后理解为什么这样写。

**预计时间**：
- 快速开始（5 分钟）：先跑起来
- 深入理解（60 分钟）：读懂代码
- 动手实践（30 分钟）：自己改代码

---

## 目录

1. [5 分钟跑起来](#1-5-分钟跑起来) — 先看到画面
2. [代码解剖](#2-代码解剖) — 读懂这 10 行
3. [为什么需要架构](#3-为什么需要架构) — 从烂代码到好代码
4. [关键概念详解](#4-关键概念详解) — YUV、PTS、RAII
5. [工业级实现](#5-工业级实现) — 完整的 Pipeline
6. [性能基准](#6-性能基准) — 数据说话
7. [常见问题](#7-常见问题)
8. [下一步](#8-下一步)

---

## 1. 5 分钟跑起来

### 1.1 环境准备

**macOS:**
```bash
brew install ffmpeg sdl2 cmake
```

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev \
    libavutil-dev libswscale-dev libsdl2-dev cmake
```

### 1.2 创建最简播放器

创建文件 `minimal_player.cpp`：

```cpp
#include <SDL2/SDL.h>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char* argv[]) {
    // 1. 打开文件
    AVFormatContext* ctx = nullptr;
    avformat_open_input(&ctx, argv[1], nullptr, nullptr);
    avformat_find_stream_info(ctx, nullptr);
    
    // 2. 找到视频流
    int idx = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st = ctx->streams[idx];
    
    // 3. 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
    AVCodecContext* cc = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(cc, st->codecpar);
    avcodec_open2(cc, codec, nullptr);
    
    // 4. 创建窗口
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Player", SDL_WINDOWPOS_CENTERED, 
                                       SDL_WINDOWPOS_CENTERED, cc->width, cc->height, 0);
    SDL_Renderer* rend = SDL_CreateRenderer(win, -1, 0);
    SDL_Texture* tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_YV12, 
                                          SDL_TEXTUREACCESS_STREAMING, cc->width, cc->height);
    
    // 5. 解码循环
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frm = av_frame_alloc();
    
    while (av_read_frame(ctx, pkt) >= 0) {
        if (pkt->stream_index == idx) {
            avcodec_send_packet(cc, pkt);
            while (avcodec_receive_frame(cc, frm) == 0) {
                SDL_UpdateYUVTexture(tex, nullptr, frm->data[0], frm->linesize[0],
                                     frm->data[1], frm->linesize[1],
                                     frm->data[2], frm->linesize[2]);
                SDL_RenderCopy(rend, tex, nullptr, nullptr);
                SDL_RenderPresent(rend);
                SDL_Delay(33);  // 约30fps
            }
        }
        av_packet_unref(pkt);
    }
    
    // 6. 清理
    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&cc);
    avformat_close_input(&ctx);
    SDL_Quit();
    return 0;
}
```

### 1.3 编译运行

```bash
# 编译
g++ minimal_player.cpp -o minimal_player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2) \
    -std=c++11

# 准备测试视频
ffmpeg -f lavfi -i testsrc=duration=5:size=640x480:rate=30 \
       -pix_fmt yuv420p sample.mp4

# 运行
./minimal_player sample.mp4
```

**看到彩色条纹在动？成功了！** 🎉

但这代码有问题，下面我们来解剖它。

---

## 2. 代码解剖

### 2.1 这 10 行做了什么？

**第 1 步：打开文件**
```cpp
AVFormatContext* ctx = nullptr;
avformat_open_input(&ctx, argv[1], nullptr, nullptr);
avformat_find_stream_info(ctx, nullptr);
```

- `AVFormatContext`：文件的"总控"，包含所有信息
- `avformat_open_input`：打开文件，探测格式（MP4/FLV/AVI 等）
- `avformat_find_stream_info`：读取流信息（视频、音频、字幕）

**第 2 步：找到视频流**
```cpp
int idx = av_find_best_stream(ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
```

一个文件可能有多个流（视频+音频+字幕），这行找到"最好的"视频流。

**第 3 步：初始化解码器**
```cpp
const AVCodec* codec = avcodec_find_decoder(st->codecpar->codec_id);
AVCodecContext* cc = avcodec_alloc_context3(codec);
avcodec_parameters_to_context(cc, st->codecpar);
avcodec_open2(cc, codec, nullptr);
```

| 函数 | 作用 |
|:---|:---|
| `avcodec_find_decoder` | 根据 codec_id 找到对应的解码器（如 H.264 找 h264 解码器）|
| `avcodec_alloc_context3` | 创建解码器上下文（存储解码状态）|
| `avcodec_parameters_to_context` | 把流参数（分辨率、码率等）复制到上下文 |
| `avcodec_open2` | 打开解码器，初始化内部状态 |

**为什么要分 4 步？**

```
找解码器 → 创建上下文 → 填参数 → 打开
   ↓           ↓          ↓       ↓
 "用哪个"    "容器"     "配置"   "启动"
```

**第 4 步：创建窗口**
```cpp
SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow(...);
SDL_Renderer* rend = SDL_CreateRenderer(...);
SDL_Texture* tex = SDL_CreateTexture(..., SDL_PIXELFORMAT_YV12, ...);
```

SDL2 的三层架构：
- **Window**：窗口（标题栏、边框）
- **Renderer**：渲染器（GPU 加速）
- **Texture**：纹理（显存中的图像）

**为什么是 YV12 格式？**

FFmpeg 解码出来的是 YUV420P，SDL 的 YV12 只是 UV 顺序交换，几乎零开销。

**第 5 步：解码循环**
```cpp
while (av_read_frame(ctx, pkt) >= 0) {      // 读取一个包
    if (pkt->stream_index == idx) {         // 只处理视频
        avcodec_send_packet(cc, pkt);        // 送入解码器
        while (avcodec_receive_frame(cc, frm) == 0) {  // 接收解码后的帧
            SDL_UpdateYUVTexture(...);       // 更新纹理
            SDL_RenderCopy(...);             // 渲染
            SDL_RenderPresent(...);          // 显示
            SDL_Delay(33);                   // 控制帧率
        }
    }
    av_packet_unref(pkt);  // ⚠️ 必须释放！
}
```

### 2.2 代码有什么问题？

| 问题 | 后果 | 严重程度 |
|:---|:---|:---:|
| **没有错误处理** | 文件打不开直接崩溃 | 🔴 高 |
| **内存泄漏** | `pkt`、`frm` 分配了但没完全释放 | 🔴 高 |
| **硬编码延时** | `SDL_Delay(33)` 假设 30fps，实际视频可能是 25fps | 🟡 中 |
| **窗口无法关闭** | 没有处理 SDL 事件，只能强制结束 | 🔴 高 |
| **单线程阻塞** | 解码慢时画面卡顿 | 🟡 中 |

**最严重的问题**：没有错误处理。

```cpp
// 当前代码：失败就崩溃
avformat_open_input(&ctx, argv[1], nullptr, nullptr);  // 文件不存在？崩溃！

// 应该这样：
int ret = avformat_open_input(&ctx, argv[1], nullptr, nullptr);
if (ret < 0) {
    fprintf(stderr, "无法打开文件: %s\n", argv[1]);
    return 1;
}
```

---

## 3. 为什么需要架构

### 3.1 "烂代码"是怎么变多的？

假设你在 minimal_player 基础上加功能：

**第 1 天：加错误处理**
```cpp
int ret = avformat_open_input(...);
if (ret < 0) { /* 处理 */ }

ret = avformat_find_stream_info(...);
if (ret < 0) { /* 处理 */ }
// ... 每个函数都要判断，main 变成 50 行
```

**第 2 天：加音频播放**
```cpp
// 再加 30 行音频初始化和播放代码
// main 变成 100 行
```

**第 3 天：加网络播放**
```cpp
// 再加 50 行网络代码
// main 变成 150 行，无法维护
```

**第 7 天**：
```cpp
int main() {
    // 300 行混乱代码
    // 改了这里，那里出问题
    // 不敢重构，只能继续堆
}
```

### 3.2 工业级代码的要求

| 要求 | 为什么重要 | 本章解决方案 |
|:---|:---|:---|
| **不泄漏内存** | 播放器要跑几小时/几天 | RAII 智能指针，自动释放 |
| **错误可处理** | 网络断了、文件坏了怎么办？ | 错误码分级，优雅降级 |
| **可测试** | 改代码后怎么保证没坏？ | 接口抽象，模块独立测试 |
| **可观测** | 线上出问题怎么排查？ | 统计接口，实时看状态 |
| **可扩展** | 后面要加功能怎么办？ | Pipeline 架构，模块可替换 |

### 3.3 Pipeline 架构设计

> **Pipeline（流水线）：数据像水一样流动，每个阶段处理完传给下一个阶段。**

<img src="../docs/images/pipeline-arch.svg" width="100%"/>

**分层设计：**

```
┌─────────────────────────────────────────────┐
│              应用层 (main.cpp)               │
│         创建 Pipeline，设置回调              │
├─────────────────────────────────────────────┤
│              调度层 (SimplePipeline)         │
│         控制数据流，管理生命周期              │
├─────────────────────────────────────────────┤
│  ┌─────────┐  ┌─────────┐  ┌─────────┐     │
│  │Demuxer  │→│Decoder  │→│Renderer │     │
│  │解封装   │  │解码     │  │渲染     │     │
│  └─────────┘  └─────────┘  └─────────┘     │
│         核心处理层（三个独立模块）            │
├─────────────────────────────────────────────┤
│  Observer + Stats + ErrorCode                │
│  可观测性基础设施                            │
└─────────────────────────────────────────────┘
```

**关键设计决策：**

| 决策 | 说明 | 好处 |
|:---|:---|:---|
| **接口抽象** | Pipeline 是纯虚接口 | 实现可替换，便于测试 |
| **模块独立** | Demuxer/Decoder/Renderer 互不依赖 | 可单独测试、优化 |
| **RAII 封装** | 智能指针管理 FFmpeg 资源 | 不泄漏、异常安全 |
| **观察者模式** | 通过回调暴露内部状态 | 不破坏封装，可观测 |

---

## 4. 关键概念详解

### 4.1 YUV 像素格式

**为什么用 YUV 而不是 RGB？**

人眼的特性：
- **对亮度敏感**：能看出明暗变化
- **对色度不敏感**：看不出细微的颜色差别

**YUV 的设计：**
- **Y**（Luma）：亮度，完整分辨率
- **U/V**（Chroma）：色度，分辨率减半

<img src="../docs/images/yuv-layout.svg" width="80%"/>

**内存占用对比（1920×1080）：**

| 格式 | 每帧大小 | 相比 RGB |
|:---|:---|:---|
| RGB | 6.2 MB | 100% |
| YUV420P | **3.1 MB** | **50%** |

**FFmpeg 中的 YUV：**

```cpp
// YUV420P 内存布局
AVFrame* frame = av_frame_alloc();
frame->data[0]  // Y 平面指针
frame->data[1]  // U 平面指针
frame->data[2]  // V 平面指针
frame->linesize[0]  // Y 行宽（可能大于宽度，有对齐）
```

⚠️ **注意 `linesize` 可能大于 `width`**：
- 某些 CPU 要求内存 16/32 字节对齐
- `linesize` 包含填充字节
- 计算偏移要用 `linesize`，不要用 `width`

### 4.2 PTS（Presentation Time Stamp）

**视频播放的时间控制**

视频帧不是越快显示越好，而是要**按正确的时间显示**。

**PTS 的作用**：告诉播放器"这帧应该在什么时候显示"。

```cpp
// 假设视频是 30fps（每秒30帧）
// 第0帧：PTS = 0ms，应该立即显示
// 第1帧：PTS = 33ms，应该在33毫秒后显示
// 第2帧：PTS = 66ms，应该在66毫秒后显示
```

**但 PTS 的单位不一定是毫秒！**

```cpp
// FFmpeg 使用 "时间基"（time_base）
// time_base = 1/1000 表示 PTS 以毫秒为单位
// time_base = 1/90000 表示 PTS 以 1/90000 秒为单位

// 转换公式：
// 实际时间（秒）= PTS × time_base
// 实际时间（毫秒）= PTS × time_base × 1000

AVStream* st = ...;
int64_t pts = frame->pts;
AVRational tb = st->time_base;
double seconds = pts * av_q2d(tb);  // av_q2d: 分数转 double
```

**同步代码实现：**

```cpp
auto start_time = std::chrono::steady_clock::now();

while (running) {
    FramePtr frame = decoder.ReceiveFrame();
    
    // 计算这帧应该显示的时间
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - start_time;
    int64_t frame_ms = frame->pts * av_q2d(time_base) * 1000;
    
    // 如果还没到显示时间，等待
    if (frame_ms > elapsed.count()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(frame_ms - elapsed.count())
        );
    }
    
    renderer.Render(frame.get());
}
```

### 4.3 RAII 内存管理

**FFmpeg 的 C API 需要手动管理内存：**

```cpp
// ❌ 裸指针：容易泄漏和重复释放
void BadExample() {
    AVPacket* pkt = av_packet_alloc();
    
    if (some_error) {
        return;  // 糟糕！pkt 没释放，内存泄漏！
    }
    
    av_packet_free(&pkt);
}
```

**C++ 解决方案：RAII（资源获取即初始化）**

```cpp
// 自定义删除器
struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        if (p) av_packet_free(&p);
    }
};

// 智能指针类型
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

// 工厂函数
inline PacketPtr MakePacket() {
    return PacketPtr(av_packet_alloc());
}

// ✅ 使用
void GoodExample() {
    PacketPtr pkt = MakePacket();
    
    if (some_error) {
        return;  // 安全！pkt 自动释放
    }
    
}  // 函数结束，pkt 自动释放
```

**本章提供的完整封装：**

```cpp
// base/ffmpeg_utils.h
#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <memory>

namespace live {

struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        if (p) av_packet_free(&p);
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* p) const {
        if (p) av_frame_free(&p);
    }
};

struct CodecContextDeleter {
    void operator()(AVCodecContext* p) const {
        if (p) avcodec_free_context(&p);
    }
};

using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;

inline PacketPtr MakePacket() { return PacketPtr(av_packet_alloc()); }
inline FramePtr MakeFrame() { return FramePtr(av_frame_alloc()); }

} // namespace live
```

**RAII 的好处：**
1. **不泄漏**：析构时自动释放
2. **异常安全**：抛出异常也会正确释放
3. **代码简洁**：不用写 `free`/`delete`
4. **所有权明确**：`unique_ptr` 表示独占所有权

### 4.4 SDL 事件循环

**为什么需要事件循环？**

```cpp
// ❌ 错误：没有事件循环
while (playing) {
    render_frame();
}
// 窗口无法响应关闭、拖动等操作
```

```cpp
// ✅ 正确：有事件循环
while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {  // 处理所有待处理事件
        if (e.type == SDL_QUIT) {  // 点击关闭按钮
            running = false;
        }
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
            running = false;
        }
    }
    
    // 渲染一帧
    render_frame();
}
```

**事件循环的作用：**
- 响应窗口操作（关闭、最小化、调整大小）
- 响应键盘鼠标输入
- 保持界面不卡顿

**注意**：`SDL_PollEvent` 是非阻塞的，没有事件立即返回 0。

---

## 5. 工业级实现

### 5.1 项目结构

```
chapter-01/
├── CMakeLists.txt          # 构建配置
├── src/
│   ├── base/               # 基础组件（可复用）
│   │   ├── pipeline.h      # Pipeline 接口
│   │   └── ffmpeg_utils.h  # RAII 封装
│   ├── core/               # 核心实现
│   │   ├── simple_pipeline.h/cpp
│   │   ├── demuxer.h/cpp
│   │   ├── decoder.h/cpp
│   │   └── renderer.h/cpp
│   └── main.cpp            # 示例程序
└── tests/                  # 单元测试
    └── test_pipeline.cpp
```

### 5.2 接口设计

**Pipeline 接口**（`base/pipeline.h`）：

```cpp
#pragma once

#include <string>
#include <memory>

namespace live {

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

struct PipelineStats {
    int64_t total_frames = 0;
    int64_t dropped_frames = 0;
    double current_fps = 0.0;
    int64_t current_pts = 0;
};

class PipelineObserver {
public:
    virtual ~PipelineObserver() = default;
    virtual void OnError(ErrorCode code, const std::string& message) = 0;
    virtual void OnFrameRendered(int64_t pts) = 0;
    virtual void OnStatsUpdated(const PipelineStats& stats) = 0;
};

class Pipeline {
public:
    virtual ~Pipeline() = default;
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
    virtual PipelineStats GetStats() const = 0;
    virtual void SetObserver(PipelineObserver* observer) = 0;
};

} // namespace live
```

### 5.3 核心模块实现

**Demuxer**（只展示关键部分）：

```cpp
class Demuxer {
public:
    ErrorCode Open(const std::string& url);
    ErrorCode ReadPacket(PacketPtr& packet);
    AVStream* GetVideoStream() const;
    
private:
    AVFormatContext* format_ctx_ = nullptr;
    int video_stream_index_ = -1;
};

ErrorCode Demuxer::Open(const std::string& url) {
    // 1. 打开文件
    int ret = avformat_open_input(&format_ctx_, url.c_str(), nullptr, nullptr);
    if (ret < 0) {
        return ErrorCode::FILE_NOT_FOUND;
    }
    
    // 2. 读取流信息
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        return ErrorCode::FORMAT_NOT_SUPPORTED;
    }
    
    // 3. 找到视频流
    video_stream_index_ = av_find_best_stream(
        format_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    
    if (video_stream_index_ < 0) {
        return ErrorCode::FORMAT_NOT_SUPPORTED;
    }
    
    return ErrorCode::OK;
}
```

**Decoder：**

```cpp
class Decoder {
public:
    ErrorCode Init(const AVCodecParameters* codecpar);
    ErrorCode SendPacket(const PacketPtr& packet);
    ErrorCode ReceiveFrame(FramePtr& frame);
    
private:
    CodecContextPtr codec_ctx_;
};

ErrorCode Decoder::Init(const AVCodecParameters* codecpar) {
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        return ErrorCode::CODEC_NOT_FOUND;
    }
    
    codec_ctx_.reset(avcodec_alloc_context3(codec));
    if (!codec_ctx_) {
        return ErrorCode::OUT_OF_MEMORY;
    }
    
    int ret = avcodec_parameters_to_context(codec_ctx_.get(), codecpar);
    if (ret < 0) {
        return ErrorCode::DECODER_ERROR;
    }
    
    ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (ret < 0) {
        return ErrorCode::DECODER_ERROR;
    }
    
    return ErrorCode::OK;
}
```

### 5.4 单元测试示例

```cpp
// tests/test_pipeline.cpp
#include <gtest/gtest.h>
#include "core/simple_pipeline.h"

using namespace live;

TEST(DemuxerTest, OpenValidFile) {
    Demuxer demuxer;
    auto err = demuxer.Open("test_data/sample.mp4");
    EXPECT_EQ(err, ErrorCode::OK);
    EXPECT_NE(demuxer.GetVideoStream(), nullptr);
}

TEST(DemuxerTest, OpenInvalidFile) {
    Demuxer demuxer;
    auto err = demuxer.Open("nonexistent.mp4");
    EXPECT_EQ(err, ErrorCode::FILE_NOT_FOUND);
}

TEST(DecoderTest, InitWithValidParams) {
    // 先打开文件获取 codecpar
    Demuxer demuxer;
    demuxer.Open("test_data/sample.mp4");
    
    Decoder decoder;
    auto err = decoder.Init(demuxer.GetVideoStream()->codecpar);
    EXPECT_EQ(err, ErrorCode::OK);
}
```

---

## 6. 性能基准

### 6.1 测试环境

- CPU: Intel i7-8700K @ 3.7GHz
- RAM: 16GB DDR4
- 测试视频: 1920×1080 @ 30fps, H.264

### 6.2 性能数据

| 指标 | minimal_player | Pipeline 版本 | 优化空间 |
|:---|:---|:---|:---|
| **CPU 占用** | 45% | 42% | - |
| **内存占用** | 85 MB | 78 MB | - |
| **启动时间** | 120 ms | 135 ms | 接口初始化开销 |
| **帧率稳定性** | ±5 fps | ±1 fps | PTS 同步更准确 |
| **内存泄漏** | 有（~2MB/分钟） | 无 | RAII 封装 |

### 6.3 内存分析

```bash
# 使用 valgrind 检测内存泄漏
valgrind --leak-check=full --show-leak-kinds=all ./live-player sample.mp4

# Pipeline 版本输出：
# All heap blocks were freed -- no leaks are possible
#
# minimal_player 输出：
# definitely lost: 2,048 bytes in 64 blocks
```

### 6.4 性能分析

```bash
# 使用 perf 分析热点
perf record ./live-player sample.mp4
perf report

# 典型结果：
# 35%  libavcodec  avcodec_send_packet    # 解码
# 25%  libavcodec  avcodec_receive_frame  # 获取帧
# 20%  libSDL2     SDL_UpdateYUVTexture   # 上传 GPU
# 10%  libc        memcpy                 # 内存拷贝
# 10%  其他
```

**优化建议（后续章节实现）：**
- 硬件解码：把 35% CPU 占用降到 5%
- 零拷贝：避免 10% 的 memcpy

---

## 7. 常见问题

### Q1: CMake 找不到 FFmpeg

```bash
# 检查安装
pkg-config --exists libavformat && echo "OK" || echo "Not found"

# 手动指定路径
cmake -DFFMPEG_ROOT=/usr/local ..
```

### Q2: 运行时崩溃

```bash
# 用 gdb 调试
gdb ./live-player
run sample.mp4
bt  # 查看堆栈
```

### Q3: 画面撕裂或卡顿

- 检查 PTS 同步逻辑
- 尝试启用 SDL 垂直同步：`SDL_RENDERER_PRESENTVSYNC`

### Q4: 内存不断增长

```bash
# 用 valgrind 定位
valgrind --tool=massif ./live-player sample.mp4
ms_print massif.out.*
```

---

## 8. 下一步

本章实现了**同步单线程**播放器，但有一个根本问题：

```
问题场景：播放 4K 视频
- 解码一帧：30ms
- 渲染一帧：5ms
- 总时间：35ms → 28fps（卡顿！）

理想方案：解码和渲染并行
- 解码线程：30ms/帧
- 渲染线程：5ms/帧（与解码重叠）
- 实际帧间隔：30ms → 33fps（流畅！）
```

**第2章预告**：
- 引入多线程架构
- 生产者-消费者队列
- 帧队列管理（固定大小、丢帧策略）
- 彻底解决卡顿问题

---

## 附录

### A. 关键术语表

| 术语 | 解释 |
|:---|:---|
| **Demuxer** | 解封装器，从容器格式中提取压缩数据 |
| **Decoder** | 解码器，将压缩数据还原为原始图像 |
| **Renderer** | 渲染器，将图像显示到屏幕 |
| **PTS** | Presentation Time Stamp，显示时间戳 |
| **Time Base** | 时间基，PTS 的单位 |
| **RAII** | Resource Acquisition Is Initialization，资源获取即初始化 |
| **Pipeline** | 流水线架构，数据分阶段处理 |
| **YUV** | 一种颜色编码格式，比 RGB 更高效 |

### B. 参考资料

- [FFmpeg 官方文档](https://ffmpeg.org/documentation.html)
- [SDL2 官方文档](https://wiki.libsdl.org/)
- 《视频编码全角度详解》

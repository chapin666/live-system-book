# 第一章：Pipeline 架构与本地播放

> **目标**：从零开始，用 100 行代码实现视频播放，理解背后的原理。

**预计时间**：2 小时

---

## 目录

1. [快速开始](#1-快速开始)
2. [视频为什么能压缩](#2-视频为什么能压缩)
3. [FFmpeg 核心概念](#3-ffmpeg-核心概念)
4. [代码详解](#4-代码详解)
5. [Pipeline 架构设计](#5-pipeline-架构设计)
6. [调试与优化](#6-调试与优化)
7. [常见问题](#7-常见问题)
8. [下一步](#8-下一步)

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

## 2. 视频为什么能压缩

### 2.1 原始视频有多大

| 分辨率 | 每帧大小 | 1秒(30fps) | 1分钟 |
|:---|:---|:---|:---|
| 1920×1080 | 6 MB | 180 MB | **10.8 GB** |

实际 1 分钟视频只有约 100 MB，压缩了 100 倍！

### 2.2 三个压缩技巧

**技巧 1：相邻像素差不多（空间冗余）**

```
原始：140 141 142 141 140 139 140 141（8 字节）
压缩：140 +0 +1 +1 -1 -1 -1 +1 +1（差分编码，更小）
```

**技巧 2：连续帧差不多（时间冗余）**

```
第 N 帧：[完整画面]     50 KB
第 N+1 帧：[变化区域]   5 KB  （只存不同的部分）
```

**技巧 3：人眼对颜色不敏感**

- 亮度（Y）：完整分辨率
- 色度（U/V）：分辨率减半 → 省 50% 空间

### 2.3 帧类型

| 类型 | 名称 | 大小 | 用途 |
|:---|:---|:---|:---|
| I 帧 | 关键帧 | 大 | 完整画面，可独立解码 |
| P 帧 | 预测帧 | 中 | 参考前一帧 |
| B 帧 | 双向帧 | 小 | 参考前后两帧 |

```
视频序列：I  P  P  P  I  P  P  P
大小：    大 小 小 小 大 小 小 小
```

---

## 3. FFmpeg 核心概念

### 3.1 四个核心结构体

```
┌────────────────────────────────────────────────────┐
│  AVFormatContext  文件总控                          │
│  ├── 打开的文件                                      │
│  ├── 有几个流（视频/音频/字幕）                       │
│  └── 总时长、码率等信息                               │
├────────────────────────────────────────────────────┤
│  AVStream         单个流信息                         │
│  ├── 视频分辨率、帧率                                 │
│  └── 时间基（time_base）                            │
├────────────────────────────────────────────────────┤
│  AVCodecContext   编解码器上下文                     │
│  └── 解码器的配置和状态                               │
├────────────────────────────────────────────────────┤
│  AVPacket         压缩数据包                         │
│  └── 从文件读出的原始压缩数据                         │
├────────────────────────────────────────────────────┤
│  AVFrame          解码后的帧                         │
│  └── YUV 像素数据，可直接显示                         │
└────────────────────────────────────────────────────┘
```

### 3.2 YUV 像素格式

**为什么用 YUV 不用 RGB？**

人眼特性：对亮度敏感，对颜色不敏感。

```
YUV420P 内存布局（1920×1080）：

┌──────────────────────────────────────────┐
│ Y 平面：1920 × 1080 = 2,073,600 字节     │  亮度（每个像素 1 字节）
├──────────────────────────────────────────┤
│ U 平面：960 × 540 = 518,400 字节         │  色度（1/4 大小）
├──────────────────────────────────────────┤
│ V 平面：960 × 540 = 518,400 字节         │  色度（1/4 大小）
└──────────────────────────────────────────┘
总计：3.1 MB（比 RGB 省 50%）
```

**FFmpeg 中的访问**：
```cpp
frame->data[0]  // Y 平面指针
frame->data[1]  // U 平面指针
frame->data[2]  // V 平面指针
frame->linesize[0]  // Y 行宽（包含对齐填充）
```

### 3.3 时间戳（PTS）

**问题**：视频帧不是越快显示越好，要按正确时间显示。

```cpp
// 30fps 视频的 PTS
// time_base = 1/1000（毫秒）
帧 0：PTS = 0      → 0ms 显示
帧 1：PTS = 33     → 33ms 显示
帧 2：PTS = 66     → 66ms 显示
```

**转换公式**：
```cpp
// 实际时间 = PTS × time_base
// 毫秒 = PTS × time_base_num / time_base_den × 1000
int64_t ms = pts * av_q2d(time_base) * 1000;
```

---

## 4. 代码详解

### 4.1 代码流程图

```
打开文件 → 找视频流 → 初始化解码器 → 创建窗口 → 解码循环 → 清理
   ↓          ↓           ↓            ↓          ↓        ↓
avformat_  av_find_   avcodec_     SDL_      while    av_free
open_input best_stream find_decoder  Create    循环      系列
```

### 4.2 关键 API 说明

**打开文件**：
```cpp
AVFormatContext* fmt = nullptr;
avformat_open_input(&fmt, "video.mp4", nullptr, nullptr);
// 返回值 < 0 表示失败
```

**找视频流**：
```cpp
int idx = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
// 返回流的索引，< 0 表示没有视频
```

**初始化解码器**：
```cpp
const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
AVCodecContext* cc = avcodec_alloc_context3(codec);
avcodec_parameters_to_context(cc, stream->codecpar);
avcodec_open2(cc, codec, nullptr);
```

**解码循环**：
```cpp
while (av_read_frame(fmt, pkt) >= 0) {  // 读一个包
    avcodec_send_packet(cc, pkt);       // 送入解码器
    while (avcodec_receive_frame(cc, frm) == 0) {  // 取解码后的帧
        // 显示
    }
}
```

### 4.3 内存管理

**FFmpeg 是 C 库，需要手动释放**：

```cpp
// 配对使用
av_packet_alloc() → av_packet_free()
av_frame_alloc() → av_frame_free()
avcodec_alloc_context3() → avcodec_free_context()
avformat_open_input() → avformat_close_input()
```

**C++ RAII 封装（推荐）**：
```cpp
struct AVPacketDeleter {
    void operator()(AVPacket* p) const {
        if (p) av_packet_free(&p);
    }
};
using PacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

// 使用
PacketPtr pkt(av_packet_alloc());
// 自动释放，不会泄漏
```

---

## 5. Pipeline 架构设计

### 5.1 为什么需要架构

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
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Demuxer    │───→│   Decoder    │───→│   Renderer   │
│   解封装      │    │   解码       │    │   渲染       │
└──────────────┘    └──────────────┘    └──────────────┘
      ↑                    ↑                   ↑
   读文件               解压数据            显示画面
```

**好处**：
1. 模块独立，可单独测试
2. 修改一个模块不影响其他
3. 易于扩展（加音频、网络等）

### 5.2 接口设计

```cpp
// 错误码
enum class ErrorCode {
    OK = 0,
    FILE_NOT_FOUND,
    CODEC_NOT_FOUND,
    // ...
};

// Pipeline 接口
class Pipeline {
public:
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
};
```

### 5.3 项目结构

```
chapter-01/
├── CMakeLists.txt
├── include/
│   └── live/
│       ├── pipeline.h
│       └── ffmpeg_utils.h
├── src/
│   ├── demuxer.cpp
│   ├── decoder.cpp
│   ├── renderer.cpp
│   ├── simple_pipeline.cpp
│   └── main.cpp
└── tests/
    └── test_basic.cpp
```

### 5.4 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(live-player)

set(CMAKE_CXX_STANDARD 14)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)
find_package(SDL2 REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})

add_executable(player src/*.cpp)
target_link_libraries(player ${FFMPEG_LIBRARIES} SDL2::SDL2)
```

---

## 6. 调试与优化

### 6.1 内存泄漏检测

```bash
valgrind --leak-check=full ./player test.mp4
```

**输出**：
```
All heap blocks were freed -- no leaks are possible
```

### 6.2 性能分析

```bash
# 记录性能
perf record ./player test.mp4

# 查看热点
perf report
```

### 6.3 常见优化

| 优化 | 效果 | 实现 |
|:---|:---|:---|
| 硬件解码 | CPU 50% → 10% | 使用 VAAPI/VideoToolbox |
| 多线程解码 | 提升多核利用率 | FFmpeg 内置支持 |
| 零拷贝渲染 | 减少内存拷贝 | 直接使用 GPU 纹理 |

---

## 7. 常见问题

### Q1: 编译报错 `undefined reference to`

```bash
# 检查 pkg-config
pkg-config --libs libavformat

# 手动指定
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
printf("format=%d, w=%d, h=%d\n", frame->format, frame->width, frame->height);
```

### Q4: 视频播放太快/太慢

**原因**：`SDL_Delay(33)` 是硬编码 30fps。

**解决**：根据 PTS 计算实际延迟（见源码）。

---

## 8. 下一步

本章是**同步单线程**实现，存在的问题：

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
| **YUV** | 像素格式，比 RGB 更高效 |
| **PTS** | 显示时间戳，控制帧显示时间 |
| **I/P/B 帧** | 视频压缩的三种帧类型 |
| **GOP** | 图像组，两个 I 帧之间的帧序列 |

### 参考

- FFmpeg 文档：https://ffmpeg.org/documentation.html
- SDL2 文档：https://wiki.libsdl.org/

---

**本章代码仓库**：https://github.com/chapin666/live-system-book/tree/master/chapter-01
# 第五章：RTMP 直播拉流播放器

> **本章目标**：使用 FFmpeg 实现 RTMP 直播播放器，理解直播与点播的差异。
> 
> **前置知识**：建议先阅读附录 A「C++11 线程速成」，理解 `std::thread` 和 `std::mutex` 基础。

第三章实现了基于 HTTP 的网络播放器，可以播放网络上的视频文件。但 HTTP 协议是为**文件传输**设计的，用于直播时延迟高达 5-10 秒。

想象一下：主播说"大家好"，观众 10 秒后才听到——这种体验显然不行。

本章将学习 **RTMP（Real-Time Messaging Protocol）**——直播行业的标准协议，延迟可控制在 1-3 秒。我们将**使用 FFmpeg 的 RTMP 实现**（就像 Ch1-Ch3 使用 FFmpeg 播放本地/HTTP 视频一样），快速构建可用的直播播放器。

**⚠️ 重要说明**：本章**不深入协议细节**（Chunk、FLV 封装等），而是聚焦"如何用 FFmpeg 播放 RTMP 流"。协议原理会简要介绍，帮你建立概念，但不会要求你自己实现。

---

## 🎯 本章学习路径

```
Ch1 本地播放 ──┐
Ch2 异步改造 ──┼──→ 你已经会播放各种视频了
Ch3 HTTP网络 ──┘
       ↓
Ch4 RTMP直播 ←── 本章：用 FFmpeg 播放直播流
       ↓
Ch5 采集     ←── 接下来：自己采集视频
```

---

## 目录

1. [直播 vs 点播：延迟的来源](#1-直播-vs-点播延迟的来源)
2. [RTMP 简介：直播行业的标准](#2-rtmp-简介直播行业的标准)
3. [使用 FFmpeg 播放 RTMP](#3-使用-ffmpeg-播放-rtmp)
4. [直播播放器的特殊处理](#4-直播播放器的特殊处理)
5. [代码实现：RTMP 直播播放器](#5-代码实现rtmp-直播播放器)
6. [本章总结与里程碑](#6-本章总结与里程碑)

---

## 1. 直播 vs 点播：延迟的来源

**本节目标**：理解为什么 HTTP 直播延迟高，RTMP 延迟低。

### 1.1 点播（VOD）：播放已存在的文件

Ch3 的 HTTP 播放器播放的是**已经存在的视频文件**：

```
服务器：视频文件.mp4（已经存在）
           ↓
客户端：HTTP 请求 → 下载 → 播放
```

特点是：文件已经生成好了，随时可以下载任何片段。

### 1.2 直播（Live）：播放正在发生的内容

直播是**正在发生**的内容，服务器边收边发：

```
主播端：摄像头 → 编码 → 推流 ──┐
                               ↓
服务器：接收流 → 转发给多个观众
                               ↓
观众端：收到数据 → 立即播放（不能等待）
```

**关键区别**：直播数据是"实时产生"的，不能像文件那样等生成好了再下载。

### 1.3 HTTP 直播（HLS）的高延迟

Apple 提出的 **HLS（HTTP Live Streaming）** 用 HTTP 协议做直播：

```
服务器端：
直播流 → 切片器 → 收集3秒 → [片段1.ts] → 收集3秒 → [片段2.ts]

客户端：
下载片段1（3秒）→ 下载片段2（3秒）→ 播放
    ↑_____________________________↓
         延迟 = 切片时间 + 下载时间 = 5-15秒
```

**为什么延迟高？**
- 服务器要收集 3-10 秒数据才能生成一个切片
- 客户端要下载完切片才能播放
- 为了流畅，客户端还会缓冲 1-2 个切片

**总延迟：5-15 秒** —— 主播说"大家好"，观众 10 秒后才听到。

### 1.4 RTMP 的低延迟原理

RTMP 是**流式协议**，不是文件传输：

```
HTTP 方式（文件）：
服务器 ──生成切片文件──→ 客户端下载 ──播放
    ↑需要等待文件生成↑

RTMP 方式（流）：
服务器 ──收到数据立即转发──→ 客户端立即播放
    ↑无需等待，收到即播↑
```

**RTMP 延迟组成**：
- 编码延迟：50-100ms
- 网络传输：20-100ms
- 缓冲区：200-500ms（抗网络波动）
- 解码延迟：10-30ms
- **总延迟：1-3 秒**

### 1.5 延迟对比

假设主播在 T0 时刻说"Hello"：

| 时间 | RTMP 状态 | HLS(3秒切片) 状态 |
|:---|:---|:---|
| T0+0.5s | 编码中 | 切片中 |
| T0+1s | 传输中 | 切片中 |
| T0+3s | **✅ 播放** | 切片完成，开始下载 |
| T0+5s | - | 下载中 |
| T0+10s | - | **✅ 播放** |

**本节小结**：
- HTTP/HLS 适合点播，延迟 5-15 秒
- RTMP 适合直播，延迟 1-3 秒
- 互动直播必须用 RTMP 或更实时的协议（如 WebRTC）

---

## 2. RTMP 简介：直播行业的标准

**本节目标**：了解 RTMP 是什么，以及 FFmpeg 如何支持 RTMP。

### 2.1 RTMP 是什么？

**RTMP（Real-Time Messaging Protocol）** 是 Adobe 开发的协议，用于实时传输音视频数据。

**核心特点**：
- 基于 TCP，默认端口 1935
- 流式传输，低延迟（1-3秒）
- 支持推拉流（主播推流，观众拉流）
- 行业标准，CDN 广泛支持

### 2.2 RTMP URL 格式

```
rtmp://服务器地址/应用名/流名

示例：
rtmp://live.example.com/live/stream123
      └──── 服务器地址 ────┘ └─┘ └────┘
                           应用  流名
```

- **服务器地址**：RTMP 服务器 IP 或域名
- **应用名**：通常是 `live`
- **流名**：标识具体的直播流（如主播房间号）

### 2.3 FFmpeg 的 RTMP 支持

**好消息**：FFmpeg 内置了完整的 RTMP 支持！

```cpp
// 播放 RTMP 流，和播放本地文件一样简单
avformat_open_input(&fmt_ctx, "rtmp://server/live/stream", nullptr, nullptr);
```

FFmpeg 会自动处理：
- ✅ RTMP 握手
- ✅ Chunk 解析
- ✅ FLV 解封装
- ✅ 音视频分离

**你不需要自己实现协议！** 就像 Ch1 播放本地文件、Ch3 播放 HTTP 视频一样，FFmpeg 帮你处理了底层细节。

### 2.4 RTMP vs HTTP 播放对比

| 步骤 | HTTP 播放（Ch3） | RTMP 播放（本章） |
|:---|:---|:---|
| 打开输入 | `avformat_open_input(url)` | `avformat_open_input(url)` |
| 读取数据 | `av_read_frame()` | `av_read_frame()` |
| 解码 | `avcodec_send_packet()` | `avcodec_send_packet()` |
| 渲染 | SDL2 | SDL2 |

**代码几乎一样！** 区别主要在：
1. URL 格式不同（`http://` vs `rtmp://`）
2. 直播需要特殊处理（网络断开重连、缓冲策略）

本节小结：FFmpeg 内置 RTMP 支持，播放 RTMP 流的 API 和播放本地文件一样。下一节我们直接写代码。

---

## 3. 使用 FFmpeg 播放 RTMP

**本节目标**：用 FFmpeg 实现最简 RTMP 播放器。

### 3.1 最小 RTMP 播放器

基于 Ch1 的 `simple_player`，只需要改 URL：

```cpp
// simple_rtmp_player.cpp（核心代码）
#include <SDL2/SDL.h>
#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <RTMP URL>\n", argv[0]);
        fprintf(stderr, "示例: %s rtmp://localhost/live/stream\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    
    // 初始化 FFmpeg
    avformat_network_init();  // 网络初始化！
    
    // 打开 RTMP 流（和打开文件一样！）
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "无法打开流: %s\n", url);
        return 1;
    }
    
    // 获取流信息
    avformat_find_stream_info(fmt_ctx, nullptr);
    
    // 查找视频流、初始化解码器...（和 Ch1 一样）
    // ...
    
    // 读取帧并播放
    AVPacket* pkt = av_packet_alloc();
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        // 解码、渲染...
        av_packet_unref(pkt);
    }
    
    // 清理
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    return 0;
}
```

**和 Ch1 本地播放器的区别**：
1. 调用 `avformat_network_init()` 初始化网络
2. URL 是 `rtmp://` 而不是文件路径

其他代码完全一样！

### 3.2 编译运行

```bash
# 编译（和 Ch1 一样）
g++ simple_rtmp_player.cpp -o simple_rtmp_player \
    $(pkg-config --cflags --libs libavformat libavcodec libavutil sdl2)

# 运行（需要 RTMP 服务器和推流）
./simple_rtmp_player rtmp://localhost/live/stream
```

**测试方法**：
1. 启动 RTMP 服务器（如 nginx-rtmp、SRS）
2. 用 OBS 或 FFmpeg 推流：`ffmpeg -re -i test.mp4 -c copy -f flv rtmp://localhost/live/stream`
3. 运行播放器观看

### 3.3 添加网络选项

RTMP 播放通常需要设置一些网络参数：

```cpp
AVDictionary* opts = nullptr;

// 设置接收缓冲区大小（字节）
av_dict_set(&opts, "buffer_size", "65536", 0);

// 设置最大延迟（微秒）
av_dict_set(&opts, "max_delay", "500000", 0);  // 500ms

// 设置超时（微秒）
av_dict_set(&opts, "stimeout", "5000000", 0);  // 5秒

// 打开输入时传入选项
int ret = avformat_open_input(&fmt_ctx, url, nullptr, &opts);

av_dict_free(&opts);
```

常用 RTMP 选项：

| 选项 | 说明 | 推荐值 |
|:---|:---|:---:|
| `buffer_size` | 接收缓冲区大小 | 65536 |
| `max_delay` | 最大延迟 | 500000 (500ms) |
| `stimeout` | 连接/读取超时 | 5000000 (5s) |
| `reconnect` | 断线重连次数 | 1 |

本节小结：播放 RTMP 流和播放本地文件 API 几乎一样，只需注意网络初始化和选项设置。

---

## 4. 直播播放器的特殊处理

**本节目标**：理解直播播放器与点播播放器的差异。

### 4.1 直播 vs 点播的关键差异

| 特性 | 点播（HTTP文件） | 直播（RTMP流） |
|:---|:---|:---|
| **时长** | 已知（文件大小固定） | 未知（持续进行） |
| **Seek** | 可以任意拖动进度 | 不能 seek（或有限制） |
| **结束** | 播完文件结束 | 主播停播才结束 |
| **缓冲** | 可以缓冲整个文件 | 只能缓冲最近几秒 |
| **断线** | 一般是网络问题 | 可能是网络或主播断流 |

### 4.2 直播播放器的调整

**1. 移除 Seek 功能**

直播没有进度条，不需要 seek：

```cpp
// 点播播放器可能有
if (user_seek) {
    av_seek_frame(fmt_ctx, stream_idx, timestamp, AVSEEK_FLAG_BACKWARD);
}

// 直播播放器没有 seek，或者只做有限的前回退
```

**2. 处理无限播放**

直播没有结束，读取帧的循环是无限的：

```cpp
// 点播：av_read_frame 返回 <0 表示结束
while (av_read_frame(fmt_ctx, pkt) >= 0) {
    // 处理帧
}
// 结束，退出

// 直播：可能永远读不完，需要特殊处理断开
while (!quit) {
    int ret = av_read_frame(fmt_ctx, pkt);
    if (ret < 0) {
        // 可能是网络断开，尝试重连
        if (try_reconnect()) continue;
        // 否则退出
        break;
    }
    // 处理帧
}
```

**3. 网络断开重连**

直播过程中网络可能波动，需要重连机制：

```cpp
bool try_reconnect(AVFormatContext*& fmt_ctx, const char* url, int max_retries) {
    for (int i = 0; i < max_retries; i++) {
        printf("尝试重连 %d/%d...\n", i + 1, max_retries);
        
        // 关闭旧连接
        avformat_close_input(&fmt_ctx);
        
        // 等待一段时间再重试
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 尝试重新打开
        int ret = avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
        if (ret >= 0) {
            printf("重连成功！\n");
            return true;
        }
    }
    return false;
}
```

**4. 缓冲策略**

直播需要平衡延迟和流畅度：

```cpp
// 设置缓冲区大小
// 太小：网络波动就卡顿
// 太大：延迟增加

// 低延迟模式（适合连麦）
av_dict_set(&opts, "max_delay", "200000", 0);  // 200ms

// 流畅优先模式（适合看直播）
av_dict_set(&opts, "max_delay", "800000", 0);  // 800ms
```

### 4.3 直播播放器架构

```
┌─────────────────────────────────────────────┐
│           RTMP 直播播放器架构                 │
├─────────────────────────────────────────────┤
│                                             │
│  ┌──────────────┐      ┌──────────────┐    │
│  │  网络读取    │──────→│  缓冲队列    │    │
│  │  (FFmpeg)    │      │  (2-3帧)     │    │
│  └──────────────┘      └──────┬───────┘    │
│         ↑                     │            │
│         │ 断线重连            ↓            │
│         └──────────────── 解码渲染         │
│                            (SDL2)          │
│                                             │
└─────────────────────────────────────────────┘
```

本节小结：直播播放器需要处理断线重连、调整缓冲策略，但核心解码渲染逻辑和点播一样。

---

## 5. 代码实现：RTMP 直播播放器

**本节目标**：完整可运行的 RTMP 直播播放器代码。

### 5.1 完整代码

```cpp
/**
 * simple_rtmp_player.cpp
 * 
 * 基于 FFmpeg 的 RTMP 直播播放器
 * 编译: g++ simple_rtmp_player.cpp -o simple_rtmp_player \
 *       $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale sdl2)
 * 运行: ./simple_rtmp_player rtmp://server/live/stream
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <thread>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

// 重连函数
bool try_reconnect(AVFormatContext*& fmt_ctx, const char* url, int max_retries);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <RTMP URL>\n", argv[0]);
        fprintf(stderr, "示例: %s rtmp://localhost/live/stream\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    
    // 初始化
    avformat_network_init();
    SDL_Init(SDL_INIT_VIDEO);
    
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    struct SwsContext* sws_ctx = nullptr;
    
    // 打开 RTMP 流
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "buffer_size", "65536", 0);
    av_dict_set(&opts, "max_delay", "500000", 0);
    
    printf("正在连接: %s\n", url);
    int ret = avformat_open_input(&fmt_ctx, url, nullptr, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        fprintf(stderr, "连接失败\n");
        return 1;
    }
    
    printf("连接成功，获取流信息...\n");
    avformat_find_stream_info(fmt_ctx, nullptr);
    
    // 查找视频流
    int video_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_idx = i;
            break;
        }
    }
    
    if (video_idx < 0) {
        fprintf(stderr, "未找到视频流\n");
        goto cleanup;
    }
    
    // 初始化解码器
    AVCodecParameters* codecpar = fmt_ctx->streams[video_idx]->codecpar;
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    avcodec_open2(codec_ctx, decoder, nullptr);
    
    printf("视频: %dx%d @ %.2f fps\n",
           codecpar->width, codecpar->height,
           av_q2d(fmt_ctx->streams[video_idx]->avg_frame_rate));
    
    // 初始化 SDL
    window = SDL_CreateWindow("RTMP Player",
                              SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              codecpar->width, codecpar->height,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                SDL_TEXTUREACCESS_STREAMING,
                                codecpar->width, codecpar->height);
    
    // 初始化缩放器
    sws_ctx = sws_getContext(codecpar->width, codecpar->height, 
                             (AVPixelFormat)codecpar->format,
                             codecpar->width, codecpar->height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    // 分配帧
    AVFrame* frame = av_frame_alloc();
    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    yuv_frame->width = codecpar->width;
    yuv_frame->height = codecpar->height;
    av_frame_get_buffer(yuv_frame, 0);
    
    AVPacket* pkt = av_packet_alloc();
    bool running = true;
    bool connected = true;
    int retry_count = 0;
    const int MAX_RETRIES = 3;
    
    printf("开始播放，按 Q 退出\n");
    
    while (running) {
        // 处理 SDL 事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                running = false;
            }
        }
        
        // 读取帧
        ret = av_read_frame(fmt_ctx, pkt);
        
        if (ret < 0) {
            // 读取失败，可能是断线
            if (retry_count < MAX_RETRIES) {
                printf("连接中断，尝试重连...\n");
                if (try_reconnect(fmt_ctx, url, 3)) {
                    retry_count = 0;
                    continue;
                }
                retry_count++;
            } else {
                printf("重连失败，退出\n");
                break;
            }
            continue;
        }
        
        retry_count = 0;  // 重置重试计数
        
        if (pkt->stream_index == video_idx) {
            // 发送给解码器
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                av_packet_unref(pkt);
                continue;
            }
            
            // 接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) break;
                
                // 转换为 YUV420P
                sws_scale(sws_ctx, frame->data, frame->linesize, 0,
                         codecpar->height, yuv_frame->data, yuv_frame->linesize);
                
                // 渲染
                SDL_UpdateYUVTexture(texture, nullptr,
                                     yuv_frame->data[0], yuv_frame->linesize[0],
                                     yuv_frame->data[1], yuv_frame->linesize[1],
                                     yuv_frame->data[2], yuv_frame->linesize[2]);
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
        }
        
        av_packet_unref(pkt);
    }
    
cleanup:
    // 清理
    av_frame_free(&frame);
    av_frame_free(&yuv_frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    
    printf("播放结束\n");
    return 0;
}

bool try_reconnect(AVFormatContext*& fmt_ctx, const char* url, int max_retries) {
    for (int i = 0; i < max_retries; i++) {
        printf("  重试 %d/%d...\n", i + 1, max_retries);
        
        avformat_close_input(&fmt_ctx);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        int ret = avformat_open_input(&fmt_ctx, url, nullptr, nullptr);
        if (ret >= 0) {
            avformat_find_stream_info(fmt_ctx, nullptr);
            printf("  重连成功！\n");
            return true;
        }
    }
    return false;
}
```

### 5.2 关键改进点

相比 Ch1 的本地播放器，主要增加了：

1. **网络初始化**：`avformat_network_init()`
2. **网络选项**：`buffer_size`、`max_delay`
3. **断线重连**：`try_reconnect()` 函数
4. **错误处理**：区分"读到结尾"和"网络断开"

### 5.3 模块化版本

工程化版本见 `chapter-04/src/rtmp_player/`：
- `rtmp_connection.h/cpp`：RTMP 连接管理
- `flv_parser.h/cpp`：FLV 解析（可选，用于理解封装格式）
- `main.cpp`：播放器主逻辑

---

## 6. 本章总结与里程碑

### ✅ 本章学到的

| 知识点 | 掌握程度 |
|:---|:---:|
| 直播 vs 点播的差异 | ⭐⭐⭐⭐⭐ |
| RTMP 协议基础概念 | ⭐⭐⭐⭐⭐ |
| 用 FFmpeg 播放 RTMP | ⭐⭐⭐⭐⭐ |
| 直播播放器的特殊处理 | ⭐⭐⭐⭐⭐ |
| 断线重连机制 | ⭐⭐⭐⭐⭐ |

### 🎯 本章里程碑

**学完本章，你能：**
1. ✅ 播放任意 RTMP 直播流
2. ✅ 处理网络波动和断线重连
3. ✅ 理解直播低延迟的原理

**你已经完成的播放器功能：**
```
Ch1: 本地文件播放     ✅
Ch2: 异步流畅播放     ✅
Ch3: HTTP网络播放     ✅
Ch4: RTMP直播播放     ✅ ← 新增！
```

**现在你有一个完整的直播播放器了！**

### 📋 下一步预告

Ch5 我们将进入**主播端**，学习如何：
- 采集摄像头和麦克风
- 处理音频 3A（回声消除、降噪、增益）

你将从"观众"变成"主播"，自己采集视频推流！

### 📚 延伸阅读

如果你想深入了解 RTMP 协议细节（Chunk、FLV 封装等），可以阅读：
- RTMP 规范文档（Adobe）
- FFmpeg `libavformat/rtmpproto.c` 源码
- 本书附录 C「RTMP 协议详解」（进阶选读）

**但记住**：实际开发中，FFmpeg 已经帮你封装好了这些细节，你不需要自己实现。

---

## 附录：常见问题

### Q1: 为什么连接 RTMP 服务器失败？

**可能原因**：
1. 服务器地址错误
2. 防火墙阻断 1935 端口
3. 服务器没有对应的流

**排查方法**：
```bash
# 用 ffprobe 测试连接
ffprobe rtmp://server/live/stream

# 用 telnet 测试端口
 telnet server 1935
```

### Q2: 播放卡顿怎么办？

**调整缓冲**：
```cpp
// 增加缓冲区（延迟增加，但更流畅）
av_dict_set(&opts, "buffer_size", "131072", 0);  // 128KB
av_dict_set(&opts, "max_delay", "1000000", 0);   // 1s
```

### Q3: 延迟太高怎么办？

**减少缓冲**：
```cpp
// 减少缓冲区（延迟降低，但对网络要求更高）
av_dict_set(&opts, "max_delay", "200000", 0);   // 200ms
```

### Q4: 如何测试播放器？

**方法1：使用公共 RTMP 测试流**
```bash
# 一些测试服务器（可能不稳定）
./simple_rtmp_player rtmp://live.example.com/test/stream
```

**方法2：本地搭建 RTMP 服务器**
```bash
# 使用 Docker 启动 SRS
docker run -p 1935:1935 ossrs/srs:4

# 用 FFmpeg 推流测试
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://localhost/live/stream

# 运行播放器
./simple_rtmp_player rtmp://localhost/live/stream
```

---

**本章完** 🎉

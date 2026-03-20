# 第一章：本地播放器

**目标**：运行 `./live-player video.mp4`，弹出窗口播放视频。

**预计时间**：阅读 40 分钟，动手 20 分钟。

---

## 1. 视频文件里到底存了什么？

想象你有一个视频文件 `video.mp4`，大小 100MB，时长 1 分钟。

### 1.1 如果不压缩，视频有多大？

视频的本质是**快速播放的一系列图片**。

假设：
- 分辨率：1920 × 1080（全高清，也叫 1080p）
- 帧率：每秒 30 张图片
- 时长：1 分钟 = 60 秒

**计算一张图片的大小**：

电脑屏幕显示颜色用 **RGB** 模式：
- R = Red（红色）
- G = Green（绿色）  
- B = Blue（蓝色）

每种颜色用 **1 个字节（Byte）** 表示，范围是 0-255。

为什么是 0-255？因为 1 字节 = 8 位（bit），2^8 = 256。

所以一个像素需要：
```
1 字节（R）+ 1 字节（G）+ 1 字节（B）= 3 字节
```

这就是为什么之前说 **3 字节**。

一张 1920 × 1080 的图片：
```
1920 × 1080 × 3 字节 = 6,220,800 字节 ≈ 6 MB
```

**计算 1 秒视频的大小**：
```
每秒 30 张图片 × 6 MB/张 = 180 MB/秒
```

**计算 1 分钟视频的大小**：
```
180 MB/秒 × 60 秒 = 10,800 MB = 10.8 GB
```

**结论**：
- 不压缩的 1 分钟 1080p 视频 ≈ **10.8 GB**
- 实际的 MP4 文件可能只有 **100 MB**
- 压缩了约 **100 倍**！

### 1.2 为什么能压缩 100 倍？

视频压缩利用了两种冗余：

#### 空间冗余（单张图片内部）

一张风景照片：
- 天空区域：几万像素都是类似的蓝色
- 草地区域：几万像素都是类似的绿色

不需要存每个像素的具体颜色，只需要存：
- "天空区域从 (100, 50) 到 (800, 200) 都是浅蓝色"
- 用数学方法（DCT 变换）高效表示颜色变化

#### 时间冗余（帧与帧之间）

视频的特点是**连续帧变化很小**：
- 第 1 帧：完整的画面（I 帧，也叫关键帧）
- 第 2 帧：只存"人走动了一步"的变化（P 帧，预测帧）
- 第 3 帧：只存"手抬起来"的变化（P 帧）

如果画面没变化，理论上只需要存 "和上一帧一样"，几乎不占用空间。

### 1.3 视频播放需要哪几步？

要把压缩的视频显示出来，需要经历：

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│ MP4文件  │     │ H.264    │     │ YUV像素  │     │ 屏幕显示 │
│ (100MB)  │ ──▶ │ 压缩数据  │ ──▶ │ (原始)   │ ──▶ │ (RGB)    │
└──────────┘     └──────────┘     └──────────┘     └──────────┘
      │                │                │                │
      ▼                ▼                ▼                ▼
   解封装           解码              颜色转换          显示
   Demuxer         Decoder          (自动完成)        Renderer
```

#### 第一步：解封装（Demux）

MP4 是一种"容器格式"（Container Format）。

**容器的概念**：

想象 MP4 是一个 ZIP 压缩包，里面可以装：
- 视频流（H.264 编码）
- 音频流（AAC 编码）
- 字幕
- 封面图
- 元数据（标题、作者、时长等）

**解封装的作用**：

把 MP4 文件拆开，取出里面的视频流（压缩的 H.264 数据）。

类比：
- 解封装 = 解压 ZIP 文件，取出里面的文件
- 视频流 = ZIP 里的一个文件

#### 第二步：解码（Decode）

H.264 是一种视频编码标准（还有 H.265/HEVC、VP9、AV1 等）。

**编码的作用**：

把原始像素（很大）压缩成二进制数据（很小）。

**解码的作用**：

反过来，把二进制数据还原成原始像素。

解码后的像素格式通常是 **YUV**，而不是 RGB。

**为什么用 YUV？**

人眼的特性：
- 对亮度（明暗）敏感
- 对色度（颜色）不太敏感

YUV 把亮度和色度分开：
- Y = Luma（亮度）
- U = Chroma Blue（蓝色色度）
- V = Chroma Red（红色色度）

YUV420P 的含义：
- Y 分辨率：1920 × 1080（完整）
- U 分辨率：960 × 540（1/4）
- V 分辨率：960 × 540（1/4）

UV 用 1/4 分辨率，因为人眼看不出区别，但省了一半空间。

#### 第三步：渲染（Render）

显示到屏幕上。

现代显卡可以直接渲染 YUV 格式，不需要先转成 RGB（省了一次转换）。

我们用的 SDL2 库支持直接显示 YUV 纹理。

---

## 2. 准备工作

### 2.1 安装 FFmpeg

FFmpeg 是一套开源的音视频处理工具，包含：
- 命令行工具（`ffmpeg`, `ffprobe`, `ffplay`）
- 开发库（`libavformat`, `libavcodec`, `libavutil` 等）

**为什么需要 FFmpeg？**

FFmpeg 实现了几乎所有的音视频格式和编解码器。

开发播放器不需要自己写 H.264 解码器（太复杂了），直接调用 FFmpeg 的 API。

**macOS 安装：**
```bash
brew install ffmpeg
```

**Ubuntu/Debian 安装：**
```bash
sudo apt-get update
sudo apt-get install ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

**验证安装：**
```bash
ffmpeg -version
```

应该看到类似输出：
```
ffmpeg version 6.0
built with Apple clang version 14.0.0
...
```

### 2.2 安装 SDL2

SDL（Simple DirectMedia Layer）是一个跨平台的多媒体库。

**功能**：
- 创建窗口
- 处理键盘鼠标事件
- 显示图像（2D 图形）
- 播放音频

我们主要用它的窗口创建和图像显示功能。

**macOS 安装：**
```bash
brew install sdl2
```

**Ubuntu 安装：**
```bash
sudo apt-get install libsdl2-dev
```

**验证安装：**
```bash
sdl2-config --version
```

### 2.3 安装 CMake

CMake 是一个构建工具，用于：
1. 检测系统环境（查找 FFmpeg、SDL2 安装位置）
2. 生成编译配置文件（Makefile 或 Xcode/VS 项目）
3. 调用编译器生成可执行文件

**macOS 安装：**
```bash
brew install cmake
```

**Ubuntu 安装：**
```bash
sudo apt-get install cmake
```

**验证安装：**
```bash
cmake --version
```

---

## 3. 代码结构

我们的播放器分为三个模块：

```
┌─────────────────────────────────────────────────────────────────┐
│                           main.cpp                               │
│                      （把三个模块串起来）                          │
└───────────────┬──────────────────────┬──────────────────────────┘
                │                      │
       ┌────────▼────────┐    ┌────────▼────────┐
       │    Demuxer      │    │     Decoder     │
       │    解封装器      │    │      解码器      │
       │                 │    │                 │
       │ 输入：MP4 文件   │    │ 输入：H.264 数据 │
       │ 输出：AVPacket  │───▶│ 输出：AVFrame   │
       │  (压缩数据包)    │    │  (YUV 像素帧)   │
       └─────────────────┘    └────────┬────────┘
                                       │
                              ┌────────▼────────┐
                              │     Renderer    │
                              │      渲染器      │
                              │                 │
                              │ 输入：AVFrame   │
                              │ 输出：屏幕显示   │
                              └─────────────────┘
```

### 3.1 Demuxer 模块详解

**职责**：读取视频文件，提取压缩数据。

**核心数据结构**：

```cpp
AVFormatContext  // 格式上下文，代表整个文件
├── streams[]    // 流数组（视频流、音频流、字幕流）
├── duration     // 总时长
└── metadata     // 元数据（标题、作者等）

AVPacket         // 压缩数据包
├── data         // 数据指针（H.264 NAL 单元）
├── size         // 数据大小
├── pts          // 显示时间戳（Presentation Time Stamp）
├── dts          // 解码时间戳（Decode Time Stamp）
└── stream_index // 属于哪个流
```

**关键函数**：

```cpp
// 打开视频文件
int avformat_open_input(AVFormatContext **ps, const char *url, ...);

// 读取流信息（解析 moov box 等）
int avformat_find_stream_info(AVFormatContext *ic, ...);

// 读取一个数据包
int av_read_frame(AVFormatContext *s, AVPacket *pkt);

// 释放数据包
void av_packet_unref(AVPacket *pkt);
```

**代码实现**（简化版）：

```cpp
class Demuxer {
public:
    bool Open(const std::string& url) {
        // 1. 打开文件
        avformat_open_input(&fmt_ctx_, url.c_str(), nullptr, nullptr);
        
        // 2. 读取流信息
        avformat_find_stream_info(fmt_ctx_, nullptr);
        
        // 3. 找到视频流
        video_stream_idx_ = av_find_best_stream(
            fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        
        return true;
    }
    
    bool ReadPacket(AVPacket* packet) {
        return av_read_frame(fmt_ctx_, packet) >= 0;
    }
    
private:
    AVFormatContext* fmt_ctx_ = nullptr;
    int video_stream_idx_ = -1;
};
```

### 3.2 Decoder 模块详解

**职责**：把压缩数据（H.264）解码成原始像素（YUV）。

**核心数据结构**：

```cpp
AVCodec          // 编解码器（H.264 解码器、H.265 解码器等）
├── name         // 编码器名称（"h264", "hevc" 等）
└── id           // 编码器 ID（AV_CODEC_ID_H264）

AVCodecContext   // 编解码器上下文（运行时状态）
├── width        // 视频宽度
├── height       // 视频高度
├── pix_fmt      // 像素格式（YUV420P, YUV422P 等）
└── thread_count // 解码线程数

AVFrame          // 解码后的视频帧
├── data[0]      // Y 平面指针
├── data[1]      // U 平面指针
├── data[2]      // V 平面指针
├── linesize[0]  // Y 平面每行字节数
├── linesize[1]  // U 平面每行字节数
├── linesize[2]  // V 平面每行字节数
├── width        // 帧宽度
├── height       // 帧高度
└── pts          // 显示时间戳
```

**关键函数**：

```cpp
// 查找解码器
AVCodec* avcodec_find_decoder(AVCodecID id);

// 分配编解码器上下文
AVCodecContext* avcodec_alloc_context3(const AVCodec *codec);

// 复制参数到上下文
int avcodec_parameters_to_context(AVCodecContext *codec, const AVCodecParameters *par);

// 打开编解码器
int avcodec_open2(AVCodecContext *avctx, const AVCodec *codec, AVDictionary **options);

// 发送压缩数据包
int avcodec_send_packet(AVCodecContext *avctx, const AVPacket *avpkt);

// 接收解码后的帧
int avcodec_receive_frame(AVCodecContext *avctx, AVFrame *frame);
```

**为什么 send 和 receive 分离？**

因为解码器内部有缓冲和状态：
- 一个输入 packet 可能输出多个 frame（B 帧重排序）
- 或者需要多个 packet 才能输出一个 frame（需要参考帧）

**代码实现**（简化版）：

```cpp
class Decoder {
public:
    bool Init(const AVCodecParameters* codecpar) {
        // 1. 找到解码器
        codec_ = avcodec_find_decoder(codecpar->codec_id);
        
        // 2. 分配上下文
        dec_ctx_ = avcodec_alloc_context3(codec_);
        
        // 3. 复制参数
        avcodec_parameters_to_context(dec_ctx_, codecpar);
        
        // 4. 打开解码器
        avcodec_open2(dec_ctx_, codec_, nullptr);
        
        return true;
    }
    
    bool SendPacket(const AVPacket* packet) {
        return avcodec_send_packet(dec_ctx_, packet) >= 0;
    }
    
    bool ReceiveFrame(AVFrame* frame) {
        return avcodec_receive_frame(dec_ctx_, frame) >= 0;
    }
    
private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* dec_ctx_ = nullptr;
};
```

### 3.3 Renderer 模块详解

**职责**：把 YUV 像素显示到屏幕上。

**核心数据结构**：

```cpp
SDL_Window      // SDL 窗口
├── width       // 窗口宽度
├── height      // 窗口高度
└── title       // 窗口标题

SDL_Renderer    // 渲染器（2D 图形渲染上下文）

SDL_Texture     // 纹理（GPU 显存中的一块区域）
├── format      // 像素格式（我们使用 SDL_PIXELFORMAT_YV12）
├── width       // 纹理宽度
└── height      // 纹理高度
```

**关键函数**：

```cpp
// 初始化 SDL 视频子系统
int SDL_Init(Uint32 flags);

// 创建窗口
SDL_Window* SDL_CreateWindow(const char* title, int x, int y, 
                             int w, int h, Uint32 flags);

// 创建渲染器
SDL_Renderer* SDL_CreateRenderer(SDL_Window* window, int index, Uint32 flags);

// 创建纹理
SDL_Texture* SDL_CreateTexture(SDL_Renderer* renderer, Uint32 format,
                               int access, int w, int h);

// 更新 YUV 纹理
int SDL_UpdateYUVTexture(SDL_Texture* texture, const SDL_Rect* rect,
                         const Uint8* Yplane, int Ypitch,
                         const Uint8* Uplane, int Upitch,
                         const Uint8* Vplane, int Vpitch);

// 复制纹理到渲染器
int SDL_RenderCopy(SDL_Renderer* renderer, SDL_Texture* texture,
                   const SDL_Rect* srcrect, const SDL_Rect* dstrect);

// 显示到屏幕
void SDL_RenderPresent(SDL_Renderer* renderer);
```

**代码实现**（简化版）：

```cpp
class Renderer {
public:
    bool Init(int width, int height) {
        // 1. 初始化 SDL
        SDL_Init(SDL_INIT_VIDEO);
        
        // 2. 创建窗口
        window_ = SDL_CreateWindow("Player", 
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width, height, SDL_WINDOW_SHOWN);
        
        // 3. 创建渲染器
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        
        // 4. 创建 YUV 纹理
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_YV12,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
        return true;
    }
    
    bool RenderFrame(const AVFrame* frame) {
        // 更新 YUV 纹理
        SDL_UpdateYUVTexture(texture_, nullptr,
                             frame->data[0], frame->linesize[0],  // Y
                             frame->data[1], frame->linesize[1],  // U
                             frame->data[2], frame->linesize[2]); // V
        
        // 渲染
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
        return true;
    }
    
private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};
```

---

## 4. 完整代码

### 4.1 主程序 main.cpp

```cpp
// Live Player - Chapter 01
// 本地视频播放器

#include <cstdio>
#include <cstring>

#include "demuxer.h"
#include "decoder.h"
#include "renderer.h"

int main(int argc, char* argv[]) {
    // 检查参数
    if (argc < 2) {
        printf("用法: %s <视频文件>\n", argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    printf("正在播放: %s\n", filename);
    
    // ========== 第1步：解封装 ==========
    printf("[1/3] 打开视频文件...\n");
    
    Demuxer demuxer;
    if (!demuxer.Open(filename)) {
        printf("错误: 无法打开文件\n");
        return 1;
    }
    
    printf("      找到视频流，时长: %.2f 秒\n", demuxer.duration());
    
    // ========== 第2步：初始化解码器 ==========
    printf("[2/3] 初始化解码器...\n");
    
    Decoder decoder;
    AVStream* video_stream = demuxer.video_stream();
    if (!decoder.Init(video_stream->codecpar)) {
        printf("错误: 初始化解码器失败\n");
        return 1;
    }
    
    printf("      分辨率: %dx%d\n", decoder.width(), decoder.height());
    
    // ========== 第3步：初始化渲染器 ==========
    printf("[3/3] 创建窗口...\n");
    
    Renderer renderer;
    if (!renderer.Init(decoder.width(), decoder.height())) {
        printf("错误: 创建窗口失败\n");
        return 1;
    }
    
    // ========== 第4步：播放循环 ==========
    printf("开始播放 (按 ESC 退出)...\n\n");
    
    // 分配 packet 和 frame
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    int frame_count = 0;
    
    while (true) {
        // 4.1 读取一个压缩数据包
        if (!demuxer.ReadPacket(packet)) {
            break;  // 文件结束
        }
        
        // 只处理视频包（跳过音频包）
        if (packet->stream_index != demuxer.video_stream_index()) {
            av_packet_unref(packet);
            continue;
        }
        
        // 4.2 送数据给解码器
        decoder.SendPacket(packet);
        av_packet_unref(packet);
        
        // 4.3 尝试获取解码后的帧
        // 注意：一个 packet 可能解码出多个 frame，所以用 while
        while (decoder.ReceiveFrame(frame)) {
            frame_count++;
            
            // 4.4 显示帧
            renderer.RenderFrame(frame);
            
            // 4.5 处理窗口事件（检查是否按了 ESC）
            if (!renderer.PollEvents()) {
                goto end;  // 用户退出
            }
        }
    }
    
end:
    printf("\n共播放 %d 帧\n", frame_count);
    
    // 释放资源
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    return 0;
}
```

### 4.2 完整模块代码

见 `src/` 目录下的：
- `demuxer.h` / `demuxer.cpp`
- `decoder.h` / `decoder.cpp`
- `renderer.h` / `renderer.cpp`

每个文件都有详细注释，解释每一行代码的作用。

---

## 5. 构建和运行

### 5.1 构建步骤

```bash
# 1. 创建构建目录
mkdir build
cd build

# 2. 生成 Makefile
cmake ..

# 3. 编译
make -j4
```

**cmake 做了什么？**

1. **查找依赖**：
   ```
   -- Found PkgConfig: /opt/homebrew/bin/pkg-config
   -- Checking for module 'libavformat'
   --   Found libavformat, version 60.3.100
   -- Checking for module 'libavcodec'
   --   Found libavcodec, version 60.3.100
   -- Found SDL2: /opt/homebrew/lib/libSDL2.dylib
   ```

2. **生成构建文件**：
   - macOS/Linux：生成 `Makefile`
   - Windows：生成 Visual Studio 项目

3. **配置编译选项**：
   - 头文件搜索路径
   - 库文件链接路径
   - 编译器选项（`-Wall -Wextra` 等）

### 5.2 准备测试视频

如果没有测试视频，可以用 FFmpeg 生成一个：

```bash
# 生成 10 秒的测试视频（640x480，30fps）
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 \
       -pix_fmt yuv420p sample.mp4
```

参数解释：
- `-f lavfi`：使用 Libavfilter 输入
- `-i testsrc`：生成测试图案（彩条）
- `duration=10`：时长 10 秒
- `size=640x480`：分辨率 640x480
- `rate=30`：帧率 30fps
- `-pix_fmt yuv420p`：像素格式 YUV420P

### 5.3 运行

```bash
./live-player sample.mp4
```

**成功现象**：
- 弹出一个窗口
- 显示彩条测试图案
- 窗口标题："Live Player - Chapter 01"
- 按 ESC 键退出

---

## 6. 调试技巧

### 6.1 查看视频文件信息

```bash
ffprobe sample.mp4
```

输出示例：
```
Input #0, mov,mp4,m4a,3gp,3g2,mj2, from 'sample.mp4':
  Duration: 00:00:10.00, start: 0.000000, bitrate: 47 kb/s
  Stream #0:0(und): Video: h264 (High 4:2:0) (avc1 / 0x31637661)
    yuv420p, 640x480, 30 fps, 30 tbr, 15360 tbn, 60 tbc
```

关键信息：
- `Duration`：时长 10 秒
- `Video: h264`：H.264 编码
- `yuv420p`：YUV420P 像素格式
- `640x480`：分辨率
- `30 fps`：帧率

### 6.2 详细 JSON 格式输出

```bash
ffprobe -v quiet -print_format json -show_streams sample.mp4
```

### 6.3 检查 FFmpeg API 调用

在代码里加日志：

```cpp
printf("[Demuxer] 打开文件: %s\n", filename);
printf("[Demuxer] 找到视频流 #%d\n", video_stream_idx_);
printf("[Demuxer] 分辨率: %dx%d\n", 
       video_stream->codecpar->width,
       video_stream->codecpar->height);
```

### 6.4 内存泄漏检查（Linux/macOS）

```bash
# 安装 valgrind
brew install valgrind  # macOS
sudo apt-get install valgrind  # Ubuntu

# 运行检查
valgrind --leak-check=full ./live-player sample.mp4
```

---

## 7. 常见问题

### Q1：CMake 找不到 FFmpeg

错误信息：
```
CMake Error: Could not find FFmpeg
```

解决：
```bash
# macOS: 设置 PKG_CONFIG_PATH
export PKG_CONFIG_PATH="/opt/homebrew/opt/ffmpeg/lib/pkgconfig:$PKG_CONFIG_PATH"

# 然后重新运行 cmake
cmake ..
```

### Q2：运行时崩溃 "Segmentation fault"

可能原因：
1. 视频文件不存在或损坏
2. 视频没有视频流（纯音频文件）
3. 内存越界访问

检查：
```bash
ffprobe sample.mp4  # 确认文件有效
```

### Q3：窗口黑屏，没有画面

可能原因：
1. 像素格式不匹配（期望 YUV420P，实际是其他格式）
2. 纹理更新失败

检查：
```cpp
printf("像素格式: %d\n", frame->format);
// 应该是 0 (AV_PIX_FMT_YUV420P)
```

### Q4：播放速度太快/太慢

我们现在的代码没有精确的音视频同步，只是简单 sleep。

精确的同步需要：
- 根据帧的 PTS（时间戳）计算显示时机
- 对比当前系统时间

这会在后面的章节实现。

---

## 8. 下一步

完成本章后，继续 **第二章：网络播放器**。

**第二章内容预告**：
- 把本地文件换成 RTMP 直播流
- TCP 网络编程基础
- FLV 格式解封装
- 网络缓冲策略（延迟 vs 流畅度）

**代码变更预览**：
```cpp
// 第1章：本地文件
demuxer.Open("local.mp4");

// 第2章：网络流
demuxer.Open("rtmp://live.xxx.com/stream");
```

---

## 参考资料

1. **FFmpeg 文档**：https://ffmpeg.org/documentation.html
2. **SDL2 教程**：https://wiki.libsdl.org/SDL2/Tutorials
3. **H.264 简介**：https://www.vcodex.com/an-overview-of-h264-advanced-video-coding/
4. **YUV 格式详解**：https://en.wikipedia.org/wiki/YUV

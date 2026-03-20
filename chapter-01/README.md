# 第一章：本地播放器

> **目标**：运行 `./live-player video.mp4`，弹出窗口播放视频。

---

## 1. 视频播放到底是怎么回事？

想象你有一个**视频文件**，就像这样：

```
video.mp4 (100MB)
```

你以为里面直接存的是图片吗？**不是**。

如果直接存图片（假设 1920x1080，每秒 30 帧，1 分钟）：
- 每张图片 = 1920 × 1080 × 3 字节 ≈ 6 MB
- 1 秒 = 30 张 = 180 MB
- 1 分钟 = 10.8 GB ❌

但实际上视频只有 100MB，**压缩了 100 倍**！

### 压缩的秘密

视频利用了**时间和空间的冗余**：
- **空间冗余**：一张图里相邻像素颜色相近，可以用数学方法表示
- **时间冗余**：相邻帧内容差不多，只存"变化的部分"

### 两个关键步骤

要把压缩的视频显示出来，需要：

```
MP4文件 → [解封装] → H.264压缩数据 → [解码] → YUV像素 → [渲染] → 屏幕
```

| 步骤 | 做什么 | 类比 |
|-----|-------|------|
| **解封装** | 从 MP4 盒子里取出视频流 | 从 ZIP 里解压出文件 |
| **解码** | 把 H.264 还原成像素 | 解压 ZIP 里的压缩文件 |
| **渲染** | 把像素画到屏幕上 | 用看图软件打开图片 |

---

## 2. 准备工作

### 2.1 安装 FFmpeg

FFmpeg 是音视频开发的事实标准，几乎所有播放器都在用。

**macOS：**
```bash
brew install ffmpeg
```

**Ubuntu：**
```bash
sudo apt-get install ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

**验证安装：**
```bash
ffmpeg -version
# 应该看到版本信息
```

### 2.2 安装 SDL2

SDL2 是跨平台的图形库，帮我们创建窗口和显示图像。

**macOS：**
```bash
brew install sdl2
```

**Ubuntu：**
```bash
sudo apt-get install libsdl2-dev
```

### 2.3 安装 CMake

CMake 是构建工具，生成 Makefile 或 Xcode/VS 项目。

**macOS：**
```bash
brew install cmake
```

**Ubuntu：**
```bash
sudo apt-get install cmake
```

---

## 3. 代码结构

我们的播放器分为 3 个模块：

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                             │
│                    （拼接三个模块）                           │
└──────────────┬────────────────────────────────┬─────────────┘
               │                                │
       ┌───────▼───────┐              ┌─────────▼────────┐
       │    Demuxer    │              │     Decoder      │
       │   解封装器     │              │      解码器       │
       │               │              │                  │
       │  MP4 → 压缩数据 │─────────────▶│ H.264 → YUV像素  │
       │  (AVPacket)   │              │   (AVFrame)      │
       └───────────────┘              └─────────┬────────┘
                                                │
                                       ┌────────▼────────┐
                                       │    Renderer     │
                                       │     渲染器       │
                                       │                 │
                                       │  YUV → 屏幕显示  │
                                       │   (SDL2窗口)    │
                                       └─────────────────┘
```

### 3.1 Demuxer（解封装器）

**作用**：读取视频文件，提取压缩数据。

**关键 FFmpeg 函数**：
- `avformat_open_input()`：打开文件
- `av_read_frame()`：读取一个数据包

**代码示例**：
```cpp
// 打开文件
AVFormatContext* ctx = nullptr;
avformat_open_input(&ctx, "video.mp4", nullptr, nullptr);

// 读取数据包
AVPacket packet;
while (av_read_frame(ctx, &packet) >= 0) {
    // packet.data 就是压缩的视频数据
    // packet.size 是数据大小
}
```

### 3.2 Decoder（解码器）

**作用**：把压缩数据还原成原始像素。

**关键 FFmpeg 函数**：
- `avcodec_find_decoder()`：找到解码器（如 H.264）
- `avcodec_send_packet()`：送压缩数据
- `avcodec_receive_frame()`：取像素帧

**代码示例**：
```cpp
// 找到 H.264 解码器
const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
AVCodecContext* dec_ctx = avcodec_alloc_context3(codec);
avcodec_open2(dec_ctx, codec, nullptr);

// 解码
avcodec_send_packet(dec_ctx, &packet);    // 送压缩数据
AVFrame* frame = av_frame_alloc();
avcodec_receive_frame(dec_ctx, frame);   // 取像素帧

// frame->data[0] 就是 Y 平面（亮度）
// frame->data[1] 就是 U 平面（色度）
// frame->data[2] 就是 V 平面（色度）
```

### 3.3 Renderer（渲染器）

**作用**：把像素显示到屏幕上。

**关键 SDL2 函数**：
- `SDL_CreateWindow()`：创建窗口
- `SDL_UpdateYUVTexture()`：更新 YUV 纹理
- `SDL_RenderPresent()`：显示到屏幕

**代码示例**：
```cpp
// 创建窗口
SDL_Window* window = SDL_CreateWindow("Player", 640, 480, ...);
SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, ...);

// 渲染
SDL_UpdateYUVTexture(texture, ..., y_data, u_data, v_data);
SDL_RenderCopy(renderer, texture, nullptr, nullptr);
SDL_RenderPresent(renderer);
```

---

## 4. 完整代码

### 4.1 main.cpp

```cpp
#include "demuxer.h"
#include "decoder.h"
#include "renderer.h"

int main(int argc, char* argv[]) {
    // 1. 解封装器：打开文件
    Demuxer demuxer;
    demuxer.Open(argv[1]);
    
    // 2. 解码器：初始化解码器
    Decoder decoder;
    decoder.Init(demuxer.video_stream()->codecpar);
    
    // 3. 渲染器：创建窗口
    Renderer renderer;
    renderer.Init(decoder.width(), decoder.height());
    
    // 4. 播放循环
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    
    while (demuxer.ReadPacket(&packet)) {
        decoder.SendPacket(&packet);           // 送数据
        while (decoder.ReceiveFrame(frame)) {  // 取帧
            renderer.RenderFrame(frame);       // 显示
        }
    }
    
    return 0;
}
```

### 4.2 完整代码文件

见 `src/` 目录下的：
- `demuxer.h/cpp`
- `decoder.h/cpp`
- `renderer.h/cpp`

每个文件都有详细注释。

---

## 5. 构建和运行

### 5.1 构建

```bash
mkdir build
cd build
cmake ..
make -j4
```

**cmake 做了什么？**
1. 查找 FFmpeg 库（`libavformat`, `libavcodec`, `libavutil`）
2. 查找 SDL2 库
3. 生成 Makefile
4. make 编译出可执行文件 `live-player`

### 5.2 运行

```bash
# 准备测试视频
curl -L -o sample.mp4 "https://www.sample-videos.com/video321/mp4/720/big_buck_bunny_720p_1mb.mp4"

# 运行
./live-player sample.mp4
```

---

## 6. 调试技巧

### 6.1 查看视频信息

```bash
ffprobe -v quiet -print_format json -show_streams sample.mp4
```

你会看到：
```json
{
  "streams": [
    {
      "codec_name": "h264",
      "width": 1280,
      "height": 720,
      "pix_fmt": "yuv420p"
    }
  ]
}
```

### 6.2 检查 FFmpeg 调用

在代码里加打印：
```cpp
printf("读取 packet: pts=%ld, size=%d\n", packet.pts, packet.size);
printf("解码 frame: %dx%d\n", frame->width, frame->height);
```

### 6.3 内存泄漏检查

```bash
valgrind --leak-check=full ./live-player sample.mp4
```

---

## 7. 课后思考

### 问题 1：为什么需要 YUV？直接用 RGB 不行吗？

**提示**：YUV420P 的 UV 是 1/4 分辨率，RGB 是完整分辨率。

### 问题 2：如果一个 packet 解码不出 frame，正常吗？

**提示**：B 帧需要参考后面的帧才能解码。

### 问题 3：播放 4K 视频卡顿，可能是什么原因？

**提示**：软解 H.264 4K60fps 需要很强的 CPU。

---

## 8. 下一章预告

**第二章：网络播放器**

把 `video.mp4` 换成 `rtmp://live.xxx.com/stream`，播放直播流。

**新增内容**：
- TCP 连接 RTMP 服务器
- FLV 解封装
- 网络缓冲策略（卡顿 vs 延迟）

---

## 参考资料

- FFmpeg 官方文档：https://ffmpeg.org/documentation.html
- SDL2 官方教程：https://wiki.libsdl.org/SDL2/Tutorials
- H.264 白皮书：https://www.itu.int/rec/T-REC-H.264

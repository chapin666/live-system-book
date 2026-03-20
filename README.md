# 《直播连麦系统：从零到一》

> 一本手把手教你从零构建直播系统的实战教程

## 这本书适合谁？

- **有 C++ 基础**，但对音视频一无所知
- **看过 FFmpeg 命令**，但不知道怎么写代码调用
- **想理解直播原理**，不只是调 API

## 不需要提前了解

- 不需要懂音视频编解码
- 不需要用过 FFmpeg
- 不需要有直播开发经验

## 学习方法

每一章都是**完整的可运行代码**，不是伪代码：

```
第1章：本地播放器（你现在在这里）
    ↓ 增加网络模块
第2章：网络播放器
    ↓ 增加硬件解码
第3章：硬解播放器
    ↓ 增加采集编码
第4章：直播推流
    ...
```

**每章结构**：
1. **为什么需要这个功能？**（场景驱动）
2. **原理讲解**（类比+图示）
3. **代码实现**（完整可运行）
4. **调试技巧**（排查问题）
5. **课后思考**（加深理解）

---

## 第一章：本地播放器

### 本章目标

运行 `./live-player video.mp4`，弹出一个窗口播放视频。

通过这个过程，你会理解：
- 视频文件里到底存了什么？
- 为什么需要"解封装"和"解码"两步？
- YUV 和 RGB 有什么区别？

### 预计时间

- 阅读：30 分钟
- 动手：20 分钟

---

## 快速开始

### 1. 安装依赖

**macOS：**
```bash
brew install ffmpeg sdl2 cmake
```

**Ubuntu/Debian：**
```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libsdl2-dev cmake
```

**Windows：**
建议使用 WSL2，然后按 Ubuntu 步骤安装。

### 2. 下载代码

```bash
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-01
```

### 3. 构建

```bash
mkdir build
cd build
cmake ..
make -j4
```

**构建成功你会看到：**
```
[100%] Built target live-player
```

### 4. 准备测试视频

```bash
# 下载一个测试视频（或者用你自己的）
curl -L -o sample.mp4 "https://www.sample-videos.com/video321/mp4/720/big_buck_bunny_720p_1mb.mp4"

# 或者生成一个测试视频
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -pix_fmt yuv420p sample.mp4
```

### 5. 运行

```bash
./live-player sample.mp4
```

**成功现象：**
- 弹出一个窗口
- 视频开始播放
- 窗口标题显示 "Live Player - Chapter 01"
- 按 ESC 或点击关闭按钮退出

---

## 如果运行失败

### 错误1：找不到 FFmpeg
```
CMake Error: Could not find FFmpeg
```
**解决：** 确认 FFmpeg 已安装，并且 `pkg-config` 能找到：
```bash
pkg-config --exists libavformat && echo "FFmpeg OK"
```

### 错误2：找不到 SDL2
```
CMake Error: Could not find SDL2
```
**解决：** macOS 上可能需要设置环境变量：
```bash
export PKG_CONFIG_PATH="/opt/homebrew/opt/sdl2/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### 错误3：运行时崩溃
```
Segmentation fault
```
**解决：** 确认 sample.mp4 存在且格式正确：
```bash
ffprobe sample.mp4  # 应该能看到视频流信息
```

---

## 本章知识地图

学完本章，你会理解这个链路：

```
[MP4文件] → 解封装 → [H.264数据] → 解码 → [YUV图像] → 渲染 → [屏幕显示]
     ↑              ↑              ↑              ↑
  Demuxer       AVPacket      AVFrame       SDL2窗口
  解封装器       压缩数据包     原始像素       显示
```

### 关键概念（先混个脸熟）

| 概念 | 一句话解释 | 类比 |
|-----|-----------|------|
| **容器(MP4)** | 装视频/音频/字幕的盒子 | ZIP 文件里装了多个文件 |
| **解封装** | 把盒子打开，取出视频流 | 解压 ZIP |
| **编解码(H.264)** | 压缩/解压视频的算法 | 视频版的 ZIP 压缩 |
| **YUV** | 一种像素格式 | 不同于 RGB 的另一种颜色表示 |
| **FFmpeg** | 处理音视频的工具库 | 音视频界的瑞士军刀 |

---

## 目录结构

```
chapter-01/
├── CMakeLists.txt          # 构建配置（告诉 cmake 怎么编译）
├── README.md               # 本章教程（你现在读的）
└── src/
    ├── main.cpp            # 程序入口：拼接各个模块
    ├── demuxer.h/cpp       # 解封装：读文件 → 取压缩数据
    ├── decoder.h/cpp       # 解码：H.264 → YUV
    └── renderer.h/cpp      # 渲染：YUV → 屏幕显示
```

---

## 下一步

完成本章后，继续 [第二章：网络播放器](../chapter-02/README.md)，学习如何从网络拉流播放。

---

## LICENSE

MIT License - 你可以自由使用、修改、分发代码。

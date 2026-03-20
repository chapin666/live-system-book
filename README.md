# 《直播连麦系统：从零到一》

> 一本手把手教你从零构建直播系统的实战教程

## 这本书讲什么？

从播放一个本地视频开始，一步步构建出完整的直播连麦系统。

采用 **Pipeline 架构**，每一章在前一章基础上演进，代码始终可运行。

## 适合谁？

- 有 C++ 基础，但对音视频开发不熟悉
- 想系统学习直播技术，不只是零散搜博客

不需要：音视频编解码基础、直播开发经验

## 学习方式

渐进式实战：

```
第1章：Pipeline 架构 + 本地播放
第2章：异步化改造 + 网络播放（TODO）
第3章：硬件解码（TODO）
...
```

## 技术栈

- C++14
- FFmpeg 4.0+
- SDL2
- CMake

## 快速开始

```bash
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-01

# macOS
brew install ffmpeg sdl2 cmake

# 构建
mkdir build && cd build
cmake .. && make -j4

# 运行
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -pix_fmt yuv420p sample.mp4
./live-player sample.mp4
```

详细教程见 [chapter-01/README.md](chapter-01/)。

## 许可证

MIT

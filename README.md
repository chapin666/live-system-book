# 《直播连麦系统：从零到一》

> 一本手把手教你从零构建直播系统的实战教程

---

## 这本书讲什么？

从播放一个本地视频开始，一步步构建出完整的直播连麦系统。

**不是**罗列 API 的说明书，**而是**带你经历真实的开发过程：
- 为什么需要这个功能？
- 几种方案怎么选？
- 遇到问题怎么调试？
- 如何优化性能和稳定性？

---

## 适合谁？

**适合：**
- 有 C++ 基础，但对音视频开发不熟悉
- 用过 FFmpeg 命令行，想深入了解原理
- 想系统学习直播技术，而不是零散搜博客

**不需要：**
- 不需要懂音视频编解码原理（书中会讲）
- 不需要直播开发经验（从零开始）

**前置知识：**
- C++ 基础语法（类、指针、STL）
- 命令行和 git 基本使用

---

## 学习方式

本书采用**渐进式实战**：

每一章在前一章基础上增加功能，代码始终可运行。比如：
- 第1章能播放本地文件
- 第2章改成播放网络流（其他代码不动）
- 第3章加入硬件解码（其他代码不动）

最终你会得到一个完整的生产级系统，而不是一堆孤立 demo。

---

## 技术栈

- **语言**：C++14
- **核心库**：FFmpeg、SDL2
- **构建工具**：CMake
- **支持平台**：macOS、Linux

---

## 快速开始

```bash
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-01

# 安装依赖（macOS 示例）
brew install ffmpeg sdl2 cmake

# 构建
mkdir build && cd build
cmake .. && make -j4

# 生成测试视频并播放
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -pix_fmt yuv420p sample.mp4
./live-player sample.mp4
```

详细教程见各章目录下的 README.md。

---

## 许可证

MIT License

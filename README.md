# 📺 直播连麦系统：从零到一

> **一本让你从 C++ 开发者成长为音视频工程师的实战教程**  
> **目标平台**：Linux / macOS（仅 POSIX API，不支持 Windows）

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++14](https://img.shields.io/badge/C++-14-blue.svg)](https://isocpp.org/)
[![FFmpeg](https://img.shields.io/badge/FFmpeg-4.0+-green.svg)](https://ffmpeg.org/)

---

## 🚀 快速开始

```bash
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-02
mkdir build && cd build && cmake .. && make -j4

ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 -pix_fmt yuv420p test.mp4
./player test.mp4
```

**环境准备**：`brew install ffmpeg sdl2 cmake` (macOS) 或 `apt-get install ffmpeg libavformat-dev libsdl2-dev cmake` (Ubuntu)

---

## 📖 本书介绍

**渐进式案例**：「小直播」从 100 行代码逐步成长为生产级系统

**单章产出**：每章结束都有可运行的代码，不是纯理论

**工业级标准**：RAII、智能指针、详细错误处理、内置统计

---

## 📊 内容概览

### Part 1：播放器基础 ✅ 已完成

| 章节 | 内容 | 代码 |
|:---:|:---|---:|
| Ch1 | 视频基础理论 | 1491 |
| Ch2 | [第一个播放器](chapter-02/) | 118 |
| Ch3 | [Pipeline 工程化](chapter-03/) | 382 |
| Ch4 | [性能分析：为什么卡顿？](chapter-04/) | 92 |
| Ch5 | [C++11 多线程](chapter-05/) | 820 |
| Ch6 | [异步多线程播放器](chapter-06/) | 453 |
| Ch7 | [网络基础](chapter-07/) | 574 |
| Ch8 | [直播 vs 点播](chapter-08/) | 968 |
| Ch9 | [硬件解码优化](chapter-09/) | 391 |
| Ch10 | [音视频采集](chapter-10/) | 565 |
| Ch11 | [音频 3A 处理](chapter-11/) | 181 |
| Ch12 | [编码与推流](chapter-12/) | 259 |

**Part 1 项目实战**：
- [项目1：完整本地播放器](project-01/) — 播放控制、FPS显示
- [项目2：网络点播播放器](project-02/) — HTTP播放、缓冲重连
- [项目3：直播观众端](project-03/) — RTMP拉流、延迟优化

### Part 2-5：计划中

| Part | 内容 | 状态 |
|:---|:---|:---:|
| Part 2 | 主播端进阶（编码、高级采集） | 📝 |
| Part 3 | 实时连麦（WebRTC、SFU） | 📝 |
| Part 4 | 服务端架构（MCU、录制） | 📝 |
| Part 5 | 生产部署（Docker、K8s） | 📝 |

---

## 📈 Part 1 统计

- **教程**：1.3万行 / 30万字
- **代码**：6767 行 C++
- **章节**：12 章 + 3 个项目

---

## 🎯 学习路径

```
Ch1-3: 本地播放基础 → 项目1
    ↓
Ch4-6: 异步优化 → 项目2
    ↓
Ch7-9: 网络播放 → 项目3
    ↓
Ch10-12: 采集编码推流 → 具备主播端基础
```

**预计时间**：Part 1 需 2-3 个月

---

## 🗺️ 完整大纲

📄 [OUTLINE.md](OUTLINE.md) — 查看全部 30 章详细规划

---

## 💡 核心概念速查

| 概念 | 一句话解释 | 类比 |
|:---|:---|:---|
| Pipeline | 数据像水一样流动 | 工厂流水线 |
| PTS | 视频帧的"闹钟" | 告诉系统该何时显示 |
| YUV | 亮度与颜色分开存 | 黑白电视 + 调色板 |
| SFU | 选择性转发 | 快递分拣中心 |
| MCU | 混音混画 | 视频编辑合成 |

---

## 🛠️ 技术栈

- **C++14** — 现代 C++，RAII、智能指针
- **FFmpeg** — 音视频处理行业标准
- **SDL2** — 跨平台渲染
- **CMake** — 构建系统

---

## 📄 许可证

MIT License — 可自由用于学习或商业项目

**🌟 如果对你有帮助，请点个 Star！**

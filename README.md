# 📺 直播连麦系统：从零到一

> **一本让你从 C++ 开发者成长为音视频工程师的实战教程**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++14](https://img.shields.io/badge/C++-14-blue.svg)](https://isocpp.org/)
[![FFmpeg](https://img.shields.io/badge/FFmpeg-4.0+-green.svg)](https://ffmpeg.org/)

---

## 🎯 这本书能给你什么？

| 能力 | 学完章节 | 成果 |
|:---|:---|:---|
| 🔧 开发播放器 | 第 1-4 章 | 流畅播放 RTMP 直播流 |
| 📡 搭建直播系统 | 第 5-7 章 | 采集、编码、推流、硬件解码 |
| 👥 实现连麦互动 | 第 8-11 章 | 多人音视频实时通话 |
| 🏗️ 设计服务端架构 | 第 12-14 章 | 支持万人同时观看 |
| 🚀 上线生产环境 | 第 15-17 章 | 完整的监控、安全、部署体系 |

---

## 📚 核心特色

**单案例渐进式**：「小直播」从第 1 章到第 17 章不断进化

```
📺 小直播演进路线图

┌─────────────────────────────────────────────────────────────┐
│  🔷 第一阶段：观众端（第 1-4 章）                              │
│     Ch1 Pipeline → Ch2 异步架构 → Ch3 HTTP网络 → Ch4 RTMP拉流  │
│     成果：完整播放器，支持本地/网络/直播流播放                    │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  🔷 第二阶段：主播端（第 5-6 章）                              │
│     Ch5 采集(3A) → Ch6 编码推流                               │
│     成果：完整直播工具，采集→编码→推流闭环                      │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  🔷 第三阶段：优化（第 7-8 章）                                │
│     Ch7 硬件解码 → Ch8 首屏优化                               │
│     成果：4K流畅播放，秒开体验                                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
                    后续：连麦、服务端、运维...
```

每一章在前一章**代码基础上增量开发**，不是孤立的 Demo。

**工业级代码标准**：
- RAII 智能指针，异常安全
- 详细错误码，不是简单返回 false
- 内置统计、日志、事件回调

**章节状态**：✅ 已完成 | 📝 待编写 | 🔄 更新中

**深入浅出讲解**：

| 概念 | 一句话解释 | 类比 |
|:---|:---|:---|
| Pipeline | 数据像水一样流动 | 工厂流水线 |
| PTS | 视频帧的"闹钟" | 告诉系统这帧该什么时候显示 |
| YUV | 把亮度和颜色分开存 | 黑白电视 + 彩色调色板 |
| SFU | 选择性转发 | 快递分拣中心 |
| MCU | 混音混画 | 视频编辑软件实时合成 |

---

## 🗺️ 学习路线图

### 17 章完整大纲

| 章节 | 主题 | 核心产出 | 阶段 | 状态 |
|:---:|:---|:---|:---|:---:|
| [01](chapter-01/) | Pipeline 架构与本地播放 | 本地播放器 | 播放端 | ✅ |
| [02](chapter-02/) | 异步化改造 | 流畅播放器 | 播放端 | ✅ |
| [03](chapter-03/) | 网络基础 | HTTP 网络播放器 | 播放端 | ✅ |
| [04](chapter-04/) | RTMP 流媒体协议 | RTMP 拉流播放器 | 播放端 | ✅ |
| [05](chapter-05/) | 采集与音频 3A 处理 | 摄像头+麦克风采集 | 主播端 | ✅ |
| [06](chapter-06/) | 视频编码与 RTMP 推流 | 直播推流工具 | 主播端 | ✅ |
| [07](chapter-07/) | 硬件解码与 4K 播放 | 4K 硬件解码播放器 | 优化 | ✅ |
| 08 | 首屏优化与低延迟 | 秒开直播 | 优化 | 📝 |
| 09 | 实时传输基础 | UDP 互通 | 连麦 | 📝 |
| 10 | 信令服务器与 ICE | P2P 连接 | 连麦 | 📝 |
| 11 | WebRTC P2P 连麦 | 1v1 连麦 | 连麦 | 📝 |
| 12 | 多人房间与 SFU | 10 人房间 | 服务端 | 📝 |
| 13 | MCU 混音混画 | 100 人房间 | 服务端 | 📝 |
| 14 | 录制与时移回放 | 回放系统 | 服务端 | 📝 |
| 15 | 质量监控 | 可观测大盘 | 运维 | 📝 |
| 16 | 生产部署 | K8s 上线 | 运维 | 📝 |
| 17 | 直播安全与风控 | 安全防护 | 运维 | 📝 |

---

## 🚀 快速开始

### 环境准备

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

### 构建运行（第 1 章）

第 1 章包含完整播放器代码、详细的 FFmpeg API 讲解、性能优化指南和调试技巧：
- 📄 100 行核心播放器代码 (`simple_player`)
- 📄 Pipeline 工程化版本 (`live-player`)
- 📊 3 个 SVG 架构图
- 🔧 完整错误处理方案
- 🚀 硬件解码配置（VideoToolbox/VAAPI/NVDEC）
- 🐛 GDB/Valgrind/Perf 调试实战

```bash
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-01

mkdir build && cd build
cmake .. && make -j4

# 生成测试视频
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 \
       -pix_fmt yuv420p sample.mp4

# 运行简单版本（100行核心代码）
./simple_player sample.mp4

# 或运行 Pipeline 版本（工程化实现）
./live-player sample.mp4
```

**两个版本的区别**：

| 版本 | 代码量 | 特点 | 适合 |
|:---|:---:|:---|:---|
| `simple_player` | ~180 行 | 单文件，直接易懂 | 初学者理解原理 |
| `live_player` | ~500 行 | 模块化，接口抽象 | 学习工程化设计 |

**预期效果**：弹出窗口播放彩色条纹，按 ESC 退出。

---

### 构建运行（第 3-7 章）

第 3-7 章采用统一的代码风格和目录结构：

```bash
# 第 3 章：HTTP 网络播放器
cd chapter-03 && mkdir build && cd build
cmake .. && make
./network-player http://example.com/video.mp4

# 第 4 章：RTMP 拉流播放器
cd chapter-04 && mkdir build && cd build
cmake .. && make
./simple_rtmp_player rtmp://localhost/live/stream

# 第 5 章：音视频采集
cd chapter-05 && mkdir build && cd build
cmake .. && make
./video_capture_demo          # 摄像头预览
./audio_capture_demo out.wav  # 录制音频
./av_capture_demo             # 同步采集

# 第 6 章：RTMP 推流
cd chapter-06 && mkdir build && cd build
cmake .. && make
./simple_streamer rtmp://localhost/live/stream  # 推送测试彩条

# 第 7 章：硬件解码播放器
cd chapter-07 && mkdir build && cd build
cmake .. && make
./hw_decode_player video.mp4        # 自动检测硬件解码
./decode_bench video.mp4            # 性能对比测试
```

---

## 🎓 学习建议

1. **不要跳章节** —— 每章依赖前一章代码
2. **一定要动手** —— 自己敲代码，不要复制
3. **善用调试工具** —— `ffprobe`、`valgrind`、`perf`
4. **遇到问题查 FAQ** —— 每章末尾有常见问题

---

## 🛠️ 技术栈

- **C++14** — 现代 C++，RAII、智能指针
- **FFmpeg 4.0+** — 音视频处理行业标准
- **SDL2** — 跨平台窗口和渲染
- **CMake** — 构建系统
- **WebRTC** — 实时通信（第 9-11 章）
- **K8s** — 部署（第 16 章）

---

## 📄 许可证

[MIT License](LICENSE) — 可自由用于学习或商业项目。

---

**🌟 如果对你有帮助，请点个 Star！**

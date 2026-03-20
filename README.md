# 《直播连麦系统：从零到一》

> 一本手把手教你从零构建直播系统的实战教程

---

## 这本书适合谁？

**适合：**
- 有 C++ 基础，但对音视频一无所知
- 用过 FFmpeg 命令行，但不知道怎么写代码调用
- 想深入理解直播原理，不只是调 API

**不需要：**
- 不需要懂音视频编解码
- 不需要用过 FFmpeg
- 不需要有直播开发经验

**前置知识：**
- 熟悉 C++ 基础语法（类、指针、STL）
- 了解基本的网络概念（TCP、HTTP）
- 会用命令行和 git

---

## 本书特点

### 1. 渐进式实战

不是一下子给你一堆代码，而是**一步一步迭代**：

```
第1章：本地播放器
    能播放本地 MP4 文件
    理解：解封装 → 解码 → 渲染
    
第2章：网络播放器
    能播放 RTMP 直播流
    新增：网络模块、TCP连接
    
第3章：硬件解码
    播放 4K 不卡顿
    新增：VideoToolbox、MediaCodec
    
第4章：直播推流
    自己开播
    新增：采集、编码、推流
    
...直到第10章完整的生产级系统
```

### 2. 详细讲解

每个概念都解释清楚：

**不说**："RGB 是 3 字节"
**而是**：
> RGB 用 3 个字节表示一个像素：
> - R（红色）：1 字节，范围 0-255
> - G（绿色）：1 字节，范围 0-255  
> - B（蓝色）：1 字节，范围 0-255
> 
> 为什么是 0-255？因为 1 字节 = 8 位（bit），2^8 = 256，可以表示 256 种强度。

### 3. 完整代码

每一章的代码都是：
- **可运行的**：不是伪代码，是完整程序
- **有注释的**：关键行都有解释
- **有结构的**：模块化设计，不是一坨代码

---

## 目录

| 章节 | 内容 | 新增模块 |
|-----|------|---------|
| [第1章](chapter-01/) | 本地播放器 | Demuxer、Decoder、Renderer |
| [第2章](chapter-02/) | 网络播放器 | RTMPClient（TODO） |
| [第3章](chapter-03/) | 硬件解码 | HWDecoder（TODO） |
| [第4章](chapter-04/) | 直播推流 | Capture、Encoder（TODO） |
| [第5章](chapter-05/) | 连麦互动 | WebRTC P2P（TODO） |
| [第6章](chapter-06/) | 多人连麦 | SFU Server（TODO） |
| [第7章](chapter-07/) | 美颜特效 | GPU Filter、AI（TODO） |
| [第8章](chapter-08/) | 录制回放 | DVR、HLS（TODO） |
| [第9章](chapter-09/) | 监控压测 | Telemetry（TODO） |
| [第10章](chapter-10/) | 生产部署 | K8s、Docker（TODO） |

---

## 快速开始

### 第1章：本地播放器

**预计时间**：阅读 40 分钟，动手 20 分钟。

**目标**：运行 `./live-player video.mp4`，弹出窗口播放视频。

```bash
# 1. 克隆代码
git clone https://github.com/chapin666/live-system-book.git
cd live-system-book/chapter-01

# 2. 安装依赖（macOS）
brew install ffmpeg sdl2 cmake

# 3. 构建
mkdir build && cd build
cmake .. && make -j4

# 4. 准备测试视频
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 \
       -pix_fmt yuv420p sample.mp4

# 5. 运行
./live-player sample.mp4
```

详细教程见 [chapter-01/README.md](chapter-01/)。

---

## 项目结构

```
live-system-book/
├── README.md              # 本文件（项目总览）
├── chapter-01/
│   ├── README.md          # 第1章详细教程
│   ├── CMakeLists.txt     # 构建配置
│   └── src/               # 源代码
│       ├── main.cpp
│       ├── demuxer.h/cpp
│       ├── decoder.h/cpp
│       └── renderer.h/cpp
├── chapter-02/            # （TODO）
...
```

---

## 学习建议

1. **不要跳过第1章**
   
   第1章是所有后续章节的基础。Demuxer、Decoder、Renderer 三个模块会一直用到第10章。

2. **一定要动手跑代码**
   
   看完一章后，自己构建运行一遍。改改参数，看看会发生什么。

3. **遇到报错先看 FAQs**
   
   每章都有常见问题解答，90% 的问题都有答案。

4. **不理解的地方多查资料**
   
   每章末尾都有参考资料链接。FFmpeg 官方文档、维基百科都是很好的补充。

---

## 技术栈

- **语言**：C++14
- **核心库**：FFmpeg 4.0+、SDL2
- **构建工具**：CMake 3.10+
- **支持平台**：macOS、Linux、Windows（WSL2）

---

## 许可证

MIT License

你可以自由使用、修改、分发本书代码，用于个人学习或商业项目。

---

## 问题反馈

如果在学习过程中遇到问题：

1. 先检查该章节的"常见问题"部分
2. 确认依赖版本是否符合要求
3. 在 GitHub Issues 提问

---

**准备好开始了吗？** 进入 [第1章：本地播放器](chapter-01/)。

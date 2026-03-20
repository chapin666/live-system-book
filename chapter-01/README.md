# 第一章：Pipeline 架构与本地播放

**目标**：理解音视频播放的核心 Pipeline，写出工业级代码。

**预计时间**：阅读 60 分钟，动手 30 分钟。

---

## 📋 目录

1. [为什么需要架构？](#1-为什么需要架构)
2. [Pipeline 设计思想](#2-pipeline-设计思想)
3. [关键技术概念](#3-关键技术概念)
4. [代码实现详解](#4-代码实现详解)
5. [构建与测试](#5-构建与测试)
6. [问题排查](#6-问题排查)
7. [架构演进预告](#7-架构演进预告)

---

## 1. 为什么需要架构？

### 1.1 从一个问题开始

假设老板让你写个播放器，第一版可能是这样：

```cpp
int main() {
    AVFormatContext* ctx = nullptr;
    avformat_open_input(&ctx, "video.mp4", nullptr, nullptr);
    
    AVPacket packet;
    while (av_read_frame(ctx, &packet) >= 0) {
        // 解码... 渲染...
    }
    return 0;
}
```

这能跑，但**不能维护**：
- 所有逻辑堆在 main 里
- 没有错误处理
- 内存泄漏（packet 没释放）
- 无法测试

### 1.2 工业级代码的要求

| 要求 | 为什么重要 | 本章解决方案 |
|:---|:---|:---|
| **不泄漏内存** | 播放器要跑几小时 | RAII 智能指针 |
| **可测试** | 改代码后怎么保证没坏？ | 接口抽象 |
| **可观测** | 线上出问题怎么排查？ | 统计接口 |
| **可扩展** | 后面要加功能怎么办？ | Pipeline 架构 |

---

## 2. Pipeline 设计思想

### 2.1 什么是 Pipeline？

> **数据像水一样流动，每个阶段处理完传给下一个阶段。**

```
视频文件 → [Demuxer] → [Decoder] → [Renderer] → 屏幕
            解封装      解码        渲染
```

**比喻**：
- 视频文件 = 原料
- Demuxer = 拆解工（把盒子拆开）
- Decoder = 加工工（把压缩数据还原）
- Renderer = 装配工（显示到屏幕）

### 2.2 为什么要抽象 Pipeline 接口？

想象你要换一辆车：
- 没有接口：重新学开车（方向盘位置变了）
- 有接口：只要会开车，换什么车都能开

**代码体现**：

```cpp
// 定义接口（本章实现 SimplePipeline，后续换实现但不换接口）
class Pipeline {
public:
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
};

// 使用接口（用户代码不需要知道具体实现）
std::unique_ptr<Pipeline> player = std::make_unique<SimplePipeline>();
player->Init("video.mp4");
player->Start();
```

**好处**：
- 第2章换成网络流，用户代码不用改
- 可以 Mock 接口做测试
- 团队分工：有人写 Demuxer，有人写 Decoder，通过接口对接

### 2.3 我们的 Pipeline 架构

```
┌─────────────────────────────────────────┐
│           SimplePipeline                │
│  ┌─────────────────────────────────┐    │
│  │         核心模块                 │    │
│  │  Demuxer → Decoder → Renderer   │    │
│  └─────────────────────────────────┘    │
│  ┌─────────────────────────────────┐    │
│  │         基础设施                 │    │
│  │  Observer + Stats + ErrorCode   │    │
│  └─────────────────────────────────┘    │
└─────────────────────────────────────────┘
```

---

## 3. 关键技术概念

### 3.1 视频压缩原理

**为什么不压缩的视频那么大？**

| 参数 | 数值 | 计算结果 |
|:---|:---|:---|
| 分辨率 | 1920 × 1080 | 全高清 1080p |
| 每像素 | 3 字节 (RGB) | - |
| 每帧大小 | 1920 × 1080 × 3 | **6.2 MB** |
| 帧率 | 30 fps | - |
| 每秒大小 | 6.2 MB × 30 | **186 MB** |
| 1分钟大小 | 186 MB × 60 | **10.8 GB** |
| **压缩后** | - | **~100 MB** |
| **压缩率** | - | **100 倍** |

**压缩原理**：

| 冗余类型 | 原理 | 效果 |
|:---|:---|:---|
| **空间冗余** | 天空区域几万像素类似，只存一个值+范围 | 省 50% |
| **时间冗余** | 连续帧变化小，只存变化部分 | 省 90% |

### 3.2 视频播放的数据流

| 步骤 | 阶段 | 输入 | 输出 | 核心操作 |
|:---|:---|:---|:---|:---|
| 1 | 解封装 | MP4 文件 | H.264 数据包 | 从容器提取视频流 |
| 2 | 解码 | H.264 压缩数据 | YUV 像素帧 | 解压缩还原图像 |
| 3 | 渲染 | YUV 像素 | 屏幕显示 | 颜色转换+绘制 |

### 3.3 YUV 像素格式

**为什么用 YUV 而不是 RGB？**

**内存布局对比（1920×1080）：**

| 平面 | 分辨率 | 大小 | 说明 |
|:---|:---|:---|:---|
| Y | 1920 × 1080 | 2,073,600 字节 | 亮度，每个像素1字节 |
| U | 960 × 540 | 518,400 字节 | 色度，1/4分辨率 |
| V | 960 × 540 | 518,400 字节 | 色度，1/4分辨率 |
| **总计** | - | **3,110,400 字节** | 比 RGB 省 50% |

**原理**：人眼对亮度敏感，对色度不敏感，所以色度可以压缩。

### 3.4 RAII 内存管理

**为什么不用裸指针？**

```cpp
// ❌ 裸指针：容易泄漏
void Bad() {
    AVPacket* pkt = av_packet_alloc();
    if (error) return;  // 泄漏！pkt 没释放
    av_packet_free(&pkt);
}

// ✅ 智能指针：自动释放
void Good() {
    PacketPtr pkt = MakePacket();  // RAII 包装
    if (error) return;  // 安全！自动释放
}  // 这里也自动释放
```

---

## 4. 代码实现详解

### 4.1 项目结构

```
chapter-01/
├── CMakeLists.txt
├── README.md
└── src/
    ├── base/               # 基础组件
    │   ├── pipeline.h      # Pipeline 接口
    │   └── ffmpeg_utils.h  # FFmpeg RAII 封装
    ├── core/               # 核心实现
    │   ├── simple_pipeline.h/cpp   # 主 Pipeline
    │   ├── demuxer.h/cpp           # 解封装模块
    │   ├── decoder.h/cpp           # 解码模块
    │   └── renderer.h/cpp          # 渲染模块
    └── main.cpp            # 示例程序
```

### 4.2 接口层设计

**Pipeline 接口定义**（`base/pipeline.h`）：

```cpp
class Pipeline {
public:
    virtual ~Pipeline() = default;
    
    // 生命周期
    virtual ErrorCode Init(const std::string& url) = 0;
    virtual ErrorCode Start() = 0;
    virtual ErrorCode Stop() = 0;
    
    // 可观测性
    virtual PipelineStats GetStats() const = 0;
    virtual void SetObserver(PipelineObserver* observer) = 0;
};
```

### 4.3 核心模块协作

**初始化流程：**

| 顺序 | 调用者 | 被调用者 | 操作 |
|:---|:---|:---|:---|
| 1 | main | SimplePipeline | Init(url) |
| 2 | SimplePipeline | Demuxer | Init(url) |
| 3 | SimplePipeline | Decoder | Init(codecpar) |
| 4 | SimplePipeline | Renderer | Init(width, height) |

**播放循环：**

```
while (running) {
    packet = demuxer.ReadPacket()      // 读取压缩数据
    frame = decoder.SendReceive(packet) // 解码为像素
    renderer.Render(frame)              // 显示到屏幕
}
```

---

## 5. 构建与测试

### 5.1 构建步骤

```bash
mkdir build && cd build
cmake ..
make -j4
```

### 5.2 运行

```bash
# 生成测试视频
ffmpeg -f lavfi -i testsrc=duration=10:size=640x480:rate=30 \
       -pix_fmt yuv420p sample.mp4

# 运行
./live-player sample.mp4
```

**预期效果**：
- 终端输出初始化日志
- 窗口弹出，播放彩色条纹
- 按 ESC 退出

---

## 6. 问题排查

| 问题 | 现象 | 解决 |
|:---|:---|:---|
| CMake 找不到 FFmpeg | `Could not find FFmpeg` | `pkg-config --exists libavformat` |
| 运行时崩溃 | `Segmentation fault` | `ffprobe sample.mp4` 检查视频 |
| 窗口黑屏 | 有窗口无画面 | 检查像素格式是否为 YUV420P |

---

## 7. 架构演进预告

本章是**同步单线程**实现，下一章将演进为**异步多线程**：

```
第1章（当前）:  Demuxer → Decoder → Renderer
                单线程顺序执行
                解码慢会卡住渲染

第2章（预告）:  Demuxer → [队列] → Decoder线程 → [队列] → Renderer线程
                多线程并行
                解码渲染不互相阻塞
```

**第2章目标**：解决播放 4K 视频时的卡顿问题。

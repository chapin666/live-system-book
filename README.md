# 直播连麦系统：从零到一

一本渐进式音视频开发实战教程。

## 项目结构

```
live-system-book/
├── chapter-01/          # 本地播放器
├── chapter-02/          # 网络播放器 (TODO)
├── chapter-03/          # 硬件解码 (TODO)
├── chapter-04/          # 直播推流 (TODO)
├── chapter-05/          # 连麦互动 (TODO)
├── chapter-06/          # 多人连麦 (TODO)
├── chapter-07/          # 美颜特效 (TODO)
├── chapter-08/          # 录制回放 (TODO)
├── chapter-09/          # 监控压测 (TODO)
└── chapter-10/          # 生产部署 (TODO)
```

## 第一章：本地播放器

### 构建

```bash
cd chapter-01
mkdir build && cd build
cmake ..
make -j4
```

### 运行

```bash
./live-player ../assets/sample.mp4
```

### 依赖

- FFmpeg 4.0+
- SDL2

### 安装依赖 (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y ffmpeg libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libsdl2-dev
```

### 安装依赖 (macOS)

```bash
brew install ffmpeg sdl2
```

## 许可证

MIT

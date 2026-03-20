# Chapter 01: 本地播放器

## 本章目标

理解视频播放的完整链路：**解封装 → 解码 → 渲染**

## 架构图

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  MP4/AVI    │     │  AVPacket   │     │   AVFrame   │     │   屏幕显示   │
│  文件       │ ──▶ │  (压缩数据)  │ ──▶ │  (YUV420P)  │ ──▶ │   (RGB)     │
│             │     │             │     │             │     │             │
│ [ftyp/moov] │     │ [H.264 NAL] │     │ [Y/U/V平面] │     │ [SDL纹理]   │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
       │                   │                   │                   │
       ▼                   ▼                   ▼                   ▼
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Demuxer    │     │   Decoder   │     │  Renderer   │     │   Window    │
│  解封装器    │     │   解码器     │     │   渲染器     │     │    窗口     │
│             │     │             │     │             │     │             │
│ avformat_   │     │ avcodec_    │     │ SDL_Update  │     │ SDL_        │
│ open_input  │     │ send_packet │     │ YUVTexture  │     │ RenderPresent│
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

## 核心概念

### 1. 容器格式 (Container)

视频文件是一个"包裹"，里面可能有：
- 视频流（H.264/H.265/VP9）
- 音频流（AAC/MP3/Opus）
- 字幕流
- 元数据（标题、时长等）

常见容器：MP4、FLV、MKV、AVI

### 2. 编码格式 (Codec)

原始视频数据太大，需要压缩：
- **H.264/AVC**: 最通用的格式，兼容性最好
- **H.265/HEVC**: H.264 的继任者，同等质量省 50% 码率
- **VP9**: Google 的格式，YouTube 在用

压缩原理：**帧间预测**（只存变化的部分）

### 3. 像素格式 (Pixel Format)

- **RGB**: 红绿蓝三通道，适合屏幕显示
- **YUV**: 亮度(Y) + 色度(UV)，适合压缩
  - YUV420P: UV 分辨率只有 Y 的 1/4，省空间

为什么用 YUV？
> 人眼对亮度敏感，对颜色不敏感。YUV 可以对颜色降采样，省 50% 空间。

## 代码结构

```
src/
├── main.cpp      # 程序入口，播放循环
├── demuxer.h/cpp # 解封装：文件 → 压缩数据包
├── decoder.h/cpp # 解码：压缩数据 → YUV帧
└── renderer.h/cpp # 渲染：YUV → 屏幕
```

## 构建

```bash
mkdir build && cd build
cmake ..
make -j4
```

## 运行

```bash
./live-player ../assets/sample.mp4
```

## 调试技巧

### 查看文件信息
```bash
ffprobe -v quiet -print_format json -show_streams sample.mp4
```

### 解码为原始 YUV
```bash
ffmpeg -i sample.mp4 -c:v rawvideo -pix_fmt yuv420p output.yuv
# 播放
ffplay -f rawvideo -pix_fmt yuv420p -s 1920x1080 output.yuv
```

### 性能分析
```bash
# macOS
instruments -t Time\ Profiler ./live-player sample.mp4

# Linux
perf record ./live-player sample.mp4
perf report
```

## 课后思考

1. **为什么解封装和解码要分开？**
   - 思考：MP4 和 H.264 是什么关系？

2. **YUV420P 的 UV 为什么是 1/4 分辨率？**
   - 计算：1920x1080 的 YUV420P 一帧多少字节？

3. **如果去掉 `SDL_RenderPresent` 会怎样？**
   - 实验：注释掉这行，观察现象

4. **如何实现精确的音视频同步？**
   - 提示：用 `frame->pts` 和 `av_q2d(time_base)`

## 下一章预告

**网络播放器**：把本地文件换成 RTMP 直播流
- 新增：网络模块，TCP 连接，FLV 解封装
- 挑战：网络延迟 vs 缓冲大小的权衡

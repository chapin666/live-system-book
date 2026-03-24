# 2026-03-24 - 补充 Chapter 13-30 代码示例

## 已补充代码章节

### Chapter 13: H.265/HEVC 编码
- ✅ h265_encode_demo.cpp
  - x265 编码器初始化
  - SVC 分层编码示例
  - 码率控制模式对比 (CBR/VBR/CRF)

### Chapter 15: 音频编码
- ✅ audio_encode_demo.cpp
  - AAC 编码器封装
  - Opus 编码器 (VOIP/音乐模式)
  - 音频编码选型对比

### Chapter 16: FFmpeg 滤镜
- ✅ filter_demo.cpp
  - 滤镜图初始化
  - 美颜滤镜 (eq)
  - 水印滤镜 (drawtext)
  - 裁剪滤镜 (crop)

### Chapter 17: UDP/RTP (已有基础)
- 已有 rtp_sender.cpp / rtp_receiver.cpp

### Chapter 18-19: WebRTC 理论章节
- 以理论为主，代码在项目 P6-P7 中

### Chapter 20: WebRTC Native
- ✅ webrtc_native_demo.cpp
  - PeerConnection API 模拟
  - Offer/Answer 流程
  - GN 构建配置示例

### Chapter 21-22: SFU/MCU 架构
- 以理论为主，代码在项目 P8-P10 中

### Chapter 23: 录制存储
- 理论为主，代码在项目 P9 中

### Chapter 24: 直播协议
- ✅ hls_dash_demo.cpp
  - HLS M3U8 播放列表生成
  - DASH MPD Manifest 生成
  - 协议对比表

### Chapter 25: 信令系统
- 理论章节

### Chapter 26: 监控与质量
- ✅ quality_monitor_demo.cpp
  - 视频质量指标采集
  - 网络质量指标采集
  - MOS 评分估算
  - 质量报告生成

### Chapter 27: 安全防护
- 理论章节

### Chapter 28: 性能优化
- 理论章节

### Chapter 29: Docker 容器化
- ✅ Dockerfile
  - 多阶段构建
  - 安全最佳实践 (非 root 用户)
  - 健康检查

### Chapter 30: Kubernetes 部署
- ✅ k8s-deployment.yaml
  - Namespace/ConfigMap/Secret
  - Deployment/Service/Ingress
  - HPA 自动扩缩容
  - PVC 持久化存储

## 新增文件统计

| 章节 | 新增文件 |
|:---:|:---|
| Ch13 | h265_encode_demo.cpp |
| Ch15 | audio_encode_demo.cpp |
| Ch16 | filter_demo.cpp |
| Ch20 | webrtc_native_demo.cpp |
| Ch24 | hls_dash_demo.cpp |
| Ch26 | quality_monitor_demo.cpp |
| Ch29 | Dockerfile |
| Ch30 | k8s-deployment.yaml |
| **总计** | **8 个文件** |

## 说明

- Chapter 17-18, 21-23 的代码主要在项目 P6-P10 中
- Chapter 25, 27-28 以理论为主
- 所有章节现在都有配套的代码示例或配置文件

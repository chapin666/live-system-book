# 第十三章：视频编码进阶

> **本章目标**：掌握 H.265/AV1 编码、SVC 可伸缩编码，理解码率控制的高级策略。

第十二章我们使用 x264 实现了 H.264 编码和 RTMP 推流。H.264 是目前兼容性最好的编码格式，但在 4K 时代，它的压缩效率已显不足。

本章将学习新一代编码技术：
- **H.265/HEVC**：相同质量下码率比 H.264 降低 50%
- **AV1**：开源免版税，压缩效率比 H.265 再高 20%
- **SVC**：可伸缩分层编码，自适应网络带宽

**阅读指南**：
- 第 1-2 节：回顾 H.264，理解为什么需要新编码
- 第 3-5 节：H.265、AV1 编码原理与实现
- 第 6-7 节：SVC 分层编码、码率控制策略
- 第 8-9 节：编码器选型、本章总结

---

## 目录

1. [H.264 的局限：4K 时代的挑战](#1-h264-的局限4k-时代的挑战)
2. [编码效率对比：H.264 vs H.265 vs AV1](#2-编码效率对比h264-vs-h265-vs-av1)
3. [H.265/HEVC 编码](#3-h265hevc-编码)
4. [AV1 编码](#4-av1-编码)
5. [硬件编码：NVIDIA NVENC / Intel QSV](#5-硬件编码nvidia-nvenc--intel-qsv)
6. [SVC 可伸缩分层编码](#6-svc-可伸缩分层编码)
7. [码率控制策略详解](#7-码率控制策略详解)
8. [编码器选型指南](#8-编码器选型指南)
9. [本章总结](#9-本章总结)

---

## 1. H.264 的局限：4K 时代的挑战

### 1.1 压缩效率瓶颈

H.264 诞生于 2003 年，针对 480p/720p 视频设计。在 4K 时代面临挑战：

| 分辨率 | H.264 码率 | H.265 码率 | 节省 |
|:---|---:|---:|---:|
| 1080p@30fps | 8 Mbps | 4 Mbps | 50% |
| 4K@30fps | 35 Mbps | 15 Mbps | 57% |
| 4K@60fps | 60 Mbps | 25 Mbps | 58% |

**问题**：4K H.264 直播需要 35-60 Mbps 上行带宽，普通用户难以满足。

### 1.2 编码复杂度

H.264 的计算复杂度已接近极限：
- 高清直播 CPU 占用 30-50%
- 4K 直播几乎必须硬件编码

新一代编码在保持效率的同时，优化了并行性，更适合 GPU/专用芯片。

### 1.3 专利费用

H.264 和 H.265 都有专利池（MPEG LA），需要缴纳授权费：
- **AV1**：由 AOMedia 开发，完全免版税
- 浏览器支持：Chrome/Firefox/Safari 原生支持 AV1

---

## 2. 编码效率对比：H.264 vs H.265 vs AV1

### 2.1 核心改进对比

| 特性 | H.264 | H.265 | AV1 |
|:---|:---|:---|:---|
| 块大小 | 4x4 ~ 16x16 | 4x4 ~ 64x64 | 4x4 ~ 128x128 |
| 帧内预测方向 | 9 种 | 35 种 | 56 种 |
| 运动补偿 | 1/4 像素 | 1/8 像素 | 1/8 像素 |
| 环路滤波 | 去块滤波 | 去块滤波+SAO | CDEF+LR |
| 参考帧 | 16 帧 | 16 帧 | 8 帧 |

### 2.2 BD-Rate 对比

BD-Rate（Bjøntegaard Delta Rate）是衡量编码效率的标准指标：

```
相同质量下，码率相比 H.264 的节省：
- H.265：约 40-50%
- AV1：约 50-60%
```

### 2.3 编码速度对比

编码速度（越快越好）：

| 编码器 | 速度 | 质量 | 适用场景 |
|:---|:---:|:---:|:---|
| x264 (H.264) | 100% | 基准 | 通用直播 |
| x265 (H.265) | 30% | +40% | 点播、存储 |
| SVT-AV1 | 15% | +50% | 慢直播、VOD |
| NVENC H.265 | 500% | +35% | 快速直播 |

> **速度 vs 质量**：AV1 软件编码很慢（SVT-AV1 约 x264 的 1/6），但质量提升显著。实际直播常使用硬件编码。

---

## 3. H.265/HEVC 编码

### 3.1 H.265 核心改进

**CTU（Coding Tree Unit）**：最大 64x64 块
```
H.264：固定 16x16 宏块
H.265：64x64 → 32x32 → 16x16 → 8x8 四叉树划分
      平坦区域用大块，细节区域用小块
```

**更多帧内预测方向**：
- H.264：9 种（8 角度 + DC）
- H.265：35 种（33 角度 + DC + Planar）

### 3.2 x265 编码器使用

x265 是 H.265 的开源实现，API 与 x264 类似：

```cpp
#include <x265.h>

class H265Encoder {
public:
    bool Init(int width, int height, int fps, int bitrate) {
        x265_param* param = x265_param_alloc();
        x265_param_default(param);
        
        param->sourceWidth = width;
        param->sourceHeight = height;
        param->fpsNum = fps;
        param->fpsDenom = 1;
        param->bitrate = bitrate;  // kbps
        
        // 预设：ultrafast 到 veryslow
        x265_param_preset(param, "medium");
        
        // 直播场景：低延迟模式
        param->bEnableAccessUnitDelimiters = 0;
        param->bframes = 0;  // 无 B 帧，降低延迟
        
        encoder_ = x265_encoder_open(param);
        x265_param_free(param);
        
        return encoder_ != nullptr;
    }
    
    bool EncodeFrame(const uint8_t* yuv_data, std::vector<uint8_t>* output) {
        x265_picture pic_in, pic_out;
        x265_picture_init(x265_param_alloc(), &pic_in);
        
        pic_in.planes[0] = (void*)yuv_data;
        pic_in.planes[1] = (void*)(yuv_data + width_ * height_);
        pic_in.planes[2] = (void*)(yuv_data + width_ * height_ * 5 / 4);
        pic_in.stride[0] = width_;
        pic_in.stride[1] = width_ / 2;
        pic_in.stride[2] = width_ / 2;
        
        x265_nal* nals;
        uint32_t nal_count;
        int ret = x265_encoder_encode(encoder_, &nals, &nal_count, &pic_in, &pic_out);
        
        if (ret > 0) {
            output->clear();
            for (uint32_t i = 0; i < nal_count; i++) {
                output->insert(output->end(), nals[i].payload, nals[i].payload + nals[i].sizeBytes);
            }
            return true;
        }
        return false;
    }
    
private:
    x265_encoder* encoder_ = nullptr;
    int width_, height_;
};
```

### 3.3 H.265 直播注意事项

**兼容性**：
- iOS 11+、Android 5.0+ 原生支持 H.265 解码
- 部分浏览器（Safari）支持
- RTMP 协议不支持 H.265，需使用 RTSP/HTTP-FLV/HLS

**专利授权**：
- 流媒体服务可能需要缴纳 H.265 专利费
- 个人学习/开源项目通常免费

---

## 4. AV1 编码

### 4.1 AV1 核心特性

AV1（AOMedia Video 1）由 Alliance for Open Media 开发：
- **免版税**：无专利授权问题
- **开源**：libaom、SVT-AV1、rav1e 多个实现
- **浏览器原生支持**：Chrome 70+、Firefox 67+、Safari 16+

### 4.2 SVT-AV1 编码器

SVT-AV1（Scalable Video Technology for AV1）是 Intel 开发的高性能编码器：

```cpp
#include <svt-av1/EbSvtAv1Enc.h>

class AV1Encoder {
public:
    bool Init(int width, int height, int fps, int bitrate) {
        EbSvtAv1EncConfiguration cfg;
        eb_init_handle(&encoder_, nullptr, &cfg);
        
        cfg.source_width = width;
        cfg.source_height = height;
        cfg.frame_rate_numerator = fps;
        cfg.frame_rate_denominator = 1;
        cfg.target_bit_rate = bitrate * 1000;  // bps
        
        // 预设：0-8，数值越大质量越好速度越慢
        cfg.enc_mode = 4;  // 平衡模式
        
        // 直播优化
        cfg.look_ahead_distance = 0;  // 无前瞻延迟
        cfg.intra_period_length = fps;  // 1 秒 GOP
        
        EbErrorType ret = eb_svt_enc_init(encoder_);
        return ret == EB_ErrorNone;
    }
    
private:
    EbComponentType* encoder_ = nullptr;
};
```

### 4.3 AV1 实时编码现状

**软件编码**：
- SVT-AV1 模式 8：接近 x264 veryslow 质量，但速度很慢
- 适合：VOD 转码、慢直播

**硬件编码**：
- Intel Arc GPU（AV1 编码）
- NVIDIA RTX 40 系列（NVENC AV1）
- 速度可达 4K@60fps 实时

---

## 5. 硬件编码：NVIDIA NVENC / Intel QSV

### 5.1 为什么需要硬件编码

4K 软件编码的问题：
- CPU 占用 80-100%，无法同时进行其他处理
- 功耗高，笔记本电池快速耗尽
- 发热严重，可能触发降频

硬件编码解决方案：
- **NVIDIA NVENC**：RTX 显卡专用编码器
- **Intel QSV**：核显 Quick Sync Video
- **Apple VideoToolbox**：M1/M2/M3 芯片

### 5.2 FFmpeg 硬件编码示例

**NVIDIA NVENC H.265**：
```bash
ffmpeg -i input.mp4 -c:v hevc_nvenc -preset p4 -cq 23 output.mp4
```

**Intel QSV H.265**：
```bash
ffmpeg -i input.mp4 -c:v hevc_qsv -preset medium -global_quality 23 output.mp4
```

**Apple VideoToolbox H.265**：
```bash
ffmpeg -i input.mp4 -c:v hevc_videotoolbox -q:v 65 output.mp4
```

### 5.3 NVENC 参数详解

```cpp
// FFmpeg NVENC 编码器选项
AVDictionary* opts = nullptr;

// 预设：p1(最快) ~ p7(最慢)
av_dict_set(&opts, "preset", "p4", 0);

// 质量控制：cq 模式
av_dict_set(&opts, "rc", "vbr", 0);
av_dict_set(&opts, "cq", "23", 0);

// 多帧参考
av_dict_set(&opts, "refs", "4", 0);

// B帧数量（直播通常设为0）
av_dict_set(&opts, "bf", "0", 0);

// 初始化编码器
AVCodec* codec = avcodec_find_encoder_by_name("hevc_nvenc");
AVCodecContext* ctx = avcodec_alloc_context3(codec);
// ... 设置参数
avcodec_open2(ctx, codec, &opts);
```

---

## 6. SVC 可伸缩分层编码

### 6.1 什么是 SVC

SVC（Scalable Video Coding）允许一次编码产生**多层视频**：

```
┌─────────────────────────┐
│      增强层 (EL)         │  ← 高分辨率/高质量
│    1920x1080 @ 30fps    │
├─────────────────────────┤
│      基础层 (BL)         │  ← 低分辨率/基础质量
│     960x540 @ 15fps     │
└─────────────────────────┘
```

**优势**：
- 网络差时只传基础层，保证流畅
- 网络好时传增强层，提升质量
- 无需多次编码，节省服务器资源

### 6.2 SVC 的三种伸缩性

**时间可伸缩（Temporal Scalability）**：
```
T2:  ●     ●     ●     ●     (30fps)
T1:  ●  ●  ●  ●  ●  ●  ●  ●  (15fps)
T0:  ●  ●  ●  ●  ●  ●  ●  ●  (7.5fps)
```
- 丢帧时优先丢弃高时间层
- 保持画面流畅，降低帧率

**空间可伸缩（Spatial Scalability）**：
- 基础层：540p
- 增强层：1080p

**质量可伸缩（Quality Scalability）**：
- 同一分辨率，不同 QP（量化参数）

### 6.3 VP9 SVC 实现

VP9 原生支持 SVC，WebRTC 广泛使用：

```cpp
// libvpx VP9 SVC 编码配置
vpx_codec_enc_cfg_t cfg;
vpx_codec_enc_config_default(vpx_codec_vp9_cx(), &cfg, 0);

cfg.rc_target_bitrate = 2000;  // 总码率 2Mbps

// 配置 3 层：T0(7.5fps), T1(15fps), T2(30fps)
// 以及 2 层空间：540p 和 1080p
svc_params_.number_spatial_layers = 2;
svc_params_.number_temporal_layers = 3;
svc_params_.max_quantizers[0] = 56;
svc_params_.max_quantizers[1] = 48;
svc_params_.min_quantizers[0] = 33;
svc_params_.min_quantizers[1] = 25;

vpx_codec_control(&codec_, VP9E_SET_SVC_PARAMETERS, &svc_params_);
```

---

## 7. 码率控制策略详解

### 7.1 CBR vs VBR vs CRF

| 模式 | 说明 | 适用场景 |
|:---|:---|:---|
| CBR | 恒定码率，波动小 | 直播（推荐） |
| VBR | 可变码率，质量优先 | 点播、存储 |
| CRF | 恒定质量，码率波动大 | 存档、转码 |

### 7.2 直播码率控制

**CBR 设置要点**：
```cpp
// x264 CBR 直播配置
x264_param_t param;
x264_param_default_preset(&param, "veryfast", "zerolatency");

param.rc.i_rc_method = X264_RC_ABR;  // 平均码率
param.rc.i_bitrate = 4000;            // 4 Mbps
param.rc.i_vbv_max_bitrate = 4000;    // 最大码率
param.rc.i_vbv_buffer_size = 400;     // 缓冲 100ms
param.rc.b_filler = 1;                // 填充保持 CBR
```

**为什么直播用 CBR？**
- 网络带宽固定，CBR 不会突然超出
- 观众端缓冲稳定，减少卡顿
- CDN 流量成本可控

### 7.3 自适应码率（ABR）

根据网络状况动态调整码率：

```cpp
class AdaptiveBitrateController {
public:
    void OnNetworkReport(int rtt_ms, float loss_rate) {
        if (loss_rate > 0.05f) {
            // 丢包严重，降码率
            target_bitrate_ *= 0.8;
        } else if (loss_rate < 0.01f && rtt_ms < 100) {
            // 网络良好，尝试升码率
            target_bitrate_ *= 1.1;
        }
        target_bitrate_ = std::clamp(target_bitrate_, min_bitrate_, max_bitrate_);
    }
    
    int GetTargetBitrate() const { return target_bitrate_; }
    
private:
    int target_bitrate_ = 4000000;  // 4 Mbps
    int min_bitrate_ = 500000;       // 500 kbps
    int max_bitrate_ = 8000000;      // 8 Mbps
};
```

---

## 8. 编码器选型指南

### 8.1 决策流程图

```
是否需要浏览器播放？
├── 是 → 使用 H.264（兼容性最好）
└── 否 → 是否需要最低码率？
    ├── 是 → 有 NVIDIA RTX 40？
    │   ├── 是 → NVENC AV1
    │   └── 否 → x265 / SVT-AV1（慢）
    └── 否 → 有硬件编码？
        ├── 是 → NVENC/QSV H.265
        └── 否 → x264（平衡）
```

### 8.2 场景推荐

| 场景 | 推荐编码 | 理由 |
|:---|:---|:---|
| 普通直播 | H.264 (x264) | 兼容性无敌，硬件支持广 |
| 4K 直播 | H.265 (NVENC) | 码率低，RTX 编码快 |
| 游戏直播 | H.264 (NVENC) | 不占用 CPU，游戏更流畅 |
| 视频会议 | H.264/SVC | 自适应带宽，多端兼容 |
| 存档/转码 | AV1 (SVT-AV1) | 最高压缩率，时间充裕 |
| Web 直播 | H.264/H.265 | WebRTC 支持 |

---

## 9. 本章总结

### 核心概念

| 概念 | 一句话解释 |
|:---|:---|
| H.265 | 比 H.264 省 50% 码率，但有专利费 |
| AV1 | 免版税，压缩率更高，但编码慢 |
| SVC | 一次编码多层输出，自适应网络 |
| CBR/VBR | 恒定码率适合直播，可变码率适合点播 |
| 硬件编码 | GPU/专用芯片编码，低 CPU 占用 |

### 关键技能

- 使用 x265、SVT-AV1 进行软件编码
- 配置 NVENC/QSV 硬件编码
- 理解 SVC 分层原理
- 根据场景选择合适的编码器

### 下一步

第十四章将学习**高级采集技术**——屏幕采集、多摄像头切换、采集参数优化。

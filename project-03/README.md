# 项目实战3：直播观众端

> **前置要求**：完成 Chapter 7-8
> **目标**：实现 RTMP 直播拉流播放器

## 项目概述

本项目实现一个**直播观众端**，支持 RTMP 协议拉流观看直播。与点播相比，直播的特点是：
- 实时性优先（延迟 2-5 秒）
- 不能拖动进度
- 网络波动时需要快速恢复
- 追帧策略（落后太多时丢弃帧）

## 功能需求

### 直播播放
- [x] 支持 RTMP 流播放 (`rtmp://`)
- [x] 自动识别直播流 vs 点播
- [x] 实时延迟显示

### 直播优化
- [x] 追帧策略（落后超过阈值时加速或丢帧）
- [x] 低延迟模式配置
- [x] 断流自动重连

### 信息显示
- [x] 显示当前延迟
- [x] 显示丢帧统计
- [x] 显示码率

## 关键技术

### RTMP 播放

```cpp
bool OpenRtmpStream(const char* url) {
    AVDictionary* opts = nullptr;
    
    // RTMP 特定选项
    av_dict_set(&opts, "rtmp_buffer", "100", 0);  // 100ms 缓冲
    av_dict_set(&opts, "rtmp_live", "live", 0); // 直播模式
    
    // 低延迟选项
    av_dict_set(&opts, "fflags", "nobuffer", 0);
    av_dict_set(&opts, "flags", "low_delay", 0);
    
    int ret = avformat_open_input(&fmt_ctx_, url, nullptr, &opts);
    av_dict_free(&opts);
    
    return ret >= 0;
}
```

### 追帧策略

```cpp
void LivePlayer::HandleFrameTiming(AVFrame* frame) {
    int64_t pts_us = frame->pts * av_q2d(time_base_) * 1000000;
    int64_t now = av_gettime();
    int64_t delay = now - pts_us;
    
    // 延迟统计
    current_delay_ms_ = delay / 1000;
    
    if (delay > max_delay_threshold_ms_ * 1000) {
        // 延迟太大，丢帧追赶
        dropped_frames_++;
        return;  // 不渲染这一帧
    } else if (delay > 500000) {
        // 延迟较大，加速播放（跳过睡眠）
        // 直接渲染，不等待
    } else {
        // 正常同步
        int64_t wait_time = pts_us - now;
        if (wait_time > 0) {
            av_usleep(wait_time);
        }
    }
    
    renderer_->RenderFrame(frame);
}
```

### 直播 vs 点播检测

```cpp
bool IsLiveStream(AVFormatContext* ctx) {
    // 方法1: 检查 duration
    if (ctx->duration == AV_NOPTS_VALUE || ctx->duration <= 0) {
        return true;  // 无时长 = 直播
    }
    
    // 方法2: 检查是否可 seek
    if (!(ctx->iformat->flags & AVFMT_SEEKABLE)) {
        return true;  // 不可 seek = 直播
    }
    
    return false;
}
```

### 断流恢复

```cpp
void LivePlayer::Run() {
    while (!should_stop_) {
        if (!connected_) {
            // 尝试连接
            if (!Connect(url_)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;
            }
            connected_ = true;
        }
        
        // 读取数据
        int ret = av_read_frame(fmt_ctx_, packet);
        if (ret < 0) {
            // 断流了
            connected_ = false;
            Cleanup();
            std::cerr << "连接断开，尝试重连..." << std::endl;
            continue;
        }
        
        // 处理数据...
    }
}
```

## 项目结构

```
project-03/
├── CMakeLists.txt
├── README.md
├── include/
│   └── live/
│       └── live_player.h
└── src/
    ├── main.cpp
    └── live_player.cpp
```

## 使用方法

```bash
# 播放 RTMP 直播流
./player "rtmp://live.example.com/stream/key"

# 显示统计信息
./player "rtmp://..." --stats
```

## 验收标准

- [ ] 能播放 RTMP 直播流
- [ ] 延迟显示准确（2-5秒）
- [ ] 断流后能自动恢复
- [ ] 追帧策略有效（落后时丢帧）

## 扩展挑战

1. 支持 HLS 协议 (`http://.../index.m3u8`)
2. 支持 DASH 协议
3. 实现录制功能（边播边存）
4. 添加弹幕显示

---

**完成本项目后，你将掌握：**
- RTMP 协议基础
- 直播延迟优化
- 追帧/丢帧策略
- 直播流断线重连

---

## Part 1 完成！

完成本项目实战3后，你已经：
- ✅ 能开发本地播放器
- ✅ 能开发网络点播播放器  
- ✅ 能开发直播观众端

**具备了完整的「播放器端」开发能力！**

下一步进入 **Part 2：主播端开发**（采集、编码、推流）

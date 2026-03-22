# 第十六章：主播端架构

> **本章目标**：整合采集、处理、编码、推流，实现完整的主播端 Pipeline。

前八章（Ch9-Ch15）完成了主播端的所有核心组件：
- Ch9：硬件解码优化（播放端）
- Ch10：音视频采集
- Ch11：音频 3A 处理
- Ch12：编码与推流
- Ch13：视频编码进阶
- Ch14：高级采集技术
- Ch15：美颜与滤镜

本章将这些组件**整合为一个完整的 Pipeline**，实现可开播的主播端。

**阅读指南**：
- 第 1-2 节：架构设计、模块划分
- 第 3-5 节：Pipeline 实现、线程模型、同步机制
- 第 6-7 节：状态管理、错误恢复
- 第 8 节：本章总结

---

## 目录

1. [主播端架构概览](#1-主播端架构概览)
2. [模块划分与接口设计](#2-模块划分与接口设计)
3. [Pipeline 实现](#3-pipeline-实现)
4. [线程模型](#4-线程模型)
5. [音视频同步](#5-音视频同步)
6. [状态管理与控制](#6-状态管理与控制)
7. [错误处理与恢复](#7-错误处理与恢复)
8. [本章总结](#8-本章总结)

---

## 1. 主播端架构概览

### 1.1 数据流图

```
┌─────────────────────────────────────────────────────────────────┐
│                         主播端 Pipeline                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────┐      ┌──────────┐      ┌──────────┐      ┌────────┐ │
│  │ 视频采集  │ ───→ │ 美颜滤镜  │ ───→ │ 视频编码  │ ───→ │        │ │
│  │ 摄像头    │      │ GPU处理   │      │ H.264/265 │      │        │ │
│  └──────────┘      └──────────┘      └──────────┘      │  混合  │ │
│                                                         │  封装  │─┼→ RTMP
│  ┌──────────┐      ┌──────────┐      ┌──────────┐      │        │ │   输出
│  │ 音频采集  │ ───→ │ 3A处理    │ ───→ │ 音频编码  │ ───→ │        │ │
│  │ 麦克风    │      │ AEC/ANS   │      │ AAC       │      └────────┘ │
│  └──────────┘      └──────────┘      └──────────┘               │
│                                                                  │
│  ┌──────────┐      ┌──────────┐                                  │
│  │ 屏幕采集  │ ───→ │ 画中画   │───────────────────────────────────┘
│  │ (可选)    │      │ 合成     │
│  └──────────┘      └──────────┘
│
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 关键指标

| 指标 | 目标值 | 说明 |
|:---|:---|:---|
| 端到端延迟 | < 500ms | 采集到推流 |
| CPU 占用 | < 50% | 1080p@30fps |
| 内存占用 | < 500MB | 含缓冲 |
| 丢帧率 | < 1% | 网络正常时 |

---

## 2. 模块划分与接口设计

### 2.1 核心模块

```cpp
// 视频采集模块
class IVideoCapture {
public:
    virtual ~IVideoCapture() = default;
    virtual bool Init(const CaptureConfig& config) = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual std::shared_ptr<VideoFrame> GetFrame() = 0;
    virtual void SetCallback(FrameCallback callback) = 0;
};

// 美颜处理模块
class IBeautyProcessor {
public:
    virtual ~IBeautyProcessor() = default;
    virtual bool Init(int width, int height) = 0;
    virtual std::shared_ptr<VideoFrame> Process(std::shared_ptr<VideoFrame> frame) = 0;
    virtual void SetParams(const BeautyParams& params) = 0;
};

// 视频编码模块
class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;
    virtual bool Init(const EncoderConfig& config) = 0;
    virtual bool Encode(std::shared_ptr<VideoFrame> frame) = 0;
    virtual std::shared_ptr<EncodedPacket> GetPacket() = 0;
};

// 音频处理模块（3A）
class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    virtual bool Init(const AudioConfig& config) = 0;
    virtual std::shared_ptr<AudioFrame> Process(std::shared_ptr<AudioFrame> frame) = 0;
};

// 音频编码模块
class IAudioEncoder {
public:
    virtual ~IAudioEncoder() = default;
    virtual bool Init(const AudioEncoderConfig& config) = 0;
    virtual bool Encode(std::shared_ptr<AudioFrame> frame) = 0;
    virtual std::shared_ptr<EncodedPacket> GetPacket() = 0;
};

// 推流模块
class IRtmpPusher {
public:
    virtual ~IRtmpPusher() = default;
    virtual bool Connect(const std::string& url) = 0;
    virtual bool PushVideo(std::shared_ptr<EncodedPacket> packet) = 0;
    virtual bool PushAudio(std::shared_ptr<EncodedPacket> packet) = 0;
    virtual void Disconnect() = 0;
    virtual ConnectionState GetState() const = 0;
};
```

### 2.2 配置结构

```cpp
struct StreamerConfig {
    // 视频配置
    struct Video {
        int width = 1280;
        int height = 720;
        int fps = 30;
        int bitrate = 4000000;  // 4 Mbps
        std::string codec = "h264";  // h264/h265
        std::string preset = "veryfast";
    } video;
    
    // 音频配置
    struct Audio {
        int sample_rate = 48000;
        int channels = 2;
        int bitrate = 128000;  // 128 kbps
    } audio;
    
    // 3A 配置
    struct Audio3A {
        bool enable_aec = true;
        bool enable_ans = true;
        bool enable_agc = true;
    } audio_3a;
    
    // 美颜配置
    BeautyParams beauty;
    
    // 推流配置
    std::string rtmp_url;
    int reconnect_attempts = 3;
    int reconnect_delay_ms = 5000;
};
```

---

## 3. Pipeline 实现

### 3.1 Pipeline 类

```cpp
class StreamerPipeline {
public:
    bool Init(const StreamerConfig& config) {
        config_ = config;
        
        // 1. 初始化视频采集
        video_capture_ = CreateVideoCapture();
        if (!video_capture_->Init({config.video.width, config.video.height, config.video.fps})) {
            return false;
        }
        
        // 2. 初始化美颜处理
        beauty_processor_ = CreateBeautyProcessor();
        if (!beauty_processor_->Init(config.video.width, config.video.height)) {
            return false;
        }
        beauty_processor_->SetParams(config.beauty);
        
        // 3. 初始化视频编码
        video_encoder_ = CreateVideoEncoder();
        EncoderConfig venc_config;
        venc_config.width = config.video.width;
        venc_config.height = config.video.height;
        venc_config.fps = config.video.fps;
        venc_config.bitrate = config.video.bitrate;
        venc_config.codec = config.video.codec;
        if (!video_encoder_->Init(venc_config)) {
            return false;
        }
        
        // 4. 初始化音频采集
        audio_capture_ = CreateAudioCapture();
        if (!audio_capture_->Init({config.audio.sample_rate, config.audio.channels})) {
            return false;
        }
        
        // 5. 初始化 3A 处理
        audio_processor_ = CreateAudioProcessor();
        if (!audio_processor_->Init({config.audio.sample_rate, config.audio.channels,
                                     config.audio_3a.enable_aec,
                                     config.audio_3a.enable_ans,
                                     config.audio_3a.enable_agc})) {
            return false;
        }
        
        // 6. 初始化音频编码
        audio_encoder_ = CreateAudioEncoder();
        if (!audio_encoder_->Init({config.audio.sample_rate, config.audio.channels,
                                   config.audio.bitrate})) {
            return false;
        }
        
        // 7. 初始化推流器
        rtmp_pusher_ = CreateRtmpPusher();
        
        return true;
    }
    
    bool Start() {
        // 连接 RTMP
        if (!rtmp_pusher_->Connect(config_.rtmp_url)) {
            return false;
        }
        
        // 启动采集
        video_capture_->Start();
        audio_capture_->Start();
        
        // 启动处理线程
        running_ = true;
        video_thread_ = std::thread(&StreamerPipeline::VideoProcessingLoop, this);
        audio_thread_ = std::thread(&StreamerPipeline::AudioProcessingLoop, this);
        push_thread_ = std::thread(&StreamerPipeline::PushLoop, this);
        
        return true;
    }
    
    void Stop() {
        running_ = false;
        
        // 停止采集
        video_capture_->Stop();
        audio_capture_->Stop();
        
        // 等待线程结束
        if (video_thread_.joinable()) video_thread_.join();
        if (audio_thread_.joinable()) audio_thread_.join();
        if (push_thread_.joinable()) push_thread_.join();
        
        // 断开推流
        rtmp_pusher_->Disconnect();
    }
    
private:
    void VideoProcessingLoop() {
        while (running_) {
            auto frame = video_capture_->GetFrame();
            if (!frame) continue;
            
            // 美颜处理
            frame = beauty_processor_->Process(frame);
            
            // 编码
            video_encoder_->Encode(frame);
            
            // 取出编码包放入队列
            while (auto packet = video_encoder_->GetPacket()) {
                video_packet_queue_.Push(packet);
            }
        }
    }
    
    void AudioProcessingLoop() {
        while (running_) {
            auto frame = audio_capture_->GetFrame();
            if (!frame) continue;
            
            // 3A 处理
            frame = audio_processor_->Process(frame);
            
            // 编码
            audio_encoder_->Encode(frame);
            
            // 取出编码包放入队列
            while (auto packet = audio_encoder_->GetPacket()) {
                audio_packet_queue_.Push(packet);
            }
        }
    }
    
    void PushLoop() {
        while (running_) {
            // 优先推送音频（延迟敏感）
            if (auto audio_packet = audio_packet_queue_.TryPop(10)) {
                rtmp_pusher_->PushAudio(audio_packet);
            }
            
            // 推送视频
            if (auto video_packet = video_packet_queue_.TryPop(10)) {
                rtmp_pusher_->PushVideo(video_packet);
            }
            
            // 检查连接状态
            if (rtmp_pusher_->GetState() == ConnectionState::DISCONNECTED) {
                HandleDisconnect();
            }
        }
    }
    
    void HandleDisconnect() {
        // 重连逻辑
        for (int i = 0; i < config_.reconnect_attempts; i++) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.reconnect_delay_ms));
            
            if (rtmp_pusher_->Connect(config_.rtmp_url)) {
                return;
            }
        }
        
        // 重连失败，停止推流
        running_ = false;
    }
    
    StreamerConfig config_;
    
    std::unique_ptr<IVideoCapture> video_capture_;
    std::unique_ptr<IBeautyProcessor> beauty_processor_;
    std::unique_ptr<IVideoEncoder> video_encoder_;
    
    std::unique_ptr<IAudioCapture> audio_capture_;
    std::unique_ptr<IAudioProcessor> audio_processor_;
    std::unique_ptr<IAudioEncoder> audio_encoder_;
    
    std::unique_ptr<IRtmpPusher> rtmp_pusher_;
    
    ThreadSafeQueue<std::shared_ptr<EncodedPacket>> video_packet_queue_;
    ThreadSafeQueue<std::shared_ptr<EncodedPacket>> audio_packet_queue_;
    
    std::thread video_thread_;
    std::thread audio_thread_;
    std::thread push_thread_;
    std::atomic<bool> running_{false};
};
```

---

## 4. 线程模型

### 4.1 线程职责

```
主线程          视频线程              音频线程              推流线程
  │               │                     │                     │
  │ Start()       │ GetFrame()          │ GetFrame()          │
  ├──────────────→│                     │                     │
  │               │ Process()           │ Process()           │
  │               │ Encode()            │ Encode()            │
  │               │ ───────Push──────→  │ ───────Push──────→  │
  │               │                     │                     │ TryPop()
  │               │                     │                     │ PushVideo/Audio()
  │ Stop()        │                     │                     │
  ├──────────────→│                     │                     │
```

### 4.2 队列深度控制

```cpp
class BoundedQueue {
public:
    void Push(std::shared_ptr<EncodedPacket> packet) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 队列满时丢弃旧数据
        while (queue_.size() >= max_size_) {
            queue_.pop_front();
            dropped_count_++;
        }
        
        queue_.push_back(packet);
        cv_.notify_one();
    }
    
    std::shared_ptr<EncodedPacket> TryPop(int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                          [this] { return !queue_.empty(); })) {
            return nullptr;
        }
        
        auto packet = queue_.front();
        queue_.pop_front();
        return packet;
    }
    
private:
    std::deque<std::shared_ptr<EncodedPacket>> queue_;
    size_t max_size_ = 30;  // 最大缓冲 30 帧（约 1 秒）
    size_t dropped_count_ = 0;
    std::mutex mutex_;
    std::condition_variable cv_;
};
```

---

## 5. 音视频同步

### 5.1 时间戳生成

```cpp
class TimestampGenerator {
public:
    void Start() {
        start_time_ = GetCurrentTimeMicros();
        start_timestamp_ = 0;
    }
    
    int64_t GetTimestamp() {
        auto elapsed = GetCurrentTimeMicros() - start_time_;
        return start_timestamp_ + elapsed;
    }
    
private:
    int64_t start_time_ = 0;
    int64_t start_timestamp_ = 0;
};
```

### 5.2 同步策略

```cpp
class SyncController {
public:
    void OnVideoPacket(std::shared_ptr<EncodedPacket> packet) {
        packet->pts = timestamp_gen_.GetTimestamp();
        video_queue_.Push(packet);
    }
    
    void OnAudioPacket(std::shared_ptr<EncodedPacket> packet) {
        packet->pts = timestamp_gen_.GetTimestamp();
        audio_queue_.Push(packet);
    }
    
    // 获取下一帧（按时间戳排序）
    std::shared_ptr<EncodedPacket> GetNextPacket() {
        auto video = video_queue_.Peek();
        auto audio = audio_queue_.Peek();
        
        if (!video) return audio_queue_.Pop();
        if (!audio) return video_queue_.Pop();
        
        // 选择时间戳较小的
        if (video->pts <= audio->pts) {
            return video_queue_.Pop();
        } else {
            return audio_queue_.Pop();
        }
    }
};
```

---

## 6. 状态管理与控制

### 6.1 状态机

```cpp
enum class StreamerState {
    IDLE,           // 空闲
    INITIALIZING,   // 初始化中
    READY,          // 就绪
    CONNECTING,     // 连接中
    STREAMING,      // 推流中
    PAUSED,         // 暂停
    ERROR,          // 错误
    STOPPING        // 停止中
};

class StreamerController {
public:
    bool StartStreaming() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (state_ != StreamerState::READY) {
            return false;
        }
        
        state_ = StreamerState::CONNECTING;
        
        // 异步启动
        std::thread([this]() {
            if (pipeline_->Start()) {
                SetState(StreamerState::STREAMING);
            } else {
                SetState(StreamerState::ERROR);
            }
        }).detach();
        
        return true;
    }
    
    void StopStreaming() {
        SetState(StreamerState::STOPPING);
        pipeline_->Stop();
        SetState(StreamerState::READY);
    }
    
    void Pause() {
        if (state_ == StreamerState::STREAMING) {
            pipeline_->Pause();
            SetState(StreamerState::PAUSED);
        }
    }
    
    void Resume() {
        if (state_ == StreamerState::PAUSED) {
            pipeline_->Resume();
            SetState(StreamerState::STREAMING);
        }
    }
    
    StreamerState GetState() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }
    
    using StateCallback = std::function<void(StreamerState)>;
    void SetStateCallback(StateCallback callback) {
        state_callback_ = callback;
    }
    
private:
    void SetState(StreamerState state) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            state_ = state;
        }
        if (state_callback_) {
            state_callback_(state);
        }
    }
    
    StreamerState state_ = StreamerState::IDLE;
    std::mutex state_mutex_;
    StateCallback state_callback_;
    std::unique_ptr<StreamerPipeline> pipeline_;
};
```

### 6.2 运行时调节

```cpp
class StreamerRuntimeControl {
public:
    // 调节美颜参数
    void SetBeautyParams(const BeautyParams& params) {
        pipeline_->GetBeautyProcessor()->SetParams(params);
    }
    
    // 切换摄像头
    bool SwitchCamera(const std::string& device_id) {
        return pipeline_->GetVideoCapture()->SwitchDevice(device_id);
    }
    
    // 动态调整码率
    void SetVideoBitrate(int bitrate) {
        pipeline_->GetVideoEncoder()->SetBitrate(bitrate);
    }
    
    // 开启/关闭 3A
    void SetAudio3A(bool aec, bool ans, bool agc) {
        pipeline_->GetAudioProcessor()->Set3A(aec, ans, agc);
    }
};
```

---

## 7. 错误处理与恢复

### 7.1 错误分类

```cpp
enum class ErrorCode {
    // 采集错误
    CAMERA_DISCONNECTED = 1001,
    MICROPHONE_DISCONNECTED = 1002,
    
    // 编码错误
    ENCODER_INIT_FAILED = 2001,
    ENCODE_ERROR = 2002,
    
    // 推流错误
    CONNECTION_FAILED = 3001,
    CONNECTION_LOST = 3002,
    SEND_TIMEOUT = 3003,
    
    // 系统错误
    OUT_OF_MEMORY = 4001,
    CPU_OVERLOAD = 4002
};

struct ErrorInfo {
    ErrorCode code;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
    bool recoverable;  // 是否可恢复
};
```

### 7.2 自动恢复

```cpp
class ErrorRecovery {
public:
    void OnError(const ErrorInfo& error) {
        if (!error.recoverable) {
            StopAndNotify(error);
            return;
        }
        
        switch (error.code) {
            case ErrorCode::CONNECTION_LOST:
                AttemptReconnect();
                break;
            case ErrorCode::CAMERA_DISCONNECTED:
                AttemptSwitchCamera();
                break;
            case ErrorCode::ENCODE_ERROR:
                AttemptRestartEncoder();
                break;
            default:
                StopAndNotify(error);
        }
    }
    
private:
    void AttemptReconnect() {
        for (int attempt = 1; attempt <= max_retries_; attempt++) {
            NotifyStatus("重连中... (" + std::to_string(attempt) + "/" + 
                        std::to_string(max_retries_) + ")");
            
            std::this_thread::sleep_for(retry_interval_);
            
            if (pipeline_->Reconnect()) {
                NotifyStatus("重连成功");
                return;
            }
        }
        
        NotifyStatus("重连失败");
        pipeline_->Stop();
    }
    
    int max_retries_ = 3;
    std::chrono::seconds retry_interval_{5};
};
```

---

## 8. 本章总结

### 架构要点

| 模块 | 职责 | 线程 |
|:---|:---|:---|
| 视频采集 | 获取原始视频帧 | 采集线程 |
| 美颜处理 | GPU 图像处理 | 视频处理线程 |
| 视频编码 | H.264/H.265 编码 | 视频处理线程 |
| 音频采集 | 获取原始音频帧 | 采集线程 |
| 3A 处理 | AEC/ANS/AGC | 音频处理线程 |
| 音频编码 | AAC 编码 | 音频处理线程 |
| 推流器 | RTMP 发送 | 推流线程 |

### 关键设计

- **多线程并行**：采集、处理、推流分离
- **队列缓冲**：平滑处理速度波动
- **时间戳同步**：确保音视频同步
- **状态机管理**：清晰的流转控制
- **自动恢复**：网络断开自动重连

### 下一步

**项目5：完整主播端** —— 整合 Ch9-Ch16，实现可开播的完整主播工具。

# Project 09: 录制回放系统

基于MCU架构的音视频录制和HLS回放系统。

## 项目概述

本项目实现：
- 服务端音视频录制
- HLS切片生成
- 时移回放
- 多轨道同步

## 架构图

```
┌──────────────────────────────────────────────────────┐
│                    MCU Recorder                       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐              │
│  │ Stream A│  │ Stream B│  │ Stream C│  ...          │
│  └────┬────┘  └────┬────┘  └────┬────┘              │
│       │            │            │                    │
│       └────────────┼────────────┘                    │
│                    ▼                                 │
│            ┌───────────────┐                         │
│            │ Audio Mixer   │                         │
│            └───────┬───────┘                         │
│                    │                                 │
│       ┌────────────┼────────────┐                    │
│       ▼            ▼            ▼                    │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐              │
│  │ Video   │  │ Audio   │  │ Mux     │              │
│  │ Decoder │  │ Mixer   │  │ (FFmpeg)│              │
│  └────┬────┘  └────┬────┘  └────┬────┘              │
│       │            │            │                    │
│       └────────────┼────────────┘                    │
│                    ▼                                 │
│           ┌─────────────────┐                        │
│           │ HLS Segmenter   │                        │
│           │ - index.m3u8    │                        │
│           │ - 00001.ts      │                        │
│           │ - 00002.ts      │                        │
│           └─────────────────┘                        │
└──────────────────────────────────────────────────────┘
                            │
                            ▼
                   ┌────────────────┐
                   │  HTTP Server   │
                   │ /live/room.m3u8│
                   └────────────────┘
```

## 项目结构

```
project-09/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── recorder.h/.cpp        # 录制核心
│   ├── mcu_mixer.h/.cpp       # MCU混音混画
│   ├── hls_segmenter.h/.cpp   # HLS切片
│   ├── playlist_manager.h/.cpp # 播放列表管理
│   └── timeshift_buffer.h/.cpp # 时移缓存
├── web/
│   └── player.html            # HLS播放器
└── config/
    └── recorder_config.yaml
```

## 核心功能

### 录制管理

```cpp
class Recorder {
public:
    void StartRecording(const std::string& room_id);
    void StopRecording(const std::string& room_id);
    void AddStream(const std::string& room_id, MediaStream* stream);
    
private:
    std::map<std::string, std::unique_ptr<RecordingSession>> sessions_;
};
```

### HLS切片

```cpp
class HLSSegmenter {
public:
    void Init(const std::string& output_dir, int segment_duration);
    void ProcessFrame(const AVFrame* frame);
    void FinalizeSegment();
    
    // 生成m3u8
    void UpdatePlaylist();
    
private:
    int segment_duration_;  // 秒
    int current_segment_duration_;
    int sequence_number_;
};
```

### 时移回放

```cpp
class TimeshiftBuffer {
public:
    void WriteSegment(const HlsSegment& segment);
    std::vector<HlsSegment> GetPlaylist(int from_seconds_ago);
    
private:
    std::deque<HlsSegment> buffer_;
    size_t max_buffer_size_;  // 保留N个切片
};
```

## HLS播放

```html
<!-- player.html -->
<video id="player" controls></video>
<script src="https://cdn.jsdelivr.net/npm/hls.js@latest"></script>
<script>
const video = document.getElementById('player');
const hls = new Hls();
hls.loadSource('http://server/live/room123.m3u8');
hls.attachMedia(video);
</script>
```

## 运行

```bash
./mcu_recorder -c config/recorder_config.yaml
```

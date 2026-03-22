# 项目实战1：完整本地播放器

> **前置要求**：完成 Chapter 1-3
> **目标**：整合前3章内容，实现功能完整的本地播放器

## 项目概述

本项目将前3章的知识整合为一个**功能完整的本地视频播放器**，具备以下特性：

- 支持常见格式：MP4、MKV、AVI、FLV
- 播放控制：暂停/继续、进度显示
- 信息显示：分辨率、码率、帧率
- 完善的错误处理
- 内存安全（无泄漏）

## 功能需求

### 基础播放
- [x] 支持本地文件播放
- [x] 自动检测视频格式
- [x] 正确显示视频分辨率

### 播放控制
- [x] 空格键：暂停/继续
- [x] 显示当前播放进度（百分比）
- [x] 显示当前帧率

### 信息显示
- [x] 窗口标题显示文件名
- [x] 控制台输出视频信息（分辨率、码率、时长）
- [x] 实时帧率统计

### 错误处理
- [x] 文件不存在检测
- [x] 不支持的格式提示
- [x] 解码失败优雅退出

## 项目结构

```
project-01/
├── CMakeLists.txt
├── README.md
├── include/
│   └── live/
│       ├── idemuxer.h
│       ├── idecoder.h
│       ├── irenderer.h
│       ├── player.h
│       └── raii_utils.h
└── src/
    ├── main.cpp
    ├── player.cpp
    ├── ffmpeg_demuxer.cpp
    ├── ffmpeg_decoder.cpp
    └── sdl_renderer.cpp
```

## 核心设计

### Player 类

```cpp
class Player {
public:
    bool Init(const char* url);
    void Play();
    void Pause();
    void Stop();
    void TogglePause();  // 空格键控制
    bool IsPlaying() const;
    
    // 信息显示
    float GetProgress() const;  // 0.0 - 100.0
    float GetFPS() const;
    
private:
    std::unique_ptr<IDemuxer> demuxer_;
    std::unique_ptr<IDecoder> decoder_;
    std::unique_ptr<IRenderer> renderer_;
    
    bool is_playing_ = false;
    bool is_paused_ = false;
    int64_t duration_us_ = 0;
    int64_t current_pts_ = 0;
    
    // 帧率统计
    int frame_count_ = 0;
    int64_t fps_start_time_ = 0;
    float current_fps_ = 0;
};
```

## 关键代码片段

### 暂停/继续实现

```cpp
void Player::TogglePause() {
    is_paused_ = !is_paused_;
    if (is_paused_) {
        pause_start_time_ = av_gettime();
    } else {
        // 补偿暂停时间
        total_pause_duration_ += av_gettime() - pause_start_time_;
    }
}

// 同步时考虑暂停时间
int64_t Player::GetAdjustedTime() {
    return av_gettime() - start_time_ - total_pause_duration_;
}
```

### 帧率统计

```cpp
void Player::UpdateFPS() {
    frame_count_++;
    int64_t now = av_gettime();
    
    if (now - fps_start_time_ >= 1000000) {  // 每秒更新
        current_fps_ = frame_count_ * 1000000.0f / (now - fps_start_time_);
        frame_count_ = 0;
        fps_start_time_ = now;
        
        // 更新窗口标题
        UpdateWindowTitle();
    }
}
```

## 完整实现

见 `src/` 目录下的完整代码：
- `player.h/cpp` - 播放器核心逻辑
- `ffmpeg_*.cpp` - FFmpeg 封装
- `sdl_renderer.cpp` - SDL 渲染

## 编译运行

```bash
mkdir build && cd build
cmake ..
make
./player ../test.mp4
```

## 使用说明

| 按键 | 功能 |
|:---|:---|
| 空格 | 暂停/继续 |
| ESC | 退出 |

## 验收标准

- [ ] 能流畅播放 1080p 视频
- [ ] 播放 1 小时无内存增长（valgrind 验证）
- [ ] 错误输入有友好提示
- [ ] 帧率显示准确

## 扩展挑战

1. 添加进度条拖动（需重新定位到指定时间）
2. 添加音量控制
3. 添加全屏切换（F11）

---

**完成本项目后，你将掌握：**
- 模块化播放器架构
- 播放状态管理
- 实时统计信息更新
- 完整的错误处理流程

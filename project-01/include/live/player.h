#pragma once
#include <memory>
#include <string>

namespace live {

class IDemuxer;
class IDecoder;
class IRenderer;

class Player {
public:
    Player();
    ~Player();
    
    // 初始化和释放
    bool Init(const char* url);
    void Shutdown();
    
    // 播放控制
    void Play();
    void Pause();
    void Resume();
    void Stop();
    void TogglePause();
    
    // 状态查询
    bool IsPlaying() const { return is_playing_; }
    bool IsPaused() const { return is_paused_; }
    float GetProgress() const;
    float GetFPS() const { return current_fps_; }
    
private:
    void Run();
    void UpdateFPS();
    void UpdateWindowTitle();
    int64_t GetAdjustedTime() const;
    
    std::unique_ptr<IDemuxer> demuxer_;
    std::unique_ptr<IDecoder> decoder_;
    std::unique_ptr<IRenderer> renderer_;
    
    bool is_playing_ = false;
    bool is_paused_ = false;
    bool should_stop_ = false;
    
    int64_t start_time_ = 0;
    int64_t pause_start_time_ = 0;
    int64_t total_pause_duration_ = 0;
    int64_t duration_us_ = 0;
    
    // FPS 统计
    int frame_count_ = 0;
    int64_t fps_start_time_ = 0;
    float current_fps_ = 0;
    
    std::string url_;
    std::string title_;
};

} // namespace live

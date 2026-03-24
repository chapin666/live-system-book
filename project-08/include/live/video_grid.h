#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <map>

namespace live {

// 视频窗口
struct VideoSlot {
    std::string peer_id;
    SDL_Texture* texture = nullptr;
    SDL_Rect rect;
    int width = 0;
    int height = 0;
    bool active = false;
};

// 视频网格渲染器
class VideoGrid {
public:
    VideoGrid();
    ~VideoGrid();
    
    // 初始化窗口
    bool Initialize(int window_width, int window_height);
    void Shutdown();
    
    // 添加/删除视频槽
    bool AddSlot(const std::string& peer_id);
    void RemoveSlot(const std::string& peer_id);
    
    // 更新视频帧
    void UpdateFrame(const std::string& peer_id,
                     const uint8_t* yuv_data,
                     int width, int height);
    
    // 渲染
    void Render();
    
    // 处理事件
    bool PollEvents();
    
    // 获取布局信息
    int GetSlotCount() const;
    std::vector<std::string> GetActivePeers() const;
    
private:
    void RecalculateLayout();
    
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int window_width_ = 1280;
    int window_height_ = 720;
    
    std::map<std::string, VideoSlot> slots_;
};

} // namespace live
#pragma once

#include "base/pipeline.h"

// SDL 前向声明（避免暴露 SDL 细节）
struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace live {

// 渲染器：YUV -> 屏幕
class Renderer {
public:
    Renderer();
    ~Renderer();
    
    // 禁止拷贝
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    
    // 初始化（创建窗口）
    ErrorCode Init(int width, int height);
    
    // 渲染一帧
    ErrorCode Render(const VideoFrame& frame);
    
    // 处理窗口事件（返回 false 表示用户关闭窗口）
    bool PollEvents();

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

} // namespace live

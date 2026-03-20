#include "renderer.h"
#include <SDL2/SDL.h>
#include <stdio>

namespace live {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

ErrorCode Renderer::Init(int width, int height) {
    width_ = width;
    height_ = height;
    
    // 初始化 SDL 视频子系统
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[Renderer] SDL_Init failed: %s\n", SDL_GetError());
        return ErrorCode::InitFailed;
    }
    
    // 创建窗口
    window_ = SDL_CreateWindow(
        "Live Player - Chapter 01",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window_) {
        printf("[Renderer] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return ErrorCode::InitFailed;
    }
    
    // 创建渲染器（硬件加速 + 垂直同步）
    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!renderer_) {
        printf("[Renderer] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return ErrorCode::InitFailed;
    }
    
    // 创建 YUV 纹理
    // SDL_PIXELFORMAT_YV12: Y + V + U（注意 VU 顺序）
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    
    if (!texture_) {
        printf("[Renderer] SDL_CreateTexture failed: %s\n", SDL_GetError());
        return ErrorCode::InitFailed;
    }
    
    printf("[Renderer] Initialized: %dx%d\n", width, height);
    initialized_ = true;
    return ErrorCode::OK;
}

ErrorCode Renderer::Render(const VideoFrame& frame) {
    if (!initialized_) {
        return ErrorCode::RenderFailed;
    }
    
    // 更新 YUV 纹理
    // 注意：FFmpeg 的 AVFrame 是 YUV420P（Y, U, V 分开）
    // SDL 的 YV12 是 Y, V, U（V 和 U 顺序相反）
    int ret = SDL_UpdateYUVTexture(
        texture_,
        nullptr,  // 更新整个纹理
        frame.data[0],      // Y 平面
        frame.linesize[0],  // Y pitch
        frame.data[2],      // V 平面（注意：YUV420P 的 V 对应 YV12 的 V）
        frame.linesize[2],  // V pitch
        frame.data[1],      // U 平面（注意：YUV420P 的 U 对应 YV12 的 U）
        frame.linesize[1]   // U pitch
    );
    
    if (ret != 0) {
        printf("[Renderer] SDL_UpdateYUVTexture failed: %s\n", SDL_GetError());
        return ErrorCode::RenderFailed;
    }
    
    // 渲染
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
    
    return ErrorCode::OK;
}

bool Renderer::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;  // 用户关闭窗口
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                return false;  // 按 ESC 退出
            }
        }
    }
    return true;
}

} // namespace live

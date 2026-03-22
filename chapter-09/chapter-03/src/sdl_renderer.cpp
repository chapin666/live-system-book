#include "live/sdl_renderer.h"
#include <cstdio>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace live {

SDLRenderer::SDLRenderer() = default;

SDLRenderer::~SDLRenderer() {
    Close();
}

ErrorCode SDLRenderer::Init(int width, int height, 
                             const std::string& title) {
    width_ = width;
    height_ = height;
    
    // 1. 初始化SDL视频子系统
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("[SDLRenderer] SDL_Init failed: %s\n", SDL_GetError());
        return ErrorCode::OpenFailed;
    }
    
    // 2. 创建窗口
    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!window_) {
        printf("[SDLRenderer] CreateWindow failed: %s\n", SDL_GetError());
        return ErrorCode::OpenFailed;
    }
    
    // 3. 创建渲染器（硬件加速 + 垂直同步）
    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (!renderer_) {
        printf("[SDLRenderer] CreateRenderer failed: %s\n", SDL_GetError());
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer_) {
            return ErrorCode::OpenFailed;
        }
    }
    
    // 4. 创建YUV纹理
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    
    if (!texture_) {
        printf("[SDLRenderer] CreateTexture failed: %s\n", SDL_GetError());
        return ErrorCode::OpenFailed;
    }
    
    printf("[SDLRenderer] Initialized: %dx%d\n", width, height);
    initialized_ = true;
    return ErrorCode::OK;
}

ErrorCode SDLRenderer::RenderFrame(const AVFrame* frame) {
    if (!initialized_ || !frame) {
        return ErrorCode::ReadError;
    }
    
    // 更新YUV纹理
    int ret = SDL_UpdateYUVTexture(
        texture_,
        nullptr,
        frame->data[0],      // Y平面
        frame->linesize[0],  // Y stride
        frame->data[1],      // U平面
        frame->linesize[1],  // U stride
        frame->data[2],      // V平面
        frame->linesize[2]   // V stride
    );
    
    if (ret != 0) {
        printf("[SDLRenderer] UpdateYUVTexture failed: %s\n", SDL_GetError());
        return ErrorCode::ReadError;
    }
    
    // 清除画布并复制纹理
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    
    return ErrorCode::OK;
}

bool SDLRenderer::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                return false;
            }
        }
    }
    return true;
}

void SDLRenderer::Present() {
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
}

void SDLRenderer::Close() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
    initialized_ = false;
    printf("[SDLRenderer] Closed\n");
}

} // namespace live

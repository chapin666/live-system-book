#include "renderer.h"
#include <cstring>

namespace live {

Renderer::Renderer() = default;

Renderer::~Renderer() {
    Close();
}

bool Renderer::Init(int width, int height, const char* title) {
    Close();

    width_ = width;
    height_ = height;

    // 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        error_ = "SDL 初始化失败: " + std::string(SDL_GetError());
        return false;
    }

    // 创建窗口
    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,  // 居中显示
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE  // 可调整大小
    );
    if (!window_) {
        error_ = "创建窗口失败: " + std::string(SDL_GetError());
        Close();
        return false;
    }

    // 创建渲染器
    renderer_ = SDL_CreateRenderer(
        window_,
        -1,  // 自动选择驱动
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC  // 硬件加速 + 垂直同步
    );
    if (!renderer_) {
        error_ = "创建渲染器失败: " + std::string(SDL_GetError());
        Close();
        return false;
    }

    // 创建 YUV 纹理
    // SDL_PIXELFORMAT_YV12 对应 YUV420P（注意 V 和 U 顺序）
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,  // 频繁更新
        width,
        height
    );
    if (!texture_) {
        error_ = "创建纹理失败: " + std::string(SDL_GetError());
        Close();
        return false;
    }

    printf("[Renderer] 初始化完成: %dx%d\n", width, height);
    initialized_ = true;
    return true;
}

void Renderer::Close() {
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
    width_ = 0;
    height_ = 0;
}

bool Renderer::RenderFrame(const AVFrame* frame) {
    if (!initialized_ || !texture_ || !renderer_) {
        error_ = "渲染器未初始化";
        return false;
    }

    // 更新 YUV 纹理
    // 注意：FFmpeg 的 AVFrame 数据布局：
    // data[0] = Y 平面, linesize[0] = Y 行大小
    // data[1] = U 平面, linesize[1] = U 行大小 (YUV420P)
    // data[2] = V 平面, linesize[2] = V 行大小 (YUV420P)
    //
    // SDL 的 YV12 格式：
    // Y 平面, V 平面, U 平面（V 和 U 顺序与 YUV420P 相反）
    
    int ret = SDL_UpdateYUVTexture(
        texture_,
        nullptr,  // 更新整个纹理
        frame->data[0],      // Y 平面
        frame->linesize[0],  // Y pitch
        frame->data[1],      // U 平面（YUV420P 的 U 对应 YV12 的 U）
        frame->linesize[1],  // U pitch
        frame->data[2],      // V 平面（YUV420P 的 V 对应 YV12 的 V）
        frame->linesize[2]   // V pitch
    );

    if (ret != 0) {
        error_ = "更新纹理失败: " + std::string(SDL_GetError());
        return false;
    }

    // 清除渲染目标
    SDL_RenderClear(renderer_);

    // 复制纹理到渲染器
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);

    // 显示到屏幕（双缓冲交换）
    SDL_RenderPresent(renderer_);

    return true;
}

bool Renderer::PollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            // 用户点击关闭按钮
            return false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                // 按 ESC 退出
                return false;
            }
        }
    }
    return true;
}

} // namespace live

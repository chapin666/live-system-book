#include "live/irenderer.h"
#include <SDL2/SDL.h>
#include <iostream>

extern "C" {
#include <libavutil/frame.h>
}

namespace live {

class SDLRenderer : public IRenderer {
public:
    SDLRenderer() = default;
    ~SDLRenderer() { Close(); }
    
    bool Init(int width, int height, const char* title) override {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL 初始化失败" << std::endl;
            return false;
        }
        
        window_ = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height, SDL_WINDOW_SHOWN);
        if (!window_) return false;
        
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        texture_ = SDL_CreateTexture(renderer_,
            SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
            width, height);
        
        width_ = width;
        height_ = height;
        return true;
    }
    
    bool RenderFrame(const AVFrame* frame) override {
        SDL_UpdateYUVTexture(texture_, nullptr,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
        
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
        return true;
    }
    
    bool PollEvents() override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
        }
        return true;
    }
    
    void Close() override {
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
        texture_ = nullptr;
        renderer_ = nullptr;
        window_ = nullptr;
    }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0, height_ = 0;
};

std::unique_ptr<IRenderer> CreateSDLRenderer() {
    return std::make_unique<SDLRenderer>();
}

} // namespace live

#include "live/irenderer.h"
#include <SDL2/SDL.h>
#include <iostream>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace live {

class SDLRenderer : public IRenderer {
public:
    SDLRenderer() = default;
    ~SDLRenderer() { Close(); }
    
    bool Init(int width, int height, const char* title) override {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        window_ = SDL_CreateWindow(title,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   width, height,
                                   SDL_WINDOW_SHOWN);
        if (!window_) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer_) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        texture_ = SDL_CreateTexture(renderer_,
                                     SDL_PIXELFORMAT_YV12,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
        if (!texture_) {
            std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        width_ = width;
        height_ = height;
        return true;
    }
    
    void Close() override {
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
    }
    
    void RenderFrame(void* frame) override {
        AVFrame* avframe = static_cast<AVFrame*>(frame);
        
        // 更新YUV纹理
        SDL_UpdateYUVTexture(texture_, nullptr,
                             avframe->data[0], avframe->linesize[0],
                             avframe->data[1], avframe->linesize[1],
                             avframe->data[2], avframe->linesize[2]);
        
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }
    
    bool PollEvents() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    return false;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    // 空格键暂停/继续 - 由Player处理
                }
            }
        }
        return true;
    }
    
    void SetTitle(const char* title) override {
        if (window_) {
            SDL_SetWindowTitle(window_, title);
        }
    }
    
private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
};

std::unique_ptr<IRenderer> CreateSDLRenderer() {
    return std::make_unique<SDLRenderer>();
}

} // namespace live
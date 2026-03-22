#pragma once

#include "live/irenderer.h"
#include <SDL2/SDL.h>

namespace live {

class SDLRenderer : public IRenderer {
public:
    SDLRenderer();
    ~SDLRenderer() override;
    
    SDLRenderer(const SDLRenderer&) = delete;
    SDLRenderer& operator=(const SDLRenderer&) = delete;

    ErrorCode Init(int width, int height, 
                   const std::string& title) override;
    ErrorCode RenderFrame(const AVFrame* frame) override;
    bool PollEvents() override;
    void Present() override;
    void Close() override;

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};

} // namespace live

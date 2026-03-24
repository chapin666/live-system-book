#include "live/video_grid.h"
#include <iostream>
#include <cstring>
#include <math>

namespace live {

VideoGrid::VideoGrid() = default;
VideoGrid::~VideoGrid() { Shutdown(); }

bool VideoGrid::Initialize(int window_width, int window_height) {
    window_width_ = window_width;
    window_height_ = window_height;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL 初始化失败: " << SDL_GetError() << std::endl;
        return false;
    }
    
    window_ = SDL_CreateWindow("多人会议",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               window_width_, window_height_,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "创建窗口失败: " << SDL_GetError() << std::endl;
        return false;
    }
    
    renderer_ = SDL_CreateRenderer(window_, -1, 
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "创建渲染器失败: " << SDL_GetError() << std::endl;
        return false;
    }
    
    return true;
}

void VideoGrid::Shutdown() {
    for (auto& pair : slots_) {
        if (pair.second.texture) {
            SDL_DestroyTexture(pair.second.texture);
        }
    }
    slots_.clear();
    
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

bool VideoGrid::AddSlot(const std::string& peer_id) {
    VideoSlot slot;
    slot.peer_id = peer_id;
    slot.active = true;
    
    // 创建纹理
    slot.texture = SDL_CreateTexture(renderer_,
                                     SDL_PIXELFORMAT_IYUV,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     640, 480);
    if (!slot.texture) {
        std::cerr << "创建纹理失败: " << SDL_GetError() << std::endl;
        return false;
    }
    
    slots_[peer_id] = slot;
    RecalculateLayout();
    
    std::cout << "[视频网格] 添加 " << peer_id << "，当前 " << slots_.size() << " 路视频" << std::endl;
    return true;
}

void VideoGrid::RemoveSlot(const std::string& peer_id) {
    auto it = slots_.find(peer_id);
    if (it != slots_.end()) {
        if (it->second.texture) {
            SDL_DestroyTexture(it->second.texture);
        }
        slots_.erase(it);
        RecalculateLayout();
        std::cout << "[视频网格] 移除 " << peer_id << std::endl;
    }
}

void VideoGrid::UpdateFrame(const std::string& peer_id,
                            const uint8_t* yuv_data,
                            int width, int height) {
    auto it = slots_.find(peer_id);
    if (it == slots_.end()) return;
    
    VideoSlot& slot = it->second;
    
    // 如果分辨率变化，重新创建纹理
    if (width != slot.width || height != slot.height) {
        if (slot.texture) {
            SDL_DestroyTexture(slot.texture);
        }
        slot.texture = SDL_CreateTexture(renderer_,
                                         SDL_PIXELFORMAT_IYUV,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         width, height);
        slot.width = width;
        slot.height = height;
    }
    
    // 更新纹理 (假设 YUV420P 格式)
    int y_size = width * height;
    int uv_size = y_size / 4;
    
    SDL_UpdateYUVTexture(slot.texture, nullptr,
                         yuv_data, width,                    // Y
                         yuv_data + y_size, width / 2,       // U
                         yuv_data + y_size + uv_size, width / 2);  // V
}

void VideoGrid::RecalculateLayout() {
    int count = slots_.size();
    if (count == 0) return;
    
    // 计算网格布局
    int cols = (int)ceil(sqrt(count));
    int rows = (int)ceil((double)count / cols);
    
    int slot_width = window_width_ / cols;
    int slot_height = window_height_ / rows;
    
    int index = 0;
    for (auto& pair : slots_) {
        VideoSlot& slot = pair.second;
        int col = index % cols;
        int row = index / cols;
        
        slot.rect.x = col * slot_width;
        slot.rect.y = row * slot_height;
        slot.rect.w = slot_width;
        slot.rect.h = slot_height;
        
        index++;
    }
}

void VideoGrid::Render() {
    SDL_SetRenderDrawColor(renderer_, 32, 32, 32, 255);
    SDL_RenderClear(renderer_);
    
    // 渲染每个视频槽
    for (const auto& pair : slots_) {
        const VideoSlot& slot = pair.second;
        if (slot.texture && slot.active) {
            SDL_RenderCopy(renderer_, slot.texture, nullptr, &slot.rect);
        }
    }
    
    SDL_RenderPresent(renderer_);
}

bool VideoGrid::PollEvents() {
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
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                window_width_ = event.window.data1;
                window_height_ = event.window.data2;
                RecalculateLayout();
            }
        }
    }
    return true;
}

int VideoGrid::GetSlotCount() const {
    return (int)slots_.size();
}

std::vector<std::string> VideoGrid::GetActivePeers() const {
    std::vector<std::string> peers;
    for (const auto& pair : slots_) {
        if (pair.second.active) {
            peers.push_back(pair.first);
        }
    }
    return peers;
}

} // namespace live
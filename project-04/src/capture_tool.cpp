#include "live/capturer.h"
#include <iostream>
#include <string>
#include <SDL2/SDL.h>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

namespace live {

// 视频采集实现
class VideoCapturer : public ICapturer {
public:
    bool Init(int device_id, int width, int height, int fps) override {
        // 注册设备
        avdevice_register_all();
        
        // 根据平台选择输入格式
        const AVInputFormat* input_format = nullptr;
        const char* device_name = nullptr;
        
#ifdef __APPLE__
        input_format = av_find_input_format("avfoundation");
        device_name = (device_id == 0) ? "0" : "1";
#elif defined(__linux__)
        input_format = av_find_input_format("v4l2");
        device_name = "/dev/video0";
#else
        input_format = av_find_input_format("dshow");
        device_name = "video=Camera";
#endif
        
        AVFormatContext* fmt_ctx = nullptr;
        AVDictionary* options = nullptr;
        
        char resolution[64];
        snprintf(resolution, sizeof(resolution), "%dx%d", width, height);
        av_dict_set(&options, "video_size", resolution, 0);
        av_dict_set(&options, "framerate", "30", 0);
        
        if (avformat_open_input(&fmt_ctx, device_name, input_format, &options) < 0) {
            std::cerr << "Failed to open camera" << std::endl;
            av_dict_free(&options);
            return false;
        }
        
        av_dict_free(&options);
        fmt_ctx_ = fmt_ctx;
        
        width_ = width;
        height_ = height;
        return true;
    }
    
    void Close() override {
        if (fmt_ctx_) {
            avformat_close_input(&fmt_ctx_);
        }
        running_ = false;
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
    }
    
    bool Start() override {
        running_ = true;
        capture_thread_ = std::thread([this]() {
            CaptureLoop();
        });
        return true;
    }
    
    void Stop() override {
        running_ = false;
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
    }
    
private:
    void CaptureLoop() {
        AVPacket packet;
        while (running_) {
            if (av_read_frame(fmt_ctx_, &packet) >= 0) {
                // 这里简化处理，实际需解码为YUV/RGB
                av_packet_unref(&packet);
            }
        }
    }
    
    AVFormatContext* fmt_ctx_ = nullptr;
    std::thread capture_thread_;
    bool running_ = false;
    int width_ = 0;
    int height_ = 0;
};

std::unique_ptr<ICapturer> CreateVideoCapturer() {
    return std::make_unique<VideoCapturer>();
}

// 预览窗口实现
class SDLPreview : public IPreview {
public:
    bool Init(int width, int height) override {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
        
        window_ = SDL_CreateWindow("Capture Preview",
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   width, height,
                                   SDL_WINDOW_SHOWN);
        if (!window_) return false;
        
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
        return true;
    }
    
    void Close() override {
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }
    
    void Render(const uint8_t* data, int width, int height) override {
        SDL_UpdateTexture(texture_, nullptr, data, width * 3);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }
    
    bool PollEvents() override {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return false;
        }
        return true;
    }
    
private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};

std::unique_ptr<IPreview> CreatePreview() {
    return std::make_unique<SDLPreview>();
}

} // namespace live
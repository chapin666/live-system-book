#include "live/frame_queue.h"
#include "decoder_thread.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <chrono>

extern "C" {
#include <libavutil/time.h>
}

using namespace live;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <视频文件>" << std::endl;
        return 1;
    }

    // 1. 启动解码线程
    DecoderConfig config;
    config.filename = argv[1];
    config.queue_max_size = 3;  // 3帧缓冲

    DecoderThread decoder(config);
    if (!decoder.Start()) {
        return 1;
    }

    // 2. 初始化 SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL 初始化失败: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Live Player - Chapter 02 (Async)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        decoder.GetWidth(), decoder.GetHeight(),
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        decoder.GetWidth(), decoder.GetHeight());

    // 3. 渲染循环
    int64_t start_time = av_gettime();
    int rendered_frames = 0;
    bool quit = false;

    std::cout << "[Main] Starting render loop" << std::endl;

    while (!quit) {
        // 处理事件（非阻塞）
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    quit = true;
                }
            }
        }

        // 从队列取帧
        AVFrame* frame = decoder.GetFrameQueue()->Pop(false);  // 非阻塞
        
        if (frame) {
            // PTS 同步
            int64_t pts_us = frame->pts * av_q2d(decoder.GetTimeBase()) * 1000000;
            int64_t elapsed = av_gettime() - start_time;
            int64_t delay = pts_us - elapsed;

            if (delay > 0 && delay < 1000000) {  // 合理的延迟范围
                SDL_Delay(delay / 1000);  // 毫秒
            }

            // 渲染
            SDL_UpdateYUVTexture(
                texture, nullptr,
                frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);

            rendered_frames++;
            av_frame_free(&frame);
        } else if (!decoder.IsRunning() && decoder.GetFrameQueue()->Empty()) {
            // 解码结束且队列为空
            break;
        } else {
            // 队列为空，等待一帧
            SDL_Delay(1);
        }
    }

    // 4. 清理
    std::cout << "[Main] Rendered " << rendered_frames << " frames" << std::endl;

    decoder.Stop();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "[Main] Exit" << std::endl;
    return 0;
}

// artificial_lag_demo.cpp - 故意制造卡顿的实验
// 第四章：为什么卡顿？性能分析

#include <SDL2/SDL.h>
#include <stdio.h>
#include <unistd.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

// 模拟耗时操作
void simulate_expensive_operation(int ms) {
    usleep(ms * 1000);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件> [延迟毫秒]\n", argv[0]);
        return 1;
    }
    
    int artificial_lag_ms = (argc >= 3) ? atoi(argv[2]) : 50;
    printf("故意添加延迟: %d ms\n", artificial_lag_ms);
    
    // 打开文件
    AVFormatContext* fmt_ctx = nullptr;
    avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    avformat_find_stream_info(fmt_ctx, nullptr);
    
    int video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* st = fmt_ctx->streams[video_idx];
    
    const AVCodec* codec = avcodec_find_decoder(st->ccodecpar->codec_id);
    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, st->ccodecpar);
    avcodec_open2(ctx, codec, nullptr);
    
    // 初始化 SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("卡顿实验",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        ctx->width, ctx->height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        ctx->width, ctx->height);
    
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    int64_t start_time = av_gettime();
    int frame_count = 0;
    bool quit = false;
    
    printf("播放中... 观察窗口拖动时的卡顿\n");
    printf("按 ESC 退出\n");
    
    while (!quit && av_read_frame(fmt_ctx, pkt) >= 0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = true;
        }
        
        if (pkt->stream_index != video_idx) {
            av_packet_unref(pkt);
            continue;
        }
        
        // ===== 故意添加延迟 =====
        simulate_expensive_operation(artificial_lag_ms);
        
        avcodec_send_packet(ctx, pkt);
        av_packet_unref(pkt);
        
        while (avcodec_receive_frame(ctx, frame) == 0) {
            // 同步
            int64_t pts_us = frame->pts * av_q2d(st->time_base) * 1000000;
            int64_t elapsed = av_gettime() - start_time;
            if (pts_us > elapsed) av_usleep(pts_us - elapsed);
            
            // 渲染
            SDL_UpdateYUVTexture(texture, nullptr,
                frame->data[0], frame->linesize[0],
                frame->data[1], frame->linesize[1],
                frame->data[2], frame->linesize[2]);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
            
            frame_count++;
        }
    }
    
    printf("播放完成，共 %d 帧\n", frame_count);
    
    // 清理
    av_frame_free(&frame);
    av_packet_free(&pkt);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    avcodec_free_context(&ctx);
    avformat_close_input(&fmt_ctx);
    
    return 0;
}

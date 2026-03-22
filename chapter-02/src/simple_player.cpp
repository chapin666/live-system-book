// simple_player.cpp - 100行核心播放器代码
// 第二章：第一个播放器

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件>\n", argv[0]);
        return 1;
    }

    // ========== 1. 打开输入文件 ==========
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        fprintf(stderr, "无法打开文件\n");
        return 1;
    }
    
    avformat_find_stream_info(fmt_ctx, nullptr);

    // ========== 2. 查找视频流 ==========
    int video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_idx < 0) {
        fprintf(stderr, "未找到视频流\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    AVStream* video_stream = fmt_ctx->streams[video_idx];

    printf("视频: %dx%d\n", 
           video_stream->ccodecpar->width,
           video_stream->ccodecpar->height);

    // ========== 3. 初始化解码器 ==========
    const AVCodec* codec = avcodec_find_decoder(video_stream->ccodecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, video_stream->ccodecpar);
    avcodec_open2(codec_ctx, codec, nullptr);

    int width = codec_ctx->width;
    int height = codec_ctx->height;

    // ========== 4. 初始化 SDL2 ==========
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow(
        "第二章：第一个播放器",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING, width, height);

    // ========== 5. 解码循环 ==========
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    int64_t start_time = av_gettime();
    bool quit = false;

    while (!quit && av_read_frame(fmt_ctx, packet) >= 0) {
        // 处理退出事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
        }

        if (packet->stream_index != video_idx) {
            av_packet_unref(packet);
            continue;
        }

        // 解码
        avcodec_send_packet(codec_ctx, packet);
        av_packet_unref(packet);

        while (avcodec_receive_frame(codec_ctx, frame) == 0) {
            // 同步：根据 PTS 控制播放速度
            int64_t pts_us = frame->pts * av_q2d(video_stream->time_base) * 1000000;
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
        }
    }

    // ========== 6. 清理资源 ==========
    av_frame_free(&frame);
    av_packet_free(&packet);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return 0;
}

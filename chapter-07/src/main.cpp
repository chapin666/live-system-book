#include "live/ring_buffer.h"
#include "live/download_thread.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

// SDL 渲染上下文
struct RenderContext {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

// 初始化 SDL
bool InitSDL(RenderContext& ctx, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    ctx.window = SDL_CreateWindow(
        "Network Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!ctx.window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    ctx.renderer = SDL_CreateRenderer(ctx.window, -1, SDL_RENDERER_ACCELERATED);
    if (!ctx.renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    ctx.width = width;
    ctx.height = height;
    return true;
}

// 创建或更新纹理
void EnsureTexture(RenderContext& ctx, int width, int height) {
    if (!ctx.texture || ctx.width != width || ctx.height != height) {
        if (ctx.texture) {
            SDL_DestroyTexture(ctx.texture);
        }
        ctx.texture = SDL_CreateTexture(
            ctx.renderer,
            SDL_PIXELFORMAT_IYUV,
            SDL_TEXTUREACCESS_STREAMING,
            width, height
        );
        ctx.width = width;
        ctx.height = height;
    }
}

// 清理 SDL
void CleanupSDL(RenderContext& ctx) {
    if (ctx.texture) SDL_DestroyTexture(ctx.texture);
    if (ctx.renderer) SDL_DestroyRenderer(ctx.renderer);
    if (ctx.window) SDL_DestroyWindow(ctx.window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <URL>" << std::endl;
        return 1;
    }

    const char* url = argv[1];
    
    // 1. 创建环形缓冲区 (4MB)
    live::RingBuffer ring_buffer(4 * 1024 * 1024);
    
    // 2. 启动下载线程
    live::DownloadConfig config;
    config.url = url;
    config.connect_timeout = 10;
    
    live::DownloadThread downloader(&ring_buffer, config);
    if (!downloader.Start()) {
        return 1;
    }
    
    // 3. 等待缓冲足够数据（预缓冲）
    std::cout << "[Main] Buffering..." << std::endl;
    while (ring_buffer.Size() < 1024 * 1024 && downloader.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "[Main] Starting playback" << std::endl;
    
    // 4. 使用自定义 IO 打开流
    AVFormatContext* fmt_ctx = avformat_alloc_context();
    
    int buffer_size = 32768;
    uint8_t* avio_buffer = static_cast<uint8_t*>(av_malloc(buffer_size));
    AVIOContext* avio = avio_alloc_context(
        avio_buffer, buffer_size, 0, &ring_buffer,
        live::RingBuffer::ReadCallback, nullptr, nullptr);
    
    fmt_ctx->pb = avio;
    
    if (avformat_open_input(&fmt_ctx, nullptr, nullptr, nullptr) < 0) {
        std::cerr << "Failed to open input" << std::endl;
        downloader.Stop();
        return 1;
    }
    
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "Failed to find stream info" << std::endl;
        avformat_close_input(&fmt_ctx);
        downloader.Stop();
        return 1;
    }
    
    // 查找视频流
    int video_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_idx < 0) {
        std::cerr << "No video stream found" << std::endl;
        avformat_close_input(&fmt_ctx);
        downloader.Stop();
        return 1;
    }
    
    AVStream* stream = fmt_ctx->streams[video_idx];
    
    // 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);
    
    // 初始化 SDL
    RenderContext render_ctx;
    if (!InitSDL(render_ctx, codec_ctx->width, codec_ctx->height)) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        downloader.Stop();
        return 1;
    }
    
    // 初始化图像转换
    struct SwsContext* sws_ctx = sws_getContext(
        codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
        codec_ctx->width, codec_ctx->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    // 分配 YUV420P 帧
    AVFrame* frame_yuv = av_frame_alloc();
    frame_yuv->format = AV_PIX_FMT_YUV420P;
    frame_yuv->width = codec_ctx->width;
    frame_yuv->height = codec_ctx->height;
    av_frame_get_buffer(frame_yuv, 0);
    
    // 主循环
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    bool running = true;
    
    while (running && av_read_frame(fmt_ctx, pkt) >= 0) {
        // 处理 SDL 事件
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        
        if (pkt->stream_index == video_idx) {
            avcodec_send_packet(codec_ctx, pkt);
            
            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                // 转换为 YUV420P
                sws_scale(sws_ctx, frame->data, frame->linesize, 0,
                         codec_ctx->height, frame_yuv->data, frame_yuv->linesize);
                
                // 渲染
                EnsureTexture(render_ctx, codec_ctx->width, codec_ctx->height);
                SDL_UpdateYUVTexture(render_ctx.texture, nullptr,
                    frame_yuv->data[0], frame_yuv->linesize[0],
                    frame_yuv->data[1], frame_yuv->linesize[1],
                    frame_yuv->data[2], frame_yuv->linesize[2]);
                
                SDL_RenderClear(render_ctx.renderer);
                SDL_RenderCopy(render_ctx.renderer, render_ctx.texture, nullptr, nullptr);
                SDL_RenderPresent(render_ctx.renderer);
            }
        }
        
        av_packet_unref(pkt);
    }
    
    // 清理
    running = false;
    downloader.Stop();
    
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_frame_free(&frame_yuv);
    sws_freeContext(sws_ctx);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    av_freep(&avio_buffer);
    
    CleanupSDL(render_ctx);
    
    std::cout << "[Main] Playback finished" << std::endl;
    return 0;
}

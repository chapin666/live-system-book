/**
 * 简单的RTMP播放器 - 使用FFmpeg（最小可运行代码）
 * 
 * 编译: g++ simple_rtmp_player.cpp -o simple_rtmp_player $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale sdl2)
 * 运行: ./simple_rtmp_player rtmp://localhost/live/stream
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/time.h>
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <RTMP URL>\n", argv[0]);
        fprintf(stderr, "示例: %s rtmp://localhost/live/stream\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    
    // 初始化FFmpeg网络
    avformat_network_init();
    
    // 打开输入
    AVFormatContext* fmt_ctx = nullptr;
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "buffer_size", "65536", 0);
    av_dict_set(&opts, "max_delay", "500000", 0);
    
    printf("Connecting to: %s\n", url);
    int ret = avformat_open_input(&fmt_ctx, url, nullptr, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open stream: %s\n", errbuf);
        return 1;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to find stream info\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    av_dump_format(fmt_ctx, 0, url, 0);
    
    // 查找视频流
    int video_stream_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }
    
    if (video_stream_idx == -1) {
        fprintf(stderr, "No video stream found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    AVStream* video_stream = fmt_ctx->streams[video_stream_idx];
    AVCodecParameters* codecpar = video_stream->codecpar;
    
    printf("Video: %dx%d @ %.2f fps\n", 
           codecpar->width, codecpar->height,
           av_q2d(video_stream->avg_frame_rate));
    
    // 初始化解码器
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        fprintf(stderr, "Decoder not found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    avcodec_open2(codec_ctx, decoder, nullptr);
    
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    SDL_Window* window = SDL_CreateWindow(
        "RTMP Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codecpar->width, codecpar->height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        codecpar->width, codecpar->height
    );
    
    // 分配帧
    AVFrame* frame = av_frame_alloc();
    AVPacket* pkt = av_packet_alloc();
    
    bool running = true;
    SDL_Event event;
    
    printf("Starting playback... Press Q to quit\n");
    
    while (running && av_read_frame(fmt_ctx, pkt) >= 0) {
        // 处理SDL事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                running = false;
            }
        }
        
        if (pkt->stream_index == video_stream_idx) {
            // 发送数据到解码器
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) continue;
            
            // 接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) break;
                
                // 更新SDL纹理
                SDL_UpdateYUVTexture(
                    texture, nullptr,
                    frame->data[0], frame->linesize[0],
                    frame->data[1], frame->linesize[1],
                    frame->data[2], frame->linesize[2]
                );
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
                
                // 简单的帧率控制
                SDL_Delay(33); // ~30fps
            }
        }
        
        av_packet_unref(pkt);
    }
    
    // 清理
    av_frame_free(&frame);
    av_packet_free(&pkt);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    
    printf("Playback finished\n");
    return 0;
}

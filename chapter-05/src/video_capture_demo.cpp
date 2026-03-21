/**
 * 摄像头采集演示 - 实时预览
 * 
 * 编译: g++ capture_demo.cpp -o capture_demo $(pkg-config --cflags --libs libavformat libavcodec libavutil libavdevice libswscale sdl2)
 * 运行: ./capture_demo [设备名]
 *   macOS: ./capture_demo "0" 或 ./capture_demo "FaceTime HD Camera"
 *   Linux: ./capture_demo /dev/video0
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

int main(int argc, char* argv[]) {
    // 初始化FFmpeg设备
    avdevice_register_all();
    
    // 选择设备
    const char* device_name = (argc > 1) ? argv[1] : nullptr;
    
    // 检测平台
    const AVInputFormat* input_format = nullptr;
    const char* default_device = nullptr;
    
#if defined(__APPLE__)
    input_format = av_find_input_format("avfoundation");
    default_device = "0";
    printf("Platform: macOS (AVFoundation)\n");
#elif defined(__linux__)
    input_format = av_find_input_format("v4l2");
    default_device = "/dev/video0";
    printf("Platform: Linux (V4L2)\n");
#else
    #error "Unsupported platform"
#endif
    
    const char* device = device_name ? device_name : default_device;
    printf("Opening device: %s\n", device);
    
    // 设置采集参数
    AVDictionary* options = nullptr;
    av_dict_set(&options, "video_size", "1280x720", 0);
    av_dict_set(&options, "framerate", "30", 0);
    av_dict_set(&options, "pixel_format", "nv12", 0);
    
    // 打开设备
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, device, input_format, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open device: %s\n", errbuf);
        return 1;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to find stream info\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    av_dump_format(fmt_ctx, 0, device, 0);
    
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
    
    AVCodecParameters* codecpar = fmt_ctx->streams[video_stream_idx]->codecpar;
    printf("Capture: %dx%d\n", codecpar->width, codecpar->height);
    
    // 初始化解码器（某些设备需要解码）
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = nullptr;
    
    if (decoder) {
        codec_ctx = avcodec_alloc_context3(decoder);
        avcodec_parameters_to_context(codec_ctx, codecpar);
        avcodec_open2(codec_ctx, decoder, nullptr);
    }
    
    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "Camera Capture",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codecpar->width, codecpar->height,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12,
        SDL_TEXTUREACCESS_STREAMING,
        codecpar->width, codecpar->height
    );
    
    // 创建缩放器（如果需要格式转换）
    struct SwsContext* sws_ctx = sws_getContext(
        codecpar->width, codecpar->height, (AVPixelFormat)codecpar->format,
        codecpar->width, codecpar->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->format = AV_PIX_FMT_YUV420P;
    rgb_frame->width = codecpar->width;
    rgb_frame->height = codecpar->height;
    av_frame_get_buffer(rgb_frame, 0);
    
    AVPacket* pkt = av_packet_alloc();
    bool running = true;
    SDL_Event event;
    int frame_count = 0;
    
    printf("Capture started. Press Q to quit\n");
    
    while (running) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                running = false;
            }
        }
        
        // 读取一帧
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            continue;
        }
        
        if (pkt->stream_index == video_stream_idx) {
            if (codec_ctx) {
                // 需要解码
                avcodec_send_packet(codec_ctx, pkt);
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    // 格式转换
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0,
                             codecpar->height, rgb_frame->data, rgb_frame->linesize);
                    
                    // 显示
                    SDL_UpdateYUVTexture(
                        texture, nullptr,
                        rgb_frame->data[0], rgb_frame->linesize[0],
                        rgb_frame->data[1], rgb_frame->linesize[1],
                        rgb_frame->data[2], rgb_frame->linesize[2]
                    );
                    
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_RenderPresent(renderer);
                    
                    frame_count++;
                    if (frame_count % 30 == 0) {
                        printf("Captured %d frames\r", frame_count);
                        fflush(stdout);
                    }
                }
            }
        }
        
        av_packet_unref(pkt);
    }
    
    printf("\nTotal frames captured: %d\n", frame_count);
    
    // 清理
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&pkt);
    sws_freeContext(sws_ctx);
    
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    
    return 0;
}

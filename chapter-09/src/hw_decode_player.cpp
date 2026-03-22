/**
 * 硬件解码播放器 - 自动检测平台并选择最佳方案
 * 
 * 编译: g++ hw_decode_player.cpp -o hw_decode_player $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale sdl2)
 * 运行: ./hw_decode_player <video_file_or_url>
 * 
 * 支持:
 *   - macOS: VideoToolbox
 *   - Linux Intel/AMD: VAAPI
 *   - Linux NVIDIA: NVDEC (通过CUDA)
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
}

// 检测硬件加速类型
enum HWAccelType {
    HW_NONE,
    HW_VIDEOTOOLBOX,  // macOS
    HW_VAAPI,         // Linux Intel/AMD
    HW_NVDEC,         // NVIDIA
    HW_DXVA,          // Windows (备用)
};

HWAccelType DetectHWAccel() {
#if defined(__APPLE__)
    return HW_VIDEOTOOLBOX;
#elif defined(__linux__)
    // 检查NVIDIA GPU
    FILE* fp = popen("lspci | grep -i nvidia", "r");
    if (fp) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), fp)) {
            pclose(fp);
            return HW_NVDEC;
        }
        pclose(fp);
    }
    // 默认尝试VAAPI
    return HW_VAAPI;
#else
    return HW_NONE;
#endif
}

const char* GetHWAccelName(HWAccelType type) {
    switch (type) {
        case HW_VIDEOTOOLBOX: return "VideoToolbox";
        case HW_VAAPI: return "VAAPI";
        case HW_NVDEC: return "NVDEC";
        default: return "Software";
    }
}

AVHWDeviceType GetAVHWDeviceType(HWAccelType type) {
    switch (type) {
        case HW_VIDEOTOOLBOX: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
        case HW_VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
        case HW_NVDEC: return AV_HWDEVICE_TYPE_CUDA;
        default: return AV_HWDEVICE_TYPE_NONE;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件或URL>\n", argv[0]);
        fprintf(stderr, "示例: %s test.mp4\n", argv[0]);
        fprintf(stderr, "      %s rtmp://localhost/live/stream\n", argv[0]);
        return 1;
    }
    
    const char* input = argv[1];
    
    // 检测硬件加速
    HWAccelType hw_type = DetectHWAccel();
    printf("Hardware Decoder: %s\n", GetHWAccelName(hw_type));
    
    // 初始化FFmpeg
    avformat_network_init();
    
    // 打开输入
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, input, nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open input: %s\n", errbuf);
        return 1;
    }
    
    avformat_find_stream_info(fmt_ctx, nullptr);
    av_dump_format(fmt_ctx, 0, input, 0);
    
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
    printf("Video: %dx%d, codec: %d\n", codecpar->width, codecpar->height, codecpar->codec_id);
    
    // 查找解码器
    const AVCodec* decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder) {
        fprintf(stderr, "Decoder not found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    // 创建解码器上下文
    AVCodecContext* codec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    
    // 设置硬件加速
    AVBufferRef* hw_device_ctx = nullptr;
    AVBufferRef* hw_frames_ctx = nullptr;
    bool use_hw = false;
    
    if (hw_type != HW_NONE) {
        AVHWDeviceType device_type = GetAVHWDeviceType(hw_type);
        ret = av_hwdevice_ctx_create(&hw_device_ctx, device_type, nullptr, nullptr, 0);
        
        if (ret >= 0) {
            codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            use_hw = true;
            printf("Hardware acceleration enabled\n");
        } else {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            printf("Failed to create HW device: %s (falling back to software)\n", errbuf);
        }
    }
    
    // 打开解码器
    ret = avcodec_open2(codec_ctx, decoder, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to open decoder\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow(
        "Hardware Decode Player",
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
    AVFrame* sw_frame = av_frame_alloc(); // 用于硬件解码后的软件帧
    AVPacket* pkt = av_packet_alloc();
    
    bool running = true;
    SDL_Event event;
    int frame_count = 0;
    int64_t start_time = av_gettime();
    int hw_frame_count = 0;
    
    printf("Playback started. Press Q to quit\n");
    
    while (running && av_read_frame(fmt_ctx, pkt) >= 0) {
        // 处理事件
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                running = false;
            }
        }
        
        if (pkt->stream_index != video_stream_idx) {
            av_packet_unref(pkt);
            continue;
        }
        
        // 发送包到解码器
        ret = avcodec_send_packet(codec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0) continue;
        
        // 接收帧
        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            if (ret < 0) break;
            
            frame_count++;
            
            // 检查是否为硬件帧
            if (frame->format == AV_PIX_FMT_VIDEOTOOLBOX ||
                frame->format == AV_PIX_FMT_VAAPI ||
                frame->format == AV_PIX_FMT_CUDA) {
                hw_frame_count++;
                
                // 从硬件下载到内存
                ret = av_hwframe_transfer_data(sw_frame, frame, 0);
                if (ret < 0) {
                    fprintf(stderr, "Failed to transfer HW frame\n");
                    continue;
                }
            } else {
                // 已经是软件帧
                av_frame_ref(sw_frame, frame);
            }
            
            // 显示
            if (sw_frame->data[0]) {
                SDL_UpdateYUVTexture(
                    texture, nullptr,
                    sw_frame->data[0], sw_frame->linesize[0],
                    sw_frame->data[1], sw_frame->linesize[1],
                    sw_frame->data[2], sw_frame->linesize[2]
                );
                
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
            }
            
            av_frame_unref(sw_frame);
            
            // 统计帧率
            if (frame_count % 30 == 0) {
                int64_t elapsed = av_gettime() - start_time;
                double fps = frame_count * 1000000.0 / elapsed;
                printf("Frames: %d, FPS: %.2f, HW: %d/%d\r", 
                       frame_count, fps, hw_frame_count, frame_count);
                fflush(stdout);
            }
            
            SDL_Delay(33); // ~30fps
        }
    }
    
    printf("\nTotal frames decoded: %d (HW: %d)\n", frame_count, hw_frame_count);
    
    // 清理
    av_frame_free(&frame);
    av_frame_free(&sw_frame);
    av_packet_free(&pkt);
    
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
    if (hw_frames_ctx) av_buffer_unref(&hw_frames_ctx);
    
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    
    return 0;
}

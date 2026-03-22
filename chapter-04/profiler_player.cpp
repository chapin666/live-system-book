// profiler_player.cpp - 带性能分析的播放器
// 本章示例：测量解码和渲染耗时

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <chrono>
#include <deque>
#include <numeric>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}

// 性能监控器 - 测量解码、渲染耗时
class PerformanceMonitor {
public:
    struct Stats {
        int64_t decode_us;
        int64_t render_us;
        int64_t total_us;
        char frame_type;
    };
    
    void StartDecode() {
        decode_start_ = Now();
    }
    
    void EndDecode(char frame_type) {
        decode_end_ = Now();
        last_frame_type_ = frame_type;
    }
    
    void StartRender() {
        render_start_ = Now();
    }
    
    void EndRender() {
        render_end_ = Now();
    }
    
    void RecordFrame() {
        Stats s;
        s.decode_us = DurationUs(decode_start_, decode_end_);
        s.render_us = DurationUs(render_start_, render_end_);
        s.total_us = s.decode_us + s.render_us;
        s.frame_type = last_frame_type_;
        
        stats_.push_back(s);
        if (stats_.size() > 30) stats_.pop_front();
        
        frame_count_++;
        if (frame_count_ % 30 == 0) {
            PrintStats();
        }
    }
    
    void PrintStats() const {
        if (stats_.empty()) return;
        
        int64_t avg_decode = 0, avg_render = 0, max_total = 0;
        int64_t iframe_avg = 0, pframe_avg = 0, bframe_avg = 0;
        int iframe_count = 0, pframe_count = 0, bframe_count = 0;
        
        for (const auto& s : stats_) {
            avg_decode += s.decode_us;
            avg_render += s.render_us;
            max_total = std::max(max_total, s.total_us);
            
            switch (s.frame_type) {
                case 'I': iframe_avg += s.decode_us; iframe_count++; break;
                case 'P': pframe_avg += s.decode_us; pframe_count++; break;
                case 'B': bframe_avg += s.decode_us; bframe_count++; break;
            }
        }
        avg_decode /= stats_.size();
        avg_render /= stats_.size();
        
        double decode_fps = 1000000.0 / avg_decode;
        double total_fps = 1000000.0 / (avg_decode + avg_render);
        
        printf("\n========== 性能统计 (最近%d帧) ==========\n", (int)stats_.size());
        printf("解码: %4ld μs (%.1f fps)\n", avg_decode, decode_fps);
        printf("渲染: %4ld μs\n", avg_render);
        printf("总计: %4ld μs (%.1f fps)\n", avg_decode + avg_render, total_fps);
        printf("最大: %4ld μs\n", max_total);
        
        // 帧类型细分
        printf("\n帧类型解码统计:\n");
        if (iframe_count > 0) printf("  I帧: %4ld μs (样本%d)\n", iframe_avg / iframe_count, iframe_count);
        if (pframe_count > 0) printf("  P帧: %4ld μs (样本%d)\n", pframe_avg / pframe_count, pframe_count);
        if (bframe_count > 0) printf("  B帧: %4ld μs (样本%d)\n", bframe_avg / bframe_count, bframe_count);
        
        // 预算检查
        const int64_t BUDGET_30FPS = 33333;
        const int64_t BUDGET_60FPS = 16667;
        
        if (avg_decode + avg_render > BUDGET_30FPS) {
            printf("\n⚠️ 警告：超过30fps预算！(%ldμs > %ldμs)\n", avg_decode + avg_render, BUDGET_30FPS);
        } else if (avg_decode + avg_render > BUDGET_60FPS) {
            printf("\n✓ 满足30fps，但无法满足60fps\n");
        } else {
            printf("\n✓ 满足60fps\n");
        }
        printf("========================================\n\n");
    }
    
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    
    TimePoint Now() { return Clock::now(); }
    
    int64_t DurationUs(TimePoint start, TimePoint end) {
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    TimePoint decode_start_, decode_end_;
    TimePoint render_start_, render_end_;
    char last_frame_type_ = '?';
    std::deque<Stats> stats_;
    int frame_count_ = 0;
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <视频文件>\n", argv[0]);
        fprintf(stderr, "\n功能:\n");
        fprintf(stderr, "  - 实时显示解码和渲染耗时\n");
        fprintf(stderr, "  - 区分I/P/B帧的解码成本\n");
        fprintf(stderr, "  - 检查帧率预算（30fps/60fps）\n");
        fprintf(stderr, "\n尝试拖动窗口观察卡顿现象！\n");
        return 1;
    }

    // ========== 初始化FFmpeg ==========
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, argv[1], nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "无法打开文件: %s\n", errbuf);
        return 1;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "无法获取流信息\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }

    // 查找视频流
    int video_stream_idx = av_find_best_stream(
        fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_idx < 0) {
        fprintf(stderr, "未找到视频流\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    AVStream* video_stream = fmt_ctx->streams[video_stream_idx];

    printf("视频信息: %dx%d @ %.2ffps\n", 
           video_stream->codecpar->width,
           video_stream->codecpar->height,
           av_q2d(video_stream->avg_frame_rate));

    // 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(
        video_stream->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);

    printf("解码器: %s, 线程数: %d\n\n", 
           codec->name, codec_ctx->thread_count);

    // ========== 初始化SDL2 ==========
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Profiler Player (Chapter 4)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        codec_ctx->width, codec_ctx->height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
        codec_ctx->width, codec_ctx->height);

    // ========== 解码循环 ==========
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    PerformanceMonitor monitor;
    int64_t start_time = av_gettime();

    printf("开始播放，尝试拖动窗口观察卡顿...\n\n");

    while (av_read_frame(fmt_ctx, packet) >= 0) {
        // 处理窗口事件
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) goto cleanup;
        }

        if (packet->stream_index == video_stream_idx) {
            monitor.StartDecode();
            
            avcodec_send_packet(codec_ctx, packet);
            while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                // 获取帧类型
                char frame_type = av_get_picture_type_char((AVPictureType)frame->pict_type);
                monitor.EndDecode(frame_type);
                
                // 同步
                int64_t pts_us = frame->pts * av_q2d(video_stream->time_base) * 1000000;
                int64_t elapsed = av_gettime() - start_time;
                if (pts_us > elapsed) av_usleep(pts_us - elapsed);

                // 渲染
                monitor.StartRender();
                SDL_UpdateYUVTexture(texture, nullptr,
                    frame->data[0], frame->linesize[0],
                    frame->data[1], frame->linesize[1],
                    frame->data[2], frame->linesize[2]);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                SDL_RenderPresent(renderer);
                monitor.EndRender();
                
                monitor.RecordFrame();
            }
        }
        av_packet_unref(packet);
    }

cleanup:
    printf("\n播放结束，清理资源...\n");
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

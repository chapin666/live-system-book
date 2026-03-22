/**
 * 音视频同步采集演示
 * 
 * 编译: g++ av_capture_demo.cpp -o av_capture_demo $(pkg-config --cflags --libs libavformat libavcodec libavutil libavdevice libswscale sdl2)
 * 运行: ./av_capture_demo
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string>
#include <queue>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

struct AVPacketWrapper {
    AVPacket* pkt;
    int stream_index;
    int64_t pts;
};

class AVCapture {
public:
    ~AVCapture() {
        Cleanup();
    }
    
    bool InitVideo(const char* device) {
        avdevice_register_all();
        
        const AVInputFormat* fmt = nullptr;
        const char* dev = device;
        
#if defined(__APPLE__)
        fmt = av_find_input_format("avfoundation");
        if (!dev) dev = "0";
#elif defined(__linux__)
        fmt = av_find_input_format("v4l2");
        if (!dev) dev = "/dev/video0";
#endif
        
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "video_size", "640x480", 0);
        av_dict_set(&opts, "framerate", "30", 0);
        
        int ret = avformat_open_input(&video_ctx_, dev, fmt, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) {
            fprintf(stderr, "Failed to open video device\n");
            return false;
        }
        
        avformat_find_stream_info(video_ctx_, nullptr);
        
        for (unsigned int i = 0; i < video_ctx_>nb_streams; i++) {
            if (video_ctx_>streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx_ = i;
                break;
            }
        }
        
        return video_stream_idx_ >= 0;
    }
    
    bool InitAudio() {
        const AVInputFormat* fmt = nullptr;
        const char* dev = nullptr;
        
#if defined(__APPLE__)
        fmt = av_find_input_format("avfoundation");
        dev = ":0";
#elif defined(__linux__)
        fmt = av_find_input_format("alsa");
        dev = "default";
#endif
        
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "sample_rate", "48000", 0);
        
        int ret = avformat_open_input(&audio_ctx_, dev, fmt, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) return false;
        
        avformat_find_stream_info(audio_ctx_, nullptr);
        
        for (unsigned int i = 0; i < audio_ctx_>nb_streams; i++) {
            if (audio_ctx_>streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_idx_ = i;
                break;
            }
        }
        
        return audio_stream_idx_ >= 0;
    }
    
    void CaptureLoop() {
        AVPacket* v_pkt = av_packet_alloc();
        AVPacket* a_pkt = av_packet_alloc();
        
        bool running = true;
        int64_t start_time = av_gettime();
        
        printf("AV Capture started. Press Q to quit\n");
        
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT || 
                    (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_q)) {
                    running = false;
                }
            }
            
            // 读取视频
            if (video_ctx_) {
                int ret = av_read_frame(video_ctx_, v_pkt);
                if (ret >= 0 && v_pkt->stream_index == video_stream_idx_) {
                    int64_t now = av_gettime() - start_time;
                    printf("[V] pts=%lld, delay=%lld us\n", 
                           v_pkt->pts, now - v_pkt->pts);
                }
                av_packet_unref(v_pkt);
            }
            
            // 读取音频
            if (audio_ctx_) {
                int ret = av_read_frame(audio_ctx_, a_pkt);
                if (ret >= 0 && a_pkt->stream_index == audio_stream_idx_) {
                    printf("[A] pts=%lld\n", a_pkt->pts);
                }
                av_packet_unref(a_pkt);
            }
            
            SDL_Delay(1);
        }
        
        av_packet_free(&v_pkt);
        av_packet_free(&a_pkt);
    }
    
    void Cleanup() {
        if (video_ctx_) avformat_close_input(&video_ctx_);
        if (audio_ctx_) avformat_close_input(&audio_ctx_);
    }
    
private:
    AVFormatContext* video_ctx_ = nullptr;
    AVFormatContext* audio_ctx_ = nullptr;
    int video_stream_idx_ = -1;
    int audio_stream_idx_ = -1;
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    
    AVCapture capture;
    
    printf("Initializing video...\n");
    if (!capture.InitVideo(argc > 1 ? argv[1] : nullptr)) {
        fprintf(stderr, "Failed to init video\n");
        return 1;
    }
    
    printf("Initializing audio...\n");
    if (!capture.InitAudio()) {
        fprintf(stderr, "Failed to init audio\n");
        return 1;
    }
    
    printf("Starting capture loop...\n");
    capture.CaptureLoop();
    
    SDL_Quit();
    return 0;
}

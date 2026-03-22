#include "live/player_pipeline.h"
#include "live/ffmpeg_demuxer.h"
#include "live/ffmpeg_decoder.h"
#include "live/sdl_renderer.h"
#include <cstdio>
#include <thread>

extern "C" {
#include <libavutil/time.h>
}

namespace live {

PlayerPipeline::PlayerPipeline() = default;

PlayerPipeline::~PlayerPipeline() {
    // 资源由智能指针自动释放
}

PlayerPipeline::PlayerPipeline(IDemuxerPtr demuxer, IDecoderPtr decoder, 
                                IRendererPtr renderer)
    : demuxer_(std::move(demuxer))
    , decoder_(std::move(decoder))
    , renderer_(std::move(renderer)) {
}

ErrorCode PlayerPipeline::Init(const PipelineConfig& config) {
    config_ = config;
    
    // 1. 创建默认模块
    if (!demuxer_) {
        demuxer_ = std::make_unique<FFmpegDemuxer>();
    }
    if (!decoder_) {
        decoder_ = std::make_unique<FFmpegDecoder>();
    }
    if (!renderer_) {
        renderer_ = std::make_unique<SDLRenderer>();
    }
    
    // 2. 初始化Demuxer
    ErrorCode ret = demuxer_->Open(config.url);
    if (ret != ErrorCode::OK) {
        return ret;
    }
    
    // 3. 初始化解码器
    const AVCodecParameters* params = demuxer_->GetVideoParams();
    ret = decoder_->Init(params, config.decode_threads);
    if (ret != ErrorCode::OK) {
        return ret;
    }
    
    // 4. 初始化渲染器
    int width = config.video_width > 0 ? config.video_width : decoder_->GetWidth();
    int height = config.video_height > 0 ? config.video_height : decoder_->GetHeight();
    ret = renderer_->Init(width, height, "Player - Chapter 03");
    if (ret != ErrorCode::OK) {
        return ret;
    }
    
    printf("[Pipeline] Initialized successfully\n");
    initialized_ = true;
    return ErrorCode::OK;
}

ErrorCode PlayerPipeline::Run() {
    if (!initialized_) {
        return ErrorCode::OpenFailed;
    }
    
    printf("[Pipeline] Starting playback...\n");
    
    // 分配AVPacket和AVFrame
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    int64_t start_time = av_gettime();
    stop_requested_ = false;
    
    // 主循环
    while (!stop_requested_) {
        // 1. 处理窗口事件
        if (!renderer_->PollEvents()) {
            printf("[Pipeline] User requested exit\n");
            break;
        }
        
        // 2. 读取压缩数据
        ErrorCode ret = demuxer_->ReadPacket(packet);
        if (ret == ErrorCode::EndOfFile) {
            printf("[Pipeline] End of file reached\n");
            break;
        }
        if (ret != ErrorCode::OK) {
            printf("[Pipeline] Read error\n");
            break;
        }
        
        // 3. 送入解码器
        ret = decoder_->SendPacket(packet);
        if (ret != ErrorCode::OK) {
            printf("[Pipeline] Send packet failed\n");
            break;
        }
        av_packet_unref(packet);
        
        // 4. 接收解码后的帧
        while (true) {
            ret = decoder_->ReceiveFrame(frame);
            if (ret == ErrorCode::EndOfFile) {
                break;
            }
            if (ret != ErrorCode::OK) {
                break;
            }
            
            // 5. 简单同步（假设30fps）
            int64_t frame_duration = 33333;  // 33.3ms
            int64_t target_time = start_time + (played_frames_ * frame_duration);
            int64_t now = av_gettime();
            if (target_time > now) {
                av_usleep(target_time - now);
            }
            
            // 6. 渲染
            renderer_->RenderFrame(frame);
            renderer_->Present();
            
            played_frames_++;
        }
    }
    
    // 刷新解码器
    decoder_->SendPacket(nullptr);
    while (decoder_->ReceiveFrame(frame) == ErrorCode::OK) {
        renderer_->RenderFrame(frame);
        renderer_->Present();
        played_frames_++;
    }
    
    // 清理
    av_frame_free(&frame);
    av_packet_free(&packet);
    
    // 关闭各模块
    renderer_->Close();
    decoder_->Close();
    demuxer_->Close();
    
    printf("[Pipeline] Playback finished. Total frames: %ld\n", 
           played_frames_.load());
    
    return ErrorCode::OK;
}

void PlayerPipeline::RequestStop() {
    printf("[Pipeline] Stop requested\n");
    stop_requested_ = true;
}

int64_t PlayerPipeline::GetPlayedFrames() const {
    return played_frames_.load();
}

int64_t PlayerPipeline::GetDroppedFrames() const {
    return dropped_frames_.load();
}

} // namespace live

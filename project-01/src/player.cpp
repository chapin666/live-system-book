#include "live/player.h"
#include "live/idemuxer.h"
#include "live/idecoder.h"
#include "live/irenderer.h"
#include "live/raii_utils.h"
#include <iostream>
#include <sstream>
#include <iomanip>

extern "C" {
#include <libavutil/time.h>
}

namespace live {

Player::Player() = default;

Player::~Player() {
    Shutdown();
}

bool Player::Init(const char* url) {
    url_ = url;
    
    // 创建组件
    demuxer_ = CreateFFmpegDemuxer();
    decoder_ = CreateFFmpegDecoder();
    renderer_ = CreateSDLRenderer();
    
    if (!demuxer_ || !decoder_ || !renderer_) {
        std::cerr << "创建组件失败" << std::endl;
        return false;
    }
    
    // 打开输入
    if (!demuxer_->Open(url)) {
        std::cerr << "无法打开文件: " << url << std::endl;
        return false;
    }
    
    // 获取视频信息
    StreamInfo info;
    if (!demuxer_->GetVideoStreamInfo(info)) {
        std::cerr << "未找到视频流" << std::endl;
        return false;
    }
    
    duration_us_ = demuxer_->GetDuration();
    
    std::cout << "=== 视频信息 ===" << std::endl;
    std::cout << "分辨率: " << info.width << "x" << info.height << std::endl;
    std::cout << "时长: " << std::fixed << std::setprecision(2) 
              << duration_us_ / 1000000.0 << " 秒" << std::endl;
    std::cout << "================" << std::endl;
    
    // 初始化解码器和渲染器
    if (!decoder_->Init(info.codec_id, info.width, info.height)) {
        std::cerr << "初始化解码器失败" << std::endl;
        return false;
    }
    
    title_ = std::string("Player - ") + url;
    if (!renderer_->Init(info.width, info.height, title_.c_str())) {
        std::cerr << "初始化渲染器失败" << std::endl;
        return false;
    }
    
    return true;
}

void Player::Shutdown() {
    Stop();
    if (demuxer_) demuxer_->Close();
    if (decoder_) decoder_->Close();
    if (renderer_) renderer_->Close();
}

void Player::Play() {
    if (is_playing_) return;
    
    is_playing_ = true;
    is_paused_ = false;
    should_stop_ = false;
    start_time_ = av_gettime();
    fps_start_time_ = start_time_;
    frame_count_ = 0;
    
    Run();
}

void Player::Pause() {
    if (!is_playing_ || is_paused_) return;
    
    is_paused_ = true;
    pause_start_time_ = av_gettime();
}

void Player::Resume() {
    if (!is_playing_ || !is_paused_) return;
    
    is_paused_ = false;
    total_pause_duration_ += av_gettime() - pause_start_time_;
}

void Player::Stop() {
    should_stop_ = true;
    is_playing_ = false;
}

void Player::TogglePause() {
    if (is_paused_) {
        Resume();
    } else {
        Pause();
    }
}

float Player::GetProgress() const {
    if (duration_us_ <= 0) return 0.0f;
    return static_cast<float>(current_pts_) * 100.0f / duration_us_;
}

int64_t Player::GetAdjustedTime() const {
    if (is_paused_) {
        return pause_start_time_ - start_time_ - total_pause_duration_;
    }
    return av_gettime() - start_time_ - total_pause_duration_;
}

void Player::UpdateFPS() {
    frame_count_++;
    int64_t now = av_gettime();
    
    if (now - fps_start_time_ >= 1000000) {
        current_fps_ = frame_count_ * 1000000.0f / (now - fps_start_time_);
        frame_count_ = 0;
        fps_start_time_ = now;
        UpdateWindowTitle();
    }
}

void Player::UpdateWindowTitle() {
    std::ostringstream oss;
    oss << title_ 
        << " | " << std::fixed << std::setprecision(1) << GetProgress() << "%"
        << " | " << std::setprecision(2) << current_fps_ << " FPS";
    if (is_paused_) {
        oss << " [PAUSED]";
    }
    renderer_->SetTitle(oss.str().c_str());
}

void Player::Run() {
    FramePtr frame(av_frame_alloc());
    PacketPtr packet(av_packet_alloc());
    
    while (!should_stop_) {
        // 处理事件
        if (!renderer_->PollEvents()) {
            break;
        }
        
        // 暂停时只处理事件，不解码
        if (is_paused_) {
            UpdateWindowTitle();
            av_usleep(10000);  // 10ms
            continue;
        }
        
        // 读取 packet
        if (!demuxer_->ReadPacket(packet.get())) {
            break;
        }
        
        // 解码
        decoder_->SendPacket(packet.get());
        av_packet_unref(packet.get());
        
        // 接收帧
        while (decoder_->ReceiveFrame(frame.get())) {
            current_pts_ = frame->pts;
            
            // 同步
            int64_t pts_us = current_pts_;
            int64_t elapsed = GetAdjustedTime();
            if (pts_us > elapsed) {
                av_usleep(pts_us - elapsed);
            }
            
            // 渲染
            renderer_->RenderFrame(frame.get());
            
            // 更新 FPS
            UpdateFPS();
        }
    }
    
    // 刷新解码器
    decoder_->Flush();
    while (decoder_->ReceiveFrame(frame.get())) {
        renderer_->RenderFrame(frame.get());
    }
    
    is_playing_ = false;
}

} // namespace live

#include "live/network_player.h"
#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
}

namespace live {

NetworkPlayer::NetworkPlayer() = default;
NetworkPlayer::~NetworkPlayer() = default;

bool NetworkPlayer::IsNetworkUrl(const char* url) const {
    return strncmp(url, "http://", 7) == 0 ||
           strncmp(url, "https://", 8) == 0;
}

bool NetworkPlayer::PlayWithRetry(const char* url, int max_retries) {
    max_retries_ = max_retries;
    
    for (int i = 0; i < max_retries; i++) {
        std::cout << "尝试连接 (" << (i + 1) << "/" << max_retries << ")..." << std::endl;
        
        if (TryPlay(url)) {
            return true;
        }
        
        if (i < max_retries - 1) {
            std::cout << "2秒后重试..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    
    return false;
}

bool NetworkPlayer::TryPlay(const char* url) {
    AVDictionary* opts = nullptr;
    
    // 网络选项
    if (IsNetworkUrl(url)) {
        av_dict_set(&opts, "timeout", "10000000", 0);      // 10秒连接超时
        av_dict_set(&opts, "rw_timeout", "5000000", 0);    // 5秒读写超时
        av_dict_set(&opts, "tcp_nodelay", "1", 0);         // 低延迟模式
        av_dict_set(&opts, "buffer_size", "65536", 0);     // 64KB缓冲区
    }
    
    // 调用父类初始化
    bool success = Init(url);
    
    av_dict_free(&opts);
    
    if (success) {
        is_connected_ = true;
        Play();
    }
    
    return success;
}

void NetworkPlayer::SetBufferSize(int min_buffer_ms, int max_buffer_ms) {
    min_buffer_ms_ = min_buffer_ms;
    max_buffer_ms_ = max_buffer_ms;
}

int NetworkPlayer::GetBufferProgress() const {
    return buffer_progress_;
}

bool NetworkPlayer::IsConnected() const {
    return is_connected_;
}

void NetworkPlayer::HandleNetworkError(int error_code) {
    is_connected_ = false;
    
    std::string error_msg;
    switch (error_code) {
        case AVERROR_EXIT:
            error_msg = "连接被中断";
            break;
        case AVERROR(ETIMEDOUT):
            error_msg = "连接超时";
            break;
        case AVERROR(EIO):
            error_msg = "网络 I/O 错误";
            break;
        case AVERROR_INVALIDDATA:
            error_msg = "无效数据（可能是404）";
            break;
        default: {
            char errbuf[256];
            av_strerror(error_code, errbuf, sizeof(errbuf));
            error_msg = errbuf;
        }
    }
    
    std::cerr << "网络错误: " << error_msg << std::endl;
    
    if (on_network_error) {
        on_network_error(error_msg.c_str());
    }
}

void NetworkPlayer::UpdateBufferProgress() {
    // 简化实现，实际需要根据缓冲区状态计算
    if (on_buffer_progress) {
        on_buffer_progress(buffer_progress_);
    }
}

} // namespace live
#include "live/download_thread.h"
#include <curl/curl.h>
#include <iostream>
#include <chrono>

namespace live {

// libcurl 写回调
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* self = static_cast<DownloadThread*>(userp);
    size_t total_size = size * nmemb;
    
    // 写入环形缓冲区
    size_t written = 0;
    while (written < total_size && !self->IsRunning() == false ? false : 
           static_cast<DownloadThread*>(userp)->should_stop_.load() == false) {
        size_t n = self->WriteToBuffer(
            static_cast<uint8_t*>(contents) + written, 
            total_size - written);
        written += n;
        
        if (n == 0) {
            // 缓冲满，等待
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    return written;
}

DownloadThread::DownloadThread(RingBuffer* buffer, const DownloadConfig& config)
    : ring_buffer_(buffer)
    , config_(config) {
}

DownloadThread::~DownloadThread() {
    Stop();
}

bool DownloadThread::Start() {
    if (running_.load()) return false;
    
    running_.store(true);
    should_stop_.store(false);
    thread_ = std::thread(&DownloadThread::Run, this);
    
    std::cout << "[Download] Thread started, url=" << config_.url << std::endl;
    return true;
}

void DownloadThread::Stop() {
    if (!running_.load()) return;
    
    std::cout << "[Download] Stopping..." << std::endl;
    
    should_stop_.store(true);
    ring_buffer_->Stop();
    
    if (thread_.joinable()) {
        thread_.join();
    }
    
    running_.store(false);
    std::cout << "[Download] Stopped, total=" << downloaded_bytes_.load() << " bytes" << std::endl;
}

void DownloadThread::Run() {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[Download] Failed to init curl" << std::endl;
        running_.store(false);
        return;
    }
    
    // 设置 URL
    curl_easy_setopt(curl, CURLOPT_URL, config_.url.c_str());
    
    // 设置 Range（支持断点续传/seek）
    if (config_.start_pos > 0) {
        char range[64];
        snprintf(range, sizeof(range), "%ld-", config_.start_pos);
        curl_easy_setopt(curl, CURLOPT_RANGE, range);
        std::cout << "[Download] Range: " << range << std::endl;
    }
    
    // 设置回调
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, config_.connect_timeout);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1000L);  // 1KB/s
    
    // 执行下载
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[Download] Error: " << curl_easy_strerror(res) << std::endl;
    }
    
    curl_easy_cleanup(curl);
    running_.store(false);
}

} // namespace live

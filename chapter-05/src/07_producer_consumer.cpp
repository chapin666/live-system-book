/**
 * @file 07_producer_consumer.cpp
 * @brief 生产者-消费者模拟（音视频场景）
 * 
 * 模拟场景：
 * - 1个生产者（解码线程）
 * - 1个消费者（渲染线程）
 * - 队列缓冲帧数据
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include "live/threadsafe_queue.h"

using namespace live;

// 模拟视频帧
struct Frame {
    int pts;           // 显示时间戳
    int decode_time;   // 模拟解码耗时 (ms)
    char data[256];    // 模拟帧数据
    
    Frame() : pts(0), decode_time(0) {
        std::memset(data, 0, sizeof(data));
    }
    
    Frame(int p, int dt) : pts(p), decode_time(dt) {
        std::memset(data, 'F', sizeof(data));
    }
};

class VideoPlayer {
public:
    VideoPlayer() : stop_flag_(false) {}
    
    void start() {
        stop_flag_ = false;
        decoder_thread_ = std::thread(&VideoPlayer::decoder_loop, this);
        renderer_thread_ = std::thread(&VideoPlayer::renderer_loop, this);
    }
    
    void stop() {
        stop_flag_ = true;
        frame_queue_.stop();
    }
    
    void wait() {
        if (decoder_thread_.joinable()) {
            decoder_thread_.join();
        }
        if (renderer_thread_.joinable()) {
            renderer_thread_.join();
        }
    }

private:
    void decoder_loop() {
        std::cout << "[Decoder] 启动\n";
        
        int pts = 0;
        const int total_frames = 30;
        
        while (!stop_flag_ && pts < total_frames) {
            // 模拟解码耗时
            int decode_time = (pts % 5 == 0) ? 20 : 10;  // I帧更慢
            std::this_thread::sleep_for(
                std::chrono::milliseconds(decode_time));
            
            Frame frame(pts, decode_time);
            frame_queue_.push(frame);
            
            std::cout << "[Decoder] 解码帧 " << pts 
                      << " (耗时 " << decode_time << "ms)\n";
            
            ++pts;
        }
        
        std::cout << "[Decoder] 完成，队列大小: " 
                  << frame_queue_.size() << "\n";
        frame_queue_.stop();
    }
    
    void renderer_loop() {
        std::cout << "[Renderer] 启动\n";
        
        Frame frame;
        int rendered = 0;
        
        while (frame_queue_.pop(frame)) {
            // 模拟渲染耗时
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            
            std::cout << "[Renderer] 渲染帧 " << frame.pts << "\n";
            ++rendered;
        }
        
        std::cout << "[Renderer] 完成，共渲染 " << rendered << " 帧\n";
    }
    
    ThreadSafeQueue<Frame> frame_queue_;
    std::atomic<bool> stop_flag_;
    std::thread decoder_thread_;
    std::thread renderer_thread_;
};

int main() {
    std::cout << "=== 生产者-消费者模拟（视频播放）===\n\n";
    
    VideoPlayer player;
    player.start();
    
    // 模拟播放 1 秒
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "\n[Main] 请求停止...\n\n";
    player.stop();
    player.wait();
    
    std::cout << "\n=== 播放结束 ===\n";
    return 0;
}

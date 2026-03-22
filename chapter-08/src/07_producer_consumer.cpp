/**
 * @file 07_producer_consumer.cpp
 * @brief 生产者-消费者完整示例
 * 
 * 模拟音视频播放器场景：
 * - 解码线程（生产者）：读取文件并解码，产生视频帧
 * - 渲染线程（消费者）：从队列取帧并显示
 * 
 * 关键设计：
 * - 使用 ThreadSafeQueue 解耦生产和消费
 * - 优雅处理结束和 seek 操作
 * - 统计信息（队列水位、帧率等）
 */

#include "live/threadsafe_queue.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <signal.h>

using namespace live;

// 模拟视频帧
struct Frame {
    int64_t pts;                    // 显示时间戳
    std::vector<uint8_t> data;      // 帧数据
    int width;
    int height;
    bool is_keyframe;
    
    Frame() : pts(0), width(0), height(0), is_keyframe(false) {}
    
    Frame(int64_t p, int w, int h, bool key = false) 
        : pts(p), width(w), height(h), is_keyframe(key) {
        data.resize(w * h * 3 / 2);  // YUV420
    }
    
    // 禁用拷贝，启用移动（大对象优化）
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
    
    Frame(Frame&&) = default;
    Frame& operator=(Frame&&) = default;
};

// 播放器状态
enum class PlayerState {
    STOPPED,
    PLAYING,
    PAUSED,
    SEEKING,
    EOS          // End of Stream
};

// 播放器控制类
class PlayerController {
public:
    PlayerController() 
        : state_(PlayerState::STOPPED)
        , frame_queue_(60)           // 60帧缓冲
        , total_decoded_(0)
        , total_rendered_(0)
        , dropped_frames_(0)
        , target_fps_(30) {}
    
    ~PlayerController() {
        stop();
    }
    
    // 开始播放
    void play() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == PlayerState::PLAYING) return;
            state_ = PlayerState::PLAYING;
        }
        
        std::cout << "[Player] 开始播放" << std::endl;
        
        // 启动解码线程
        decoder_thread_ = std::thread(&PlayerController::decoder_loop, this);
        
        // 启动渲染线程
        renderer_thread_ = std::thread(&PlayerController::renderer_loop, this);
        
        // 启动统计线程
        stats_thread_ = std::thread(&PlayerController::stats_loop, this);
    }
    
    // 停止播放
    void stop() {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (state_ == PlayerState::STOPPED) return;
            state_ = PlayerState::STOPPED;
        }
        
        std::cout << "[Player] 停止播放" << std::endl;
        
        // 关闭队列，唤醒等待的线程
        frame_queue_.shutdown();
        
        // 通知所有等待状态的线程
        state_cv_.notify_all();
        
        // 等待线程结束
        if (decoder_thread_.joinable()) decoder_thread_.join();
        if (renderer_thread_.joinable()) renderer_thread_.join();
        if (stats_thread_.joinable()) stats_thread_.join();
    }
    
    // 暂停/继续
    void pause() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == PlayerState::PLAYING) {
            state_ = PlayerState::PAUSED;
            std::cout << "[Player] 暂停" << std::endl;
        } else if (state_ == PlayerState::PAUSED) {
            state_ = PlayerState::PLAYING;
            state_cv_.notify_all();
            std::cout << "[Player] 继续" << std::endl;
        }
    }
    
    // Seek 操作（模拟）
    void seek(int64_t target_pts) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != PlayerState::PLAYING) return;
        
        state_ = PlayerState::SEEKING;
        seek_target_ = target_pts;
        
        // 清空队列
        frame_queue_.clear();
        std::cout << "[Player] Seek 到 " << target_pts << std::endl;
        
        state_ = PlayerState::PLAYING;
        state_cv_.notify_all();
    }
    
    // 获取统计信息
    void print_stats() {
        std::cout << "\n========== 播放统计 ==========" << std::endl;
        std::cout << "总解码帧: " << total_decoded_.load() << std::endl;
        std::cout << "总渲染帧: " << total_rendered_.load() << std::endl;
        std::cout << "丢弃帧: " << dropped_frames_.load() << std::endl;
        std::cout << "当前队列: " << frame_queue_.size() << "/60" << std::endl;
        std::cout << "==============================\n" << std::endl;
    }
    
private:
    // 解码线程
    void decoder_loop() {
        pthread_setname_np(pthread_self(), "decoder");
        
        int64_t pts = 0;
        const int64_t duration = 10 * 30;  // 10秒 @ 30fps
        
        while (true) {
            // 检查状态
            {
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait(lock, [this] {
                    return state_ != PlayerState::PAUSED && 
                           state_ != PlayerState::STOPPED;
                });
                
                if (state_ == PlayerState::STOPPED) break;
                
                // 处理 seek
                if (state_ == PlayerState::SEEKING) {
                    pts = seek_target_;
                    std::cout << "[解码器] Seek 到 " << pts << std::endl;
                }
            }
            
            // 模拟解码耗时
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            // 创建帧
            auto frame = std::make_unique<Frame>(
                pts, 1920, 1080, (pts % 30 == 0)  // 每秒一个关键帧
            );
            
            // 入队（阻塞直到有空间）
            if (!frame_queue_.push(std::move(*frame))) {
                std::cout << "[解码器] 队列关闭，退出" << std::endl;
                break;
            }
            
            total_decoded_.fetch_add(1);
            ++pts;
            
            // 模拟 EOS
            if (pts >= duration) {
                std::cout << "[解码器] 到达文件末尾" << std::endl;
                // 发送 EOS 标记
                Frame eos_frame;
                eos_frame.pts = -1;  // EOS 标记
                frame_queue_.push(std::move(eos_frame));
                break;
            }
        }
        
        std::cout << "[解码器] 线程退出" << std::endl;
    }
    
    // 渲染线程
    void renderer_loop() {
        pthread_setname_np(pthread_self(), "renderer");
        
        Frame frame;
        auto last_frame_time = std::chrono::steady_clock::now();
        
        while (true) {
            // 检查状态
            {
                std::unique_lock<std::mutex> lock(state_mutex_);
                if (state_ == PlayerState::STOPPED) break;
            }
            
            // 出队（带超时，用于定期检查状态）
            if (frame_queue_.pop_with_timeout(frame, 100)) {
                // 检查 EOS
                if (frame.pts == -1) {
                    std::cout << "[渲染器] 收到 EOS，退出" << std::endl;
                    break;
                }
                
                // 帧同步：根据 PTS 控制播放速度
                auto now = std::chrono::steady_clock::now();
                auto target_time = last_frame_time + 
                    std::chrono::milliseconds(1000 / target_fps_);
                
                if (now < target_time) {
                    std::this_thread::sleep_until(target_time);
                } else {
                    // 延迟过大，丢弃帧
                    dropped_frames_.fetch_add(1);
                    continue;
                }
                
                last_frame_time = target_time;
                
                // 模拟渲染
                total_rendered_.fetch_add(1);
                
                // 每 30 帧打印一次
                if (frame.pts % 30 == 0) {
                    std::cout << "[渲染器] 显示帧 " << frame.pts 
                              << ", 队列深度: " << frame_queue_.size()
                              << std::endl;
                }
            }
        }
        
        std::cout << "[渲染器] 线程退出" << std::endl;
    }
    
    // 统计线程
    void stats_loop() {
        pthread_setname_np(pthread_self(), "stats");
        
        while (true) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_ == PlayerState::STOPPED) break;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            size_t queue_size = frame_queue_.size();
            int64_t decoded = total_decoded_.load();
            int64_t rendered = total_rendered_.load();
            
            std::cout << "[统计] 队列: " << std::setw(2) << queue_size << "/60, "
                      << "解码: " << decoded << ", "
                      << "渲染: " << rendered << ", "
                      << "丢弃: " << dropped_frames_.load()
                      << std::endl;
        }
    }
    
private:
    // 状态
    PlayerState state_;
    std::mutex state_mutex_;
    std::condition_variable state_cv_;
    
    // 队列
    ThreadSafeQueue<Frame> frame_queue_;
    
    // Seek 目标
    int64_t seek_target_ = 0;
    
    // 统计
    std::atomic<int64_t> total_decoded_;
    std::atomic<int64_t> total_rendered_;
    std::atomic<int64_t> dropped_frames_;
    int target_fps_;
    
    // 线程
    std::thread decoder_thread_;
    std::thread renderer_thread_;
    std::thread stats_thread_;
};

// 全局播放器用于信号处理
PlayerController* g_player = nullptr;

void signal_handler(int sig) {
    if (sig == SIGINT && g_player) {
        std::cout << "\n[信号] 收到 Ctrl+C，优雅停止..." << std::endl;
        g_player->stop();
    }
}

int main() {
    std::cout << "=== 生产者-消费者播放器模拟 ===" << std::endl;
    std::cout << R"(
控制说明:
  - 按 Ctrl+C 停止播放
  - 或者直接等待播放完成

场景模拟:
  - 解码线程: 模拟从文件解码视频帧
  - 渲染线程: 从队列取帧并按 30fps 显示
  - 队列: 60帧缓冲，平衡速度和稳定性
)" << std::endl;
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    
    // 创建播放器
    PlayerController player;
    g_player = &player;
    
    // 开始播放
    player.play();
    
    // 模拟 seek 操作（3秒后）
    std::this_thread::sleep_for(std::chrono::seconds(3));
    player.seek(60);  // Seek 到第 60 帧（2秒处）
    
    // 等待播放完成或用户中断
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // 停止
    player.stop();
    
    // 打印统计
    player.print_stats();
    
    std::cout << "\n播放器正常退出" << std::endl;
    
    return 0;
}

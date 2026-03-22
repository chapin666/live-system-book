// lag_experiment.cpp - 卡顿实验
// 故意在解码循环中加入延迟，观察事件响应延迟

#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <deque>

// 模拟不同耗时的解码
void SimulateDecode(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// 简单的延迟统计器
class LatencyMonitor {
public:
    void RecordLatency(int64_t ms) {
        latencies_.push_back(ms);
        if (latencies_.size() > 100) latencies_.pop_front();
    }
    
    void PrintStats() const {
        if (latencies_.empty()) return;
        
        int64_t sum = 0;
        int64_t max_val = 0;
        for (auto l : latencies_) {
            sum += l;
            max_val = std::max(max_val, l);
        }
        int64_t avg = sum / latencies_.size();
        
        printf("事件延迟统计: 平均%ldms, 最大%ldms (样本%zu)\n",
               avg, max_val, latencies_.size());
    }
    
private:
    std::deque<int64_t> latencies_;
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Lag Experiment (拖动我试试!)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        640, 480, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    
    // 读取命令行参数：模拟解码耗时（毫秒）
    int decode_time_ms = 50;  // 默认50ms
    if (argc > 1) {
        decode_time_ms = atoi(argv[1]);
        if (decode_time_ms < 1) decode_time_ms = 1;
        if (decode_time_ms > 500) decode_time_ms = 500;
    }
    
    printf("========================================\n");
    printf("       卡顿实验 - Lag Experiment\n");
    printf("========================================\n");
    printf("模拟解码耗时: %dms\n", decode_time_ms);
    printf("\n操作说明:\n");
    printf("  1. 尝试用鼠标拖动窗口\n");
    printf("  2. 观察窗口响应是否流畅\n");
    printf("  3. 查看打印的事件延迟数据\n");
    printf("\n按 Ctrl+C 退出\n\n");
    
    bool running = true;
    int frame_count = 0;
    LatencyMonitor latency_monitor;
    auto last_event_time = std::chrono::high_resolution_clock::now();
    
    while (running) {
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        // ===== 模拟解码 =====
        SimulateDecode(decode_time_ms);
        
        // ===== 模拟渲染 =====
        SDL_SetRenderDrawColor(renderer, 
            (frame_count * 5) % 255,  // 变化颜色
            100, 100, 255);
        SDL_RenderClear(renderer);
        
        // 绘制一些内容表示"渲染"
        SDL_Rect rect = {100, 100, 200, 150};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &rect);
        
        SDL_RenderPresent(renderer);
        
        // ===== 处理事件 =====
        auto event_start = std::chrono::high_resolution_clock::now();
        SDL_Event e;
        int events_processed = 0;
        
        while (SDL_PollEvent(&e)) {
            events_processed++;
            
            if (e.type == SDL_QUIT) {
                running = false;
            }
            
            // 记录窗口移动事件的延迟
            if (e.type == SDL_WINDOWEVENT && 
                e.window.event == SDL_WINDOWEVENT_MOVED) {
                auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                    event_start - last_event_time).count();
                latency_monitor.RecordLatency(latency);
            }
        }
        
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            frame_end - frame_start).count();
        
        // 定期打印统计信息
        if (++frame_count % 60 == 0) {
            double fps = 1000.0 / frame_duration;
            printf("帧%d: 模拟解码%dms, 实际帧耗时%ldms (%.1f fps), 处理%d个事件\n",
                   frame_count, decode_time_ms, frame_duration, fps, events_processed);
            
            latency_monitor.PrintStats();
        }
        
        last_event_time = event_start;
    }
    
    printf("\n实验结束\n");
    SDL_Quit();
    return 0;
}

# 第二十八章：性能调优

> **本章目标**：掌握直播系统的性能优化技巧，包括服务端优化、客户端优化和网络优化。

性能直接影响用户体验。本章从服务端、客户端、网络三个维度介绍性能优化方法。

---

## 目录

1. [性能指标定义](#1-性能指标定义)
2. [服务端性能优化](#2-服务端性能优化)
3. [客户端性能优化](#3-客户端性能优化)
4. [网络优化](#4-网络优化)
5. [编解码优化](#5-编解码优化)
6. [压力测试](#6-压力测试)
7. [本章总结](#7-本章总结)

---

## 1. 性能指标定义

### 1.1 关键性能指标（KPI）

| 指标 | 目标值 | 测量方法 |
|:---|:---:|:---|
| **端到端延迟** | < 400ms | 发送时间戳 - 接收时间戳 |
| **首帧时间** | < 2s | 加入房间到看到画面 |
| **卡顿率** | < 3% | 卡顿次数 / 总播放时长 |
| **CPU占用** | < 50% | 系统监控 |
| **内存占用** | < 500MB | 系统监控 |
| **电池消耗** | < 10%/小时 | 移动设备 |

### 1.2 性能分析工具

```cpp
// 性能分析器
class PerformanceProfiler {
public:
    void StartTrace(const std::string& name) {
        traces_[name] = GetCurrentTimeUs();
    }
    
    void EndTrace(const std::string& name) {
        auto it = traces_.find(name);
        if (it != traces_.end()) {
            int64_t elapsed = GetCurrentTimeUs() - it->second;
            LOG_INFO("[%s] took %ld us", name.c_str(), elapsed);
            
            // 记录统计
            stats_[name].AddSample(elapsed);
        }
    }
    
    void PrintStats() {
        for (const auto& [name, stat] : stats_) {
            LOG_INFO("[%s] avg=%ld, min=%ld, max=%ld, p99=%ld",
                     name.c_str(), stat.Average(), stat.Min(),
                     stat.Max(), stat.P99());
        }
    }
    
private:
    std::map<std::string, int64_t> traces_;
    std::map<std::string, Statistics> stats_;
};
```

---

## 2. 服务端性能优化

### 2.1 零拷贝技术

![零拷贝](./diagrams/zero-copy.svg)

**传统数据拷贝**：
```
网卡 → 内核缓冲区 → 用户缓冲区 → 处理 → 用户缓冲区 → 内核缓冲区 → 网卡
     （4次拷贝，4次上下文切换）
```

**零拷贝优化**：
```
网卡 → 内核缓冲区 ──────────────────────→ 网卡
              ↓（mmap直接访问）
           用户处理
```

```cpp
// 使用sendfile实现零拷贝文件传输
void SendFileZeroCopy(int socket_fd, int file_fd, off_t offset, size_t count) {
    // Linux sendfile: 内核直接传输，无需用户空间拷贝
    sendfile(socket_fd, file_fd, &offset, count);
}

// 使用splice进行管道零拷贝
void SpliceExample(int read_fd, int write_fd) {
    int pipefd[2];
    pipe(pipefd);
    
    // 从read_fd到pipe
    splice(read_fd, nullptr, pipefd[1], nullptr, 
           4096, SPLICE_F_MOVE);
    
    // 从pipe到write_fd
    splice(pipefd[0], nullptr, write_fd, nullptr,
           4096, SPLICE_F_MOVE);
}
```

### 2.2 内存池

```cpp
// 固定大小内存池
template<size_t BlockSize>
class MemoryPool {
public:
    MemoryPool(size_t initial_blocks = 1024) {
        AllocateBlock(initial_blocks);
    }
    
    ~MemoryPool() {
        for (auto block : blocks_) {
            delete[] block;
        }
    }
    
    void* Allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (free_list_.empty()) {
            AllocateBlock(1024);
        }
        
        void* ptr = free_list_.back();
        free_list_.pop_back();
        return ptr;
    }
    
    void Deallocate(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_list_.push_back(ptr);
    }
    
private:
    void AllocateBlock(size_t num_blocks) {
        char* block = new char[BlockSize * num_blocks];
        blocks_.push_back(block);
        
        for (size_t i = 0; i < num_blocks; ++i) {
            free_list_.push_back(block + i * BlockSize);
        }
    }
    
    std::vector<char*> blocks_;
    std::vector<void*> free_list_;
    std::mutex mutex_;
};

// 使用示例
MemoryPool<4096> rtp_packet_pool;  // 4KB块内存池

void* packet = rtp_packet_pool.Allocate();
// 使用...
r tp_packet_pool.Deallocate(packet);
```

### 2.3 无锁队列

```cpp
// 单生产者单消费者无锁队列
template<typename T, size_t Size>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {
        static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");
    }
    
    bool Push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) & (Size - 1);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;  // 队列满
        }
        
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool Pop(T& item) {
        size_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;  // 队列空
        }
        
        item = buffer_[current_head];
        head_.store((current_head + 1) & (Size - 1), 
                   std::memory_order_release);
        return true;
    }
    
private:
    T buffer_[Size];
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};
```

### 2.4 CPU亲和性

```cpp
// 绑定线程到特定CPU核心
void SetThreadAffinity(int cpu_core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// 为不同工作负载分配CPU核心
void SetupCPUAffinity() {
    // 网络接收线程 → CPU 0-1
    // 编码线程 → CPU 2-3
    // 业务逻辑 → CPU 4-5
    // 其他 → CPU 6-7
}
```

---

## 3. 客户端性能优化

### 3.1 渲染优化

```cpp
// 视频渲染优化
class OptimizedVideoRenderer {
public:
    void RenderFrame(const VideoFrame& frame) {
        // 1. 检查是否需要渲染（可见性检测）
        if (!IsVisible(frame.target_rect)) {
            return;  // 跳过不可见的帧
        }
        
        // 2. 检查尺寸变化
        if (frame.width != last_width_ || frame.height != last_height_) {
            RecreateTexture(frame.width, frame.height);
            last_width_ = frame.width;
            last_height_ = frame.height;
        }
        
        // 3. 批量更新纹理（减少GPU上传次数）
        if (pending_frames_.size() >= batch_size_) {
            FlushBatch();
        }
        pending_frames_.push_back(frame);
    }
    
private:
    void RecreateTexture(int width, int height) {
        // 使用2的幂次尺寸，提高GPU效率
        int tex_width = NextPowerOfTwo(width);
        int tex_height = NextPowerOfTwo(height);
        
        // 创建纹理...
    }
    
    bool IsVisible(const Rect& rect) {
        // 检测是否在屏幕范围内
        return rect.Intersects(screen_rect_);
    }
    
    std::vector<VideoFrame> pending_frames_;
    static constexpr int batch_size_ = 3;
    int last_width_ = 0;
    int last_height_ = 0;
};
```

### 3.2 功耗管理

```cpp
// 电池感知优化
class PowerManager {
public:
    void OnBatteryStatusChanged(bool is_ac_power, int battery_percent) {
        is_ac_power_ = is_ac_power;
        battery_percent_ = battery_percent;
        
        if (!is_ac_power && battery_percent < 20) {
            // 低电量模式
            EnterLowPowerMode();
        } else if (!is_ac_power) {
            // 电池模式
            EnterBatteryMode();
        } else {
            // 插电模式
            EnterPerformanceMode();
        }
    }
    
    void EnterLowPowerMode() {
        // 降低编码分辨率
        video_encoder_.SetResolution(640, 360);
        
        // 降低帧率
        video_encoder_.SetFrameRate(15);
        
        // 降低码率
        video_encoder_.SetBitrate(500000);  // 500kbps
        
        // 减少视频预览帧率
        preview_fps_ = 15;
    }
    
    void EnterBatteryMode() {
        video_encoder_.SetResolution(1280, 720);
        video_encoder_.SetFrameRate(24);
        video_encoder_.SetBitrate(1500000);  // 1.5Mbps
        preview_fps_ = 24;
    }
    
    void EnterPerformanceMode() {
        video_encoder_.SetResolution(1920, 1080);
        video_encoder_.SetFrameRate(30);
        video_encoder_.SetBitrate(2500000);  // 2.5Mbps
        preview_fps_ = 30;
    }
    
private:
    bool is_ac_power_ = true;
    int battery_percent_ = 100;
    VideoEncoder video_encoder_;
    int preview_fps_ = 30;
};
```

---

## 4. 网络优化

### 4.1 BBR拥塞控制

```
BBR (Bottleneck Bandwidth and RTT) 算法:

1. 测量瓶颈带宽 (BtlBw)
2. 测量最小RTT (RTprop)
3. 发送速率 = BtlBw
4. 拥塞窗口 = BtlBw × RTprop × 2

相比Cubic:
- 启动更快
- 在高丢包网络表现更好
- 延迟更低
```

```cpp
// 启用BBR (Linux)
void EnableBBR() {
    // 需要root权限
    system("echo 'net.ipv4.tcp_congestion_control=bbr' >> /etc/sysctl.conf");
    system("sysctl -p");
}

// 检查当前拥塞控制算法
std::string GetCongestionControl() {
    std::ifstream file("/proc/sys/net/ipv4/tcp_congestion_control");
    std::string algo;
    file >> algo;
    return algo;
}
```

### 4.2 QUIC协议

**QUIC优势**：
- 0-RTT连接建立
- 内置TLS 1.3
- 无队头阻塞的多路复用
- 连接迁移（IP变化保持连接）

```cpp
// QUIC vs TCP对比
void CompareProtocols() {
    // TCP + TLS:
    // 1. TCP三次握手 (1 RTT)
    // 2. TLS握手 (1-2 RTT)
    // 总计: 2-3 RTT
    
    // QUIC:
    // 首次连接: 1 RTT (包含TLS)
    // 后续连接: 0 RTT (使用之前协商的密钥)
}
```

---

## 5. 编解码优化

### 5.1 硬件编解码

```cpp
// 硬件编码器选择
class HardwareEncoder {
public:
    bool Initialize() {
        // 尝试VAAPI (Linux Intel/AMD)
        if (TryVAAPI()) return true;
        
        // 尝试VideoToolbox (macOS)
        if (TryVideoToolbox()) return true;
        
        // 尝试NVENC (NVIDIA GPU)
        if (TryNVENC()) return true;
        
        // 回退到软件编码
        return InitializeSoftwareEncoder();
    }
    
private:
    bool TryVAAPI() {
        #ifdef __linux__
        // 检查设备
        if (access("/dev/dri/renderD128", F_OK) == 0) {
            // 尝试初始化VAAPI
            return InitializeVAAPI();
        }
        #endif
        return false;
    }
};
```

### 5.2 SVC动态调整

```cpp
// SVC层选择优化
class SVCLayerSelector {
public:
    void UpdateNetworkCondition(int bandwidth_kbps, float packet_loss) {
        // 根据网络条件选择最佳层
        if (packet_loss > 0.05) {
            // 高丢包，降低层
            current_layer_ = std::max(0, current_layer_ - 1);
        } else if (bandwidth_kbps > target_bitrate_ * 1.5) {
            // 带宽充足，提升层
            current_layer_ = std::min(max_layer_, current_layer_ + 1);
        }
    }
    
    int GetTargetLayer() const {
        return current_layer_;
    }
    
private:
    int current_layer_ = 2;  // 默认中层
    int max_layer_ = 2;
    int target_bitrate_;
};
```

---

## 6. 压力测试

### 6.1 测试方案设计

```cpp
// SFU压力测试
class SFUStressTest {
public:
    void RunTest(int num_publishers, int num_subscribers_per_pub) {
        // 创建发布者
        for (int i = 0; i < num_publishers; ++i) {
            publishers_.push_back(CreatePublisher());
        }
        
        // 每个发布者创建订阅者
        for (auto& pub : publishers_) {
            for (int j = 0; j < num_subscribers_per_pub; ++j) {
                pub->AddSubscriber(CreateSubscriber());
            }
        }
        
        // 运行测试并收集指标
        RunForDuration(std::chrono::minutes(10));
        
        // 输出结果
        PrintResults();
    }
    
    void PrintResults() {
        LOG_INFO("=== 压力测试结果 ===");
        LOG_INFO("并发数: %lu 发布, %lu 订阅",
                 publishers_.size(),
                 total_subscribers_);
        LOG_INFO("CPU: %.1f%%", cpu_usage_);
        LOG_INFO("内存: %.1f MB", memory_mb_);
        LOG_INFO("延迟 P99: %ld ms", latency_p99_);
        LOG_INFO("丢包率: %.2f%%", packet_loss_rate_ * 100);
    }
    
private:
    std::vector<std::unique_ptr<Publisher>> publishers_;
    size_t total_subscribers_ = 0;
    double cpu_usage_ = 0;
    double memory_mb_ = 0;
    int64_t latency_p99_ = 0;
    float packet_loss_rate_ = 0;
};
```

### 6.2 容量规划

```
单台SFU服务器容量估算:

假设:
- 每个流: 2Mbps (1080p)
- 上行带宽: 1Gbps
- 下行带宽: 1Gbps

计算:
- 最大发布流数: 1000Mbps / 2Mbps = 500
- 假设平均每个流被订阅5次
- 总转发带宽: 500 × 2Mbps × 5 = 5000Mbps

限制因素:
- CPU: 软件编解码时受限
- 内存: 每个连接约10MB
- 网卡: 实际上行+下行共2Gbps

建议配置:
- 单服务器: 100-200并发流
- 集群部署支持更多用户
```

---

## 7. 本章总结

### 7.1 优化要点

| 层级 | 优化方向 | 关键技术 |
|:---|:---|:---|
| 服务端 | 零拷贝、内存池、无锁队列 | sendfile, mempool, lock-free |
| 客户端 | 渲染优化、功耗管理 | 批量渲染、电池感知 |
| 网络 | BBR、QUIC、连接迁移 | 拥塞控制算法 |
| 编解码 | 硬件加速、SVC | VAAPI, VideoToolbox, NVENC |

### 7.2 性能调优流程

```
1. 测量现状 → 2. 定位瓶颈 → 3. 针对性优化 → 4. 验证效果
     ↑                                            ↓
     └──────────── 持续迭代 ←─────────────────────┘
```

### 7.3 课后思考

1. **零拷贝权衡**：零拷贝减少了CPU拷贝，但可能增加复杂度。什么情况下不适合使用零拷贝？

2. **移动优化**：移动设备性能差异大，如何设计自适应的性能策略？

3. **容量规划**：你的SFU服务器部署在云上，如何设计自动扩缩容策略？

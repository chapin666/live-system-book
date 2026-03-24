/**
 * Chapter 28: 性能调优示例
 * 零拷贝、内存池、无锁队列、性能分析
 */

#include <iostream>
#include <vector>
#include <queue>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <memory>
#include <map>

// 性能分析器
class PerformanceProfiler {
public:
    struct Statistics {
        int64_t total = 0;
        int64_t count = 0;
        int64_t min_val = INT64_MAX;
        int64_t max_val = 0;
        
        void AddSample(int64_t value) {
            total += value;
            count++;
            min_val = std::min(min_val, value);
            max_val = std::max(max_val, value);
        }
        
        int64_t Average() const { return count > 0 ? total / count : 0; }
        int64_t Min() const { return min_val == INT64_MAX ? 0 : min_val; }
        int64_t Max() const { return max_val; }
        
        // 简化的 P99 计算
        int64_t P99() const { return max_val; }
    };
    
    void StartTrace(const std::string& name) {
        traces_[name] = GetCurrentTimeUs();
    }
    
    void EndTrace(const std::string& name) {
        auto it = traces_.find(name);
        if (it != traces_.end()) {
            int64_t elapsed = GetCurrentTimeUs() - it->second;
            stats_[name].AddSample(elapsed);
            std::cout << "[" << name << "] took " << elapsed << " us\n";
        }
    }
    
    void PrintStats() {
        std::cout << "\n=== Performance Stats ===\n";
        for (const auto& [name, stat] : stats_) {
            std::cout << "[" << name << "] "
                      << "avg=" << stat.Average() << "us, "
                      << "min=" << stat.Min() << "us, "
                      << "max=" << stat.Max() << "us\n";
        }
    }

private:
    int64_t GetCurrentTimeUs() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();
    }
    
    std::map<std::string, int64_t> traces_;
    std::map<std::string, Statistics> stats_;
};

// 简单内存池
template<size_t BlockSize, size_t BlockCount>
class MemoryPool {
public:
    MemoryPool() {
        // 预分配内存块
        memory_.resize(BlockSize * BlockCount);
        
        // 初始化空闲列表
        for (size_t i = 0; i < BlockCount; i++) {
            free_blocks_.push_back(memory_.data() + i * BlockSize);
        }
    }
    
    void* Allocate() {
        if (free_blocks_.empty()) {
            return nullptr;  // 内存池耗尽
        }
        
        void* block = free_blocks_.back();
        free_blocks_.pop_back();
        return block;
    }
    
    void Deallocate(void* block) {
        if (block) {
            free_blocks_.push_back(static_cast<uint8_t*>(block));
        }
    }
    
    size_t Available() const { return free_blocks_.size(); }

private:
    std::vector<uint8_t> memory_;
    std::vector<uint8_t*> free_blocks_;
};

// 无锁单生产者单消费者队列
template<typename T, size_t Size>
class LockFreeQueue {
public:
    LockFreeQueue() : head_(0), tail_(0) {
        buffer_.resize(Size);
    }
    
    bool Push(const T& item) {
        size_t current_tail = tail_.load(std::memory_order_relaxed);
        size_t next_tail = (current_tail + 1) % Size;
        
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
        head_.store((current_head + 1) % Size, std::memory_order_release);
        return true;
    }
    
    size_t Size_approx() const {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t tail = tail_.load(std::memory_order_relaxed);
        return (tail - head + Size) % Size;
    }

private:
    std::vector<T> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

// 对象池（基于内存池）
template<typename T>
class ObjectPool {
public:
    ObjectPool(size_t count) : pool_(sizeof(T), count) {}
    
    template<typename... Args>
    T* Acquire(Args&&... args) {
        void* memory = pool_.Allocate();
        if (!memory) return nullptr;
        return new (memory) T(std::forward<Args>(args)...);
    }
    
    void Release(T* obj) {
        if (obj) {
            obj->~T();
            pool_.Deallocate(obj);
        }
    }

private:
    MemoryPool<sizeof(T), 1024> pool_;
};

// 测试用的简单对象
struct Packet {
    uint32_t seq;
    uint8_t data[1400];
    int64_t timestamp;
    
    Packet() : seq(0), timestamp(0) {
        memset(data, 0, sizeof(data));
    }
    
    explicit Packet(uint32_t s) : seq(s), timestamp(0) {
        memset(data, 0, sizeof(data));
    }
};

void DemoMemoryPool() {
    std::cout << "\n=== Memory Pool Demo ===\n";
    
    MemoryPool<1024, 100> pool;
    std::cout << "Initial available blocks: " << pool.Available() << "\n";
    
    // 分配一些块
    std::vector<void*> blocks;
    for (int i = 0; i < 10; i++) {
        void* block = pool.Allocate();
        if (block) {
            blocks.push_back(block);
            std::cout << "Allocated block " << i << "\n";
        }
    }
    
    std::cout << "Available after alloc: " << pool.Available() << "\n";
    
    // 释放
    for (void* block : blocks) {
        pool.Deallocate(block);
    }
    
    std::cout << "Available after free: " << pool.Available() << "\n";
}

void DemoLockFreeQueue() {
    std::cout << "\n=== Lock-Free Queue Demo ===\n";
    
    LockFreeQueue<int, 64> queue;
    
    // 生产
    for (int i = 0; i < 10; i++) {
        if (queue.Push(i)) {
            std::cout << "Pushed: " << i << "\n";
        }
    }
    
    std::cout << "Queue size: " << queue.Size_approx() << "\n";
    
    // 消费
    int value;
    while (queue.Pop(value)) {
        std::cout << "Popped: " << value << "\n";
    }
}

void DemoObjectPool() {
    std::cout << "\n=== Object Pool Demo ===\n";
    
    ObjectPool<Packet> pool(100);
    
    // 获取对象
    std::vector<Packet*> packets;
    for (uint32_t i = 0; i < 5; i++) {
        Packet* p = pool.Acquire(i);
        if (p) {
            p->timestamp = i * 1000;
            packets.push_back(p);
            std::cout << "Acquired packet seq=" << p->seq << "\n";
        }
    }
    
    // 释放对象
    for (Packet* p : packets) {
        pool.Release(p);
    }
    std::cout << "All packets released\n";
}

void DemoPerformanceProfiler() {
    std::cout << "\n=== Performance Profiler Demo ===\n";
    
    PerformanceProfiler profiler;
    
    // 模拟一些操作
    for (int i = 0; i < 5; i++) {
        profiler.StartTrace("operation");
        
        // 模拟工作
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        
        profiler.EndTrace("operation");
    }
    
    profiler.PrintStats();
}

int main() {
    std::cout << "Chapter 28: 性能调优示例\n";
    std::cout << "========================\n";
    
    DemoMemoryPool();
    DemoLockFreeQueue();
    DemoObjectPool();
    DemoPerformanceProfiler();
    
    std::cout << "\n=== Optimization Tips ===\n";
    std::cout << "1. Use memory pool to reduce allocation overhead\n";
    std::cout << "2. Use lock-free queue for thread communication\n";
    std::cout << "3. Profile before optimizing\n";
    std::cout << "4. Consider zero-copy for large data transfer\n";
    std::cout << "5. Use object pool for frequently created/destroyed objects\n";
    
    return 0;
}
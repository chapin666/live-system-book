# 第五章：C++11 多线程基础

> **本章目标**：掌握 C++11 多线程编程基础，为第六章异步播放器打下坚实基础。
> 
 > **前置知识**：C++ 基础语法、面向对象编程
> 
> **本章代码**：所有示例均可编译运行，位于 `chapter-05/src/`

在音视频开发中，**多线程**是不可或缺的核心技术。播放器需要同时处理：网络数据接收、音视频解码、渲染显示——这些任务如果串行执行，画面必然卡顿。

本章将系统学习 C++11 引入的现代多线程库，从基础概念到线程安全队列，为后续构建高性能异步播放器做好准备。

---

## 🎯 本章学习路径

```
线程基础 ──┬──→ std::thread、join/detach、线程管理
           │
互斥同步 ──┼──→ mutex、lock_guard、死锁避免
           │
条件变量 ──┼──→ wait/notify、虚假唤醒、超时
           │
原子操作 ──┼──→ atomic、内存序基础
           │
队列实现 ──┴──→ 双条件变量、优雅停止

调试技巧 ─────→ GDB多线程、TSan检测数据竞争
```

---

## 目录

1. [线程基础](#1-线程基础)
   - [1.1 创建线程](#11-创建线程)
   - [1.2 线程管理](#12-线程管理)
   - [1.3 线程ID与命名](#13-线程id与命名)
2. [互斥与同步](#2-互斥与同步)
   - [2.1 为什么需要互斥](#21-为什么需要互斥)
   - [2.2 std::mutex基础](#22-stdmutex基础)
   - [2.3 RAII锁管理](#23-raii锁管理)
   - [2.4 死锁演示与避免](#24-死锁演示与避免)
3. [条件变量](#3-条件变量)
   - [3.1 生产者-消费者问题](#31-生产者-消费者问题)
   - [3.2 wait与notify](#32-wait与notify)
   - [3.3 虚假唤醒](#33-虚假唤醒)
   - [3.4 超时等待](#34-超时等待)
4. [原子操作](#4-原子操作)
   - [4.1 std::atomic基础](#41-stdatomic基础)
   - [4.2 内存序简介](#42-内存序简介)
   - [4.3 无锁计数器](#43-无锁计数器)
5. [线程安全队列](#5-线程安全队列)
   - [5.1 设计目标](#51-设计目标)
   - [5.2 双条件变量设计](#52-双条件变量设计)
   - [5.3 优雅停止](#53-优雅停止)
   - [5.4 完整实现](#54-完整实现)
6. [多线程调试技巧](#6-多线程调试技巧)
   - [6.1 GDB多线程调试](#61-gdb多线程调试)
   - [6.2 ThreadSanitizer检测数据竞争](#62-threadsanitizer检测数据竞争)
   - [6.3 常见问题排查](#63-常见问题排查)
7. [本章总结](#7-本章总结)

---

## 1. 线程基础

### 1.1 创建线程

C++11 引入了 `std::thread` 类，让创建线程变得简单：

```cpp
#include <thread>
#include <iostream>

void hello_world() {
    std::cout << "Hello from thread!\n";
}

int main() {
    // 创建线程
    std::thread t(hello_world);
    
    // 等待线程结束
    t.join();
    
    std::cout << "Main thread done\n";
    return 0;
}
```

**编译运行**：
```bash
g++ -std=c++11 -pthread 01_hello_thread.cpp -o hello_thread
./hello_thread
```

**输出**：
```
Hello from thread!
Main thread done
```

**关键点**：
- 必须链接 pthread 库（`-pthread`）
- `join()` 等待线程完成，否则程序会崩溃

#### 传递参数

```cpp
#include <thread>
#include <string>
#include <iostream>

void print_message(const std::string& msg, int times) {
    for (int i = 0; i < times; i++) {
        std::cout << msg << " " << i << "\n";
    }
}

int main() {
    // 传递参数给线程函数
    std::thread t(print_message, "Hello", 3);
    t.join();
    return 0;
}
```

**⚠️ 注意**：默认情况下参数会被**复制**到线程。如果要用引用，需用 `std::ref`：

```cpp
void modify_value(int& x) {
    x *= 2;
}

int main() {
    int value = 21;
    // 错误：值被复制，原value不变
    // std::thread t(modify_value, value);
    
    // 正确：使用std::ref传递引用
    std::thread t(modify_value, std::ref(value));
    t.join();
    
    std::cout << value << "\n";  // 输出 42
    return 0;
}
```

#### Lambda 表达式

更常见的用法是用 lambda 捕获局部变量：

```cpp
#include <thread>
#include <vector>
#include <iostream>

int main() {
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 5; i++) {
        // Lambda 捕获 i 的值
        threads.emplace_back([i]() {
            std::cout << "Thread " << i << " running\n";
        });
    }
    
    // 等待所有线程
    for (auto& t : threads) {
        t.join();
    }
    
    return 0;
}
```

**输出**（顺序可能不同）：
```
Thread 0 running
Thread 1 running
Thread 2 running
Thread 3 running
Thread 4 running
```

### 1.2 线程管理

#### join vs detach

```cpp
std::thread t(func);

// 方式1：等待线程完成（推荐）
t.join();

// 方式2：分离线程，让它独立运行
t.detach();
// 分离后不能再join，线程会在后台运行直到结束
```

**何时使用 detach**？
- 守护线程（后台日志写入）
- 长期运行的服务线程
- 明确不需要等待的任务

**⚠️ 危险示例**：

```cpp
void dangerous_detach() {
    int local_var = 42;
    
    std::thread t([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // 危险！函数返回后 local_var 已不存在
        std::cout << local_var << "\n";  // 未定义行为！
    });
    
    t.detach();  // 不要这样做！
    // 函数返回，local_var 被销毁
}
```

#### 线程生命周期

```
创建线程 ──→ 运行 ──→ 结束
    │           │
    │           ├── join() ──→ 主线程等待
    │           │
    └── detach() ─→ 后台运行
```

### 1.3 线程ID与命名

C++ 标准没有提供线程命名功能，但我们可以自己实现：

```cpp
#include <thread>
#include <mutex>
#include <map>
#include <string>
#include <sstream>

class ThreadNamer {
public:
    static void set_name(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        names_[std::this_thread::get_id()] = name;
    }
    
    static std::string get_name() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = names_.find(std::this_thread::get_id());
        if (it != names_.end()) {
            return it->second;
        }
        return thread_id_to_string();
    }
    
    static std::string get_name(std::thread::id id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = names_.find(id);
        if (it != names_.end()) {
            return it->second;
        }
        std::ostringstream oss;
        oss << id;
        return oss.str();
    }

private:
    static std::string thread_id_to_string() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }
    
    static std::mutex mutex_;
    static std::map<std::thread::id, std::string> names_;
};

// 静态成员定义
std::mutex ThreadNamer::mutex_;
std::map<std::thread::id, std::string> ThreadNamer::names_;

// 使用示例
void worker_thread() {
    ThreadNamer::set_name("Worker-1");
    std::cout << "Running in: " << ThreadNamer::get_name() << "\n";
}

int main() {
    std::thread t(worker_thread);
    std::cout << "Main thread ID: " << ThreadNamer::get_name() << "\n";
    std::cout << "Worker thread ID: " << ThreadNamer::get_name(t.get_id()) << "\n";
    t.join();
    return 0;
}
```

---

## 2. 互斥与同步

### 2.1 为什么需要互斥

多个线程同时读写共享数据会导致**数据竞争**：

```cpp
#include <thread>
#include <iostream>

int counter = 0;

void increment() {
    for (int i = 0; i < 100000; i++) {
        counter++;  // 非原子操作！
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter << "\n";  // 应该是 200000
    return 0;
}
```

**输出**（每次运行结果不同）：
```
Counter: 143882  // 错误！
Counter: 156234  // 错误！
```

**问题分析**：
`counter++` 实际上包含三步操作：
1. 读取 counter 的值
2. 值加 1
3. 写回 counter

当两个线程同时执行时，可能发生：
```
线程A 读取 counter = 100
线程B 读取 counter = 100  (同时！)
线程A 写入 counter = 101
线程B 写入 counter = 101  (覆盖了A的结果！)
```

### 2.2 std::mutex 基础

使用互斥锁保护共享数据：

```cpp
#include <thread>
#include <mutex>
#include <iostream>

int counter = 0;
std::mutex mtx;  // 互斥锁

void increment() {
    for (int i = 0; i < 100000; i++) {
        mtx.lock();    // 获取锁
        counter++;     // 临界区
        mtx.unlock();  // 释放锁
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter << "\n";  // 正确：200000
    return 0;
}
```

### 2.3 RAII 锁管理

手动 `lock/unlock` 容易出错（异常时可能忘记 unlock）。使用 RAII 类自动管理：

#### std::lock_guard

最简单的 RAII 锁，构造时加锁，析构时解锁：

```cpp
void increment() {
    for (int i = 0; i < 100000; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // 构造时加锁
        counter++;                               // 临界区
    }  // 析构时自动解锁
}
```

#### std::unique_lock

更灵活的锁，支持延迟加锁、手动解锁等：

```cpp
#include <mutex>
#include <thread>

std::mutex mtx;
int data = 0;
bool ready = false;

void producer() {
    std::unique_lock<std::mutex> lock(mtx);  // 立即加锁
    data = 42;
    ready = true;
    lock.unlock();  // 手动解锁（可选，析构时也会解锁）
}

void consumer() {
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);  // 延迟加锁
    // ... 做一些不需要锁的工作
    lock.lock();  // 现在加锁
    if (ready) {
        std::cout << data << "\n";
    }
}
```

**lock_guard vs unique_lock**：

| 特性 | lock_guard | unique_lock |
|:---|:---:|:---:|
| 自动加锁/解锁 | ✅ | ✅ |
| 延迟加锁 | ❌ | ✅ |
| 手动解锁 | ❌ | ✅ |
| 可移动 | ❌ | ✅ |
| 条件变量 | ❌ | ✅ |
| 性能 | 更快 | 稍慢 |

### 2.4 死锁演示与避免

**死锁**：两个线程互相等待对方释放锁，导致永久阻塞。

```cpp
#include <thread>
#include <mutex>
#include <iostream>

std::mutex mtx1;
std::mutex mtx2;

void thread_a() {
    std::lock_guard<std::mutex> lock1(mtx1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lock2(mtx2);  // 等待 mtx2
    std::cout << "Thread A got both locks\n";
}

void thread_b() {
    std::lock_guard<std::mutex> lock1(mtx2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lock2(mtx1);  // 等待 mtx1
    std::cout << "Thread B got both locks\n";
}

int main() {
    std::thread t1(thread_a);
    std::thread t2(thread_b);
    
    t1.join();  // 永远不会结束！
    t2.join();
    return 0;
}
```

**死锁示意图**：

```
线程A          线程B
  │              │
  ▼              ▼
锁定 mtx1     锁定 mtx2
  │              │
  ▼              ▼
等待 mtx2 ◄─── 等待 mtx1
 (阻塞)         (阻塞)
   └─────────────┘
      互相等待
```

**避免死锁的原则**：

1. **固定加锁顺序**：所有线程按相同顺序获取锁
2. **使用 std::lock**：同时获取多个锁
3. **避免嵌套锁**：一个线程不要同时持有多个锁

**正确示例 1：固定顺序**：

```cpp
void thread_a() {
    std::lock_guard<std::mutex> lock1(mtx1);
    std::lock_guard<std::mutex> lock2(mtx2);
    // ...
}

void thread_b() {
    std::lock_guard<std::mutex> lock1(mtx1);  // 和A相同顺序
    std::lock_guard<std::mutex> lock2(mtx2);
    // ...
}
```

**正确示例 2：使用 std::lock**：

```cpp
void safe_thread() {
    std::unique_lock<std::mutex> lock1(mtx1, std::defer_lock);
    std::unique_lock<std::mutex> lock2(mtx2, std::defer_lock);
    
    // 同时获取两个锁，避免死锁
    std::lock(lock1, lock2);
    // ...
}
```

---

## 3. 条件变量

### 3.1 生产者-消费者问题

经典的多线程问题：一个线程生产数据，一个线程消费数据。

**朴素实现（有缺陷）**：

```cpp
#include <queue>
#include <thread>
#include <mutex>

std::queue<int> queue;
std::mutex mtx;

void producer() {
    for (int i = 0; i < 100; i++) {
        std::lock_guard<std::mutex> lock(mtx);
        queue.push(i);
    }
}

void consumer() {
    while (true) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!queue.empty()) {
            int val = queue.front();
            queue.pop();
            // 处理 val
        }
        // 问题：队列为空时不断循环检查，浪费CPU（忙等待）
    }
}
```

### 3.2 wait 与 notify

使用条件变量解决忙等待问题：

```cpp
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>

std::queue<int> queue;
std::mutex mtx;
std::condition_variable cv;
bool done = false;

void producer() {
    for (int i = 0; i < 10; i++) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(i);
            std::cout << "Produced: " << i << "\n";
        }
        cv.notify_one();  // 通知消费者
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
    }
    cv.notify_all();  // 通知所有消费者结束
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 等待条件（自动释放锁，被唤醒后重新获取锁）
        cv.wait(lock, []() { return !queue.empty() || done; });
        
        if (!queue.empty()) {
            int val = queue.front();
            queue.pop();
            lock.unlock();  // 提前解锁，处理数据时不需要锁
            
            std::cout << "Consumed: " << val << "\n";
        } else if (done) {
            break;
        }
    }
}

int main() {
    std::thread p(producer);
    std::thread c(consumer);
    
    p.join();
    c.join();
    return 0;
}
```

**条件变量工作流程**：

```
生产者                    消费者
   │                        │
   ▼                        ▼
锁定 mtx                 锁定 mtx
   │                        │
   ▼                        ▼
放入数据              cv.wait() 检查条件
   │                   - 条件满足：继续
   ▼                   - 条件不满足：解锁，阻塞
notify_one() ◄───────────┘
   │                     (被唤醒)
   │                        │
   ▼                        ▼
解锁 mtx                 重新锁定 mtx
                          继续执行
```

### 3.3 虚假唤醒

**虚假唤醒**：条件变量的 `wait` 可能在没有 `notify` 的情况下返回。

```cpp
// 错误：使用 if 判断
if (!queue.empty()) {  // 可能被虚假唤醒，此时队列为空
    cv.wait(lock);
}

// 正确：使用 while 循环
while (!queue.empty()) {  // 唤醒后再次检查条件
    cv.wait(lock);
}
```

**C++11 推荐写法**：使用 Predicate 版本

```cpp
// 使用 Lambda 自动处理循环
cv.wait(lock, []() { return !queue.empty() || done; });

// 等价于：
while (!(!queue.empty() || done)) {
    cv.wait(lock);
}
```

### 3.4 超时等待

有时需要限制等待时间，避免永久阻塞：

```cpp
#include <chrono>

void consumer_with_timeout() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 等待最多 1 秒
        bool has_data = cv.wait_for(lock, 
                                     std::chrono::seconds(1),
                                     []() { return !queue.empty() || done; });
        
        if (has_data) {
            // 成功获取数据
            int val = queue.front();
            queue.pop();
            lock.unlock();
            std::cout << "Got: " << val << "\n";
        } else {
            // 超时
            std::cout << "Timeout, no data\n";
        }
        
        if (done && queue.empty()) break;
    }
}
```

**常用超时函数**：

| 函数 | 说明 |
|:---|:---|
| `wait_for` | 等待指定时长 |
| `wait_until` | 等待到指定时间点 |

---

## 4. 原子操作

### 4.1 std::atomic 基础

对于简单的计数器等场景，使用原子操作比互斥锁更高效：

```cpp
#include <atomic>
#include <thread>
#include <iostream>

std::atomic<int> counter{0};

void increment() {
    for (int i = 0; i < 100000; i++) {
        counter++;  // 原子操作，无需锁
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    
    t1.join();
    t2.join();
    
    std::cout << "Counter: " << counter << "\n";  // 正确：200000
    return 0;
}
```

**支持的类型**：
- 整数类型：`int`, `long`, `size_t` 等
- 指针类型：`T*`
- 布尔类型：`bool`
- 自定义类型（需满足 trivially copyable）

**原子操作**：

```cpp
std::atomic<int> val{0};

// 读
int x = val.load();

// 写
val.store(42);

// 自增/自减
val++;
val--;
++val;
--val;

// 加/减
val += 10;
val -= 5;

// 交换
int old = val.exchange(100);  // 设置新值，返回旧值

// 比较并交换（CAS）
int expected = 0;
bool success = val.compare_exchange_strong(expected, 42);
// 如果 val == expected，设置为 42，返回 true
// 否则 expected = val，返回 false
```

### 4.2 内存序简介

C++11 原子操作允许指定**内存序**，控制编译器和 CPU 的指令重排：

```cpp
// 默认：顺序一致性（最严格，最慢）
val.store(42);
val.store(42, std::memory_order_seq_cst);

// 其他内存序
val.store(42, std::memory_order_relaxed);  // 最宽松，最快
val.store(42, std::memory_order_release);
val.load(std::memory_order_acquire);
```

**内存序类型**：

| 内存序 | 说明 | 使用场景 |
|:---|:---|:---|
| `memory_order_relaxed` | 无同步保证 | 单纯计数器 |
| `memory_order_acquire` | 读取屏障 | 配合 release 使用 |
| `memory_order_release` | 写入屏障 | 发布数据 |
| `memory_order_acq_rel` | 读写屏障 | 读-修改-写操作 |
| `memory_order_seq_cst` | 全局顺序 | 默认，最安全 |

**简单规则**：
- 如果不懂内存序，用默认的 `seq_cst`
- 对于简单计数器，可以用 `relaxed` 获得更好性能

### 4.3 无锁计数器

实现高性能的无锁计数器：

```cpp
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>

class AtomicCounter {
public:
    void increment() {
        // relaxed 足够用于简单计数
        count_.fetch_add(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return count_.load(std::memory_order_relaxed);
    }
    
private:
    std::atomic<int> count_{0};
};

int main() {
    AtomicCounter counter;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < 100000; j++) {
                counter.increment();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final count: " << counter.get() << "\n";
    return 0;
}
```

---

## 5. 线程安全队列

### 5.1 设计目标

为音视频播放器设计一个线程安全的队列：

1. **多生产者-多消费者**：多个线程可以并发 push/pop
2. **阻塞等待**：队列为空时消费者阻塞，而非忙等待
3. **优雅停止**：支持安全地停止队列操作
4. **有界队列**：防止内存无限增长

### 5.2 双条件变量设计

使用两个条件变量分别处理**队列非空**和**队列非满**：

```
┌─────────────────────────────────────┐
│         ThreadSafeQueue             │
├─────────────────────────────────────┤
│  std::queue<T> queue_               │
│  std::mutex mtx_                    │
│  std::condition_variable not_empty_ │  ← 消费者等待
│  std::condition_variable not_full_  │  ← 生产者等待
│  bool stop_ = false                 │
│  size_t max_size_                   │
└─────────────────────────────────────┘
```

### 5.3 优雅停止

支持两种停止方式：
1. **立即停止**：清空队列，拒绝新任务
2. **优雅停止**：处理完队列中剩余的任务再停止

### 5.4 完整实现

```cpp
// include/live/threadsafe_queue.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace live {

template<typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_size = 0) 
        : max_size_(max_size), stop_(false) {}
    
    ~ThreadSafeQueue() {
        stop();
    }
    
    // 禁止拷贝和移动
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;
    
    // 入队，如果队列满则阻塞
    bool push(const T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // 等待队列非满
        not_full_.wait(lock, [this]() {
            return stop_ || max_size_ == 0 || queue_.size() < max_size_;
        });
        
        if (stop_) return false;
        
        queue_.push(value);
        not_empty_.notify_one();
        return true;
    }
    
    // 入队（移动语义）
    bool push(T&& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        not_full_.wait(lock, [this]() {
            return stop_ || max_size_ == 0 || queue_.size() < max_size_;
        });
        
        if (stop_) return false;
        
        queue_.push(std::move(value));
        not_empty_.notify_one();
        return true;
    }
    
    // 非阻塞入队
    bool try_push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (stop_) return false;
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;  // 队列满
        }
        
        queue_.push(value);
        not_empty_.notify_one();
        return true;
    }
    
    // 出队，如果队列空则阻塞
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // 等待队列非空
        not_empty_.wait(lock, [this]() {
            return stop_ || !queue_.empty();
        });
        
        if (stop_ && queue_.empty()) {
            return std::nullopt;
        }
        
        T value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return value;
    }
    
    // 超时出队
    template<typename Rep, typename Period>
    std::optional<T> pop_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        bool has_data = not_empty_.wait_for(lock, timeout, [this]() {
            return stop_ || !queue_.empty();
        });
        
        if (!has_data || (stop_ && queue_.empty())) {
            return std::nullopt;
        }
        
        T value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return value;
    }
    
    // 非阻塞出队
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return value;
    }
    
    // 停止队列，唤醒所有等待的线程
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // 重置队列状态
    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = false;
        // 清空队列
        while (!queue_.empty()) {
            queue_.pop();
        }
    }
    
    // 获取当前大小
    size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.size();
    }
    
    // 检查是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return queue_.empty();
    }

private:
    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    size_t max_size_;
    bool stop_;
};

} // namespace live
```

---

## 6. 多线程调试技巧

### 6.1 GDB 多线程调试

#### 基本命令

```bash
# 编译时添加调试信息
g++ -g -pthread thread_test.cpp -o thread_test

# 启动 GDB
gdb ./thread_test

# 常用命令
(gdb) run                    # 运行程序
(gdb) info threads           # 查看所有线程
(gdb) thread 2               # 切换到线程 2
(gdb) bt                     # 查看当前线程调用栈
(gdb) thread apply all bt    # 查看所有线程调用栈
(gdb) break main thread 1    # 在 main 线程设置断点
(gdb) set scheduler-locking on  # 锁定调度，只运行当前线程
(gdb) set scheduler-locking off # 恢复所有线程
```

#### 实际调试示例

```bash
$ gdb ./thread_test
(gdb) run
^C  # 按 Ctrl+C 中断
Program received signal SIGINT, Interrupt.
[Switching to Thread 0x7ffff6ffd700 (LWP 12345)]

(gdb) info threads
  Id   Target Id              Frame
* 1    Thread 0x7ffff7fe8740  main () at thread_test.cpp:45
  2    Thread 0x7ffff6ffd700  std::mutex::lock (this=0x6010c0) at mutex.cpp:123
  3    Thread 0x7ffff67fc700  pthread_cond_wait () at pthread_cond_wait.c:234

(gdb) thread 2
[Switching to thread 2]
(gdb) bt
#0  std::mutex::lock (this=0x6010c0) at mutex.cpp:123
#1  0x0000000000401a2b in worker_thread () at thread_test.cpp:23
#2  0x00007ffff7bc16ba in start_thread () from /libpthread.so.0

(gdb) thread apply all bt
# 查看所有线程的调用栈，找出死锁
```

### 6.2 ThreadSanitizer 检测数据竞争

ThreadSanitizer (TSan) 是编译器内置的工具，可以自动检测数据竞争。

#### 使用方法

```bash
# 使用 Clang 或 GCC 编译（推荐 Clang，报告更详细）
clang++ -fsanitize=thread -g -O1 thread_test.cpp -o thread_test

# 运行程序，TSan 会自动检测问题
./thread_test
```

#### 检测示例

**问题代码**：

```cpp
#include <thread>

int shared_data = 0;

void increment() {
    for (int i = 0; i < 10000; i++) {
        shared_data++;  // 数据竞争！
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    return 0;
}
```

**TSan 报告**：

```
$ ./thread_test
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x0000014bc0a0 by thread T1:
    #0 increment() thread_test.cpp:7:9
    #1 decltype(std::declval<void (*)()>()) std::thread::_Invoker<...>::_M_invoke<...>

  Previous write of size 4 at 0x0000014bc0a0 by thread T2:
    #0 increment() thread_test.cpp:7:9
    #1 decltype(std::declval<void (*)()>()) std::thread::_Invoker<...>::_M_invoke<...>

  Location is global 'shared_data' of size 4 at 0x0000014bc0a0

SUMMARY: ThreadSanitizer: data race thread_test.cpp:7:9 in increment()
==================
```

**修复后**：

```cpp
#include <thread>
#include <atomic>

std::atomic<int> shared_data{0};  // 使用原子变量

void increment() {
    for (int i = 0; i < 10000; i++) {
        shared_data++;
    }
}
// ...
```

### 6.3 常见问题排查

#### 死锁排查清单

1. **检查锁的顺序**：所有线程是否按相同顺序获取锁？
2. **检查回调函数**：锁内是否调用了可能获取其他锁的函数？
3. **检查异常安全**：异常时锁是否正确释放？
4. **使用 `std::lock`**：同时获取多个锁时使用

#### 性能问题排查

```bash
# 使用 perf 分析线程性能
perf record -g ./thread_test
perf report

# 使用 strace 查看系统调用
strace -f -e futex ./thread_test
```

#### 调试技巧总结

| 问题 | 工具/方法 |
|:---|:---|
| 死锁 | GDB `info threads` + `bt` |
| 数据竞争 | ThreadSanitizer |
| 锁竞争 | perf, Intel VTune |
| 内存问题 | AddressSanitizer, Valgrind |

---

## 7. 本章总结

### 知识点回顾

| 主题 | 核心内容 |
|:---|:---|
| **线程基础** | `std::thread`、join/detach、线程ID |
| **互斥锁** | `std::mutex`、`lock_guard`、`unique_lock` |
| **条件变量** | `wait`/`notify`、虚假唤醒、超时等待 |
| **原子操作** | `std::atomic`、内存序基础 |
| **线程安全队列** | 双条件变量、优雅停止 |
| **调试技巧** | GDB多线程、TSan检测 |

### 最佳实践

1. **优先使用高阶抽象**：
   - `lock_guard` 比手动 `lock/unlock` 更安全
   - `std::atomic` 比 `mutex` 更高效（简单场景）

2. **避免常见陷阱**：
   - 不要在锁内调用外部函数
   - 总是使用 while 循环检查条件变量
   - 注意 lambda 捕获的生命周期

3. **调试优先**：
   - 开发时用 TSan 检测数据竞争
   - 掌握 GDB 多线程调试技巧

### 下一步

第六章将使用本章学到的多线程技术，构建**异步播放器**：
- 使用线程安全队列在解码线程和渲染线程间传递数据
- 使用条件变量实现平滑的帧同步
- 使用原子操作管理播放状态

---

## 附录：示例代码索引

| 文件名 | 说明 |
|:---|:---|
| `01_hello_thread.cpp` | 基础线程创建 |
| `02_mutex_basics.cpp` | 互斥锁基础 |
| `03_deadlock_demo.cpp` | 死锁演示 |
| `04_condition_variable.cpp` | 条件变量使用 |
| `05_atomic_counter.cpp` | 原子操作示例 |
| `06_threadsafe_queue_demo.cpp` | 线程安全队列使用 |
| `07_data_race_tsan.cpp` | TSan 检测示例 |

**编译所有示例**：

```bash
cd chapter-05
mkdir build && cd build
cmake ..
make
```

---

**本章完** 🎉

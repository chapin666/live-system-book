# 附录 A：C++11 线程速成

> 写给 Ch2 异步播放器读者的前置知识补充。

如果你不熟悉 C++11 的多线程编程，这份附录会帮你快速掌握 Ch2 所需的基础知识。

---

## 1. 为什么需要多线程？

**单线程的问题**：
```cpp
// 单线程：解码和渲染串行执行
while (true) {
    decode_frame();   // 耗时 10ms
    render_frame();   // 耗时 5ms
    // 总耗时：15ms/帧，最高 66fps
}
```

**多线程的解决**：
```cpp
// 线程A：专门解码
void decode_thread() {
    while (true) {
        decode_frame();
        queue.push(frame);  // 放入队列
    }
}

// 线程B：专门渲染
void render_thread() {
    while (true) {
        frame = queue.pop();  // 从队列取
        render_frame();
    }
}
```

---

## 2. 创建线程

```cpp
#include <thread>
#include <iostream>

void hello() {
    std::cout << "Hello from thread\n";
}

int main() {
    // 创建线程
    std::thread t(hello);
    
    // 等待线程结束
    t.join();
    
    std::cout << "Main thread done\n";
    return 0;
}
```

**关键点**：
- `std::thread` 创建新线程
- `join()` 等待线程完成（主线程会阻塞在这里）
- `detach()` 让线程独立运行（主线程不等待）

---

## 3. 传递参数

```cpp
void print_number(int n, const std::string& label) {
    std::cout << label << ": " << n << "\n";
}

int main() {
    // 传递参数给线程函数
    std::thread t(print_number, 42, "The answer");
    t.join();
    return 0;
}
```

---

## 4. 互斥锁（Mutex）

**问题：多线程访问共享数据会冲突**

```cpp
int counter = 0;

void increment() {
    for (int i = 0; i < 1000; i++) {
        counter++;  // 危险！多个线程同时修改
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);
    t1.join();
    t2.join();
    // counter 可能不是 2000！
    std::cout << counter << "\n";
    return 0;
}
```

**解决：使用互斥锁**

```cpp
#include <mutex>

int counter = 0;
std::mutex mtx;  // 互斥锁

void increment() {
    for (int i = 0; i < 1000; i++) {
        mtx.lock();     // 加锁
        counter++;      // 临界区：只有一个线程能执行
        mtx.unlock();   // 解锁
    }
}
```

**更好的方式：lock_guard（自动解锁）**

```cpp
void increment() {
    for (int i = 0; i < 1000; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // 构造时加锁
        counter++;                               // 临界区
        // 析构时自动解锁
    }
}
```

**为什么用 lock_guard？**
- 自动管理锁的生命周期
- 避免忘记 unlock
- 异常安全（抛出异常也会解锁）

---

## 5. 条件变量（Condition Variable）

**场景：线程等待某个条件**

```cpp
#include <condition_variable>

std::queue<int> queue;
std::mutex mtx;
std::condition_variable cv;

// 生产者线程
void producer() {
    for (int i = 0; i < 10; i++) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push(i);
        std::cout << "Produced: " << i << "\n";
        lock.unlock();
        cv.notify_one();  // 通知消费者
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// 消费者线程
void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 等待队列非空
        cv.wait(lock, [] { return !queue.empty(); });
        
        int value = queue.front();
        queue.pop();
        lock.unlock();
        
        std::cout << "Consumed: " << value << "\n";
        
        if (value == 9) break;
    }
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    return 0;
}
```

**关键点**：
- `cv.wait(lock, predicate)`：自动解锁等待，条件满足时加锁返回
- `cv.notify_one()`：唤醒一个等待的线程
- `cv.notify_all()`：唤醒所有等待的线程

---

## 6. Ch2 中的 FrameQueue 简化解说

```cpp
class FrameQueue {
public:
    void Push(AVFrame* frame) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列不满
        not_full_.wait(lock, [this] { return queue_.size() < max_size_; });
        
        queue_.push(frame);
        not_empty_.notify_one();  // 通知消费者
    }
    
    AVFrame* Pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列非空
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        
        AVFrame* frame = queue_.front();
        queue_.pop();
        not_full_.notify_one();  // 通知生产者
        return frame;
    }

private:
    std::queue<AVFrame*> queue_;
    std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    size_t max_size_ = 3;
};
```

**核心逻辑**：
1. `Push`：队列满时等待（`not_full_`），放入数据后通知消费者（`not_empty_`）
2. `Pop`：队列空时等待（`not_empty_`），取出数据后通知生产者（`not_full_`）

这就是**生产者-消费者模式**。

---

## 7. 常见问题

### Q1: 什么是线程安全？

**线程安全**：多个线程同时访问时，结果正确的代码。

**例子**：
```cpp
// 线程不安全
void unsafe_increment() {
    counter++;  // 实际上是：读-修改-写，三步操作
}

// 线程安全
void safe_increment() {
    std::lock_guard<std::mutex> lock(mtx);
    counter++;
}
```

### Q2: 死锁是什么？

**死锁**：两个线程互相等待对方释放锁。

```cpp
// 线程A：先锁mtx1，再锁mtx2
// 线程B：先锁mtx2，再锁mtx1
// 结果：A等B释放mtx2，B等A释放mtx1，永远等待

// 解决：始终按相同顺序加锁
```

### Q3: 为什么用 lambda？

```cpp
// 传统写法：定义一个函数
bool is_not_empty() { return !queue.empty(); }
cv.wait(lock, is_not_empty);

// Lambda 写法：内联定义
cv.wait(lock, [] { return !queue.empty(); });
//         ↑ lambda 表达式
```

Lambda 让你可以在代码中直接写小函数，不用到处定义。

---

## 8. 总结

| 概念 | 作用 | Ch2 中哪里用 |
|:---|:---|:---|
| `std::thread` | 创建线程 | `DecoderThread` |
| `std::mutex` | 保护共享数据 | `FrameQueue` 的锁 |
| `std::lock_guard` | 自动管理锁 | `Push`/`Pop` 函数 |
| `std::condition_variable` | 线程间等待/通知 | 队列满/空的等待 |

**学完本节，你应该能理解**：
- Ch2 为什么要用多线程
- `FrameQueue` 的工作原理
- 为什么需要锁和条件变量

如果还有疑问，建议先写几个简单的多线程程序练习，再回来看 Ch2。

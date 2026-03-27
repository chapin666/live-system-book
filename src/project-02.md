# 项目实战2：网络点播播放器

> **前置要求**：完成 Chapter 4-6
> **目标**：实现支持 HTTP/HTTPS 的网络视频播放器

## 项目概述

本项目将播放器扩展为支持**网络视频点播**，学习如何处理：
- HTTP 渐进下载播放
- 网络缓冲管理
- 断网重连
- 下载进度显示

## 功能需求

### 网络播放
- [x] 支持 HTTP/HTTPS URL 播放
- [x] 自动识别本地文件 vs 网络流
- [x] 网络超时处理

### 缓冲管理
- [x] 显示缓冲进度
- [x] 可调节缓冲区大小
- [x] 网络卡顿时自动缓冲

### 错误恢复
- [x] 网络断开检测
- [x] 自动重连（3次重试）
- [x] 错误提示（网络不可用、404等）

## 关键技术

### HTTP 播放原理

```
HTTP Range 请求：
GET /video.mp4 HTTP/1.1
Range: bytes=0-1023

服务器响应：
HTTP/1.1 206 Partial Content
Content-Range: bytes 0-1023/1000000

FFmpeg 自动处理 Range 请求，实现边下边播
```

### 缓冲策略

```
┌─────────────────────────────────────────┐
│           网络缓冲区                     │
│  ┌─────────────────────────────────┐   │
│  │ 已下载 |=======          | 待下载 │   │
│  └─────────────────────────────────┘   │
│       ↓                                 │
│  缓冲水位线: 2秒数据                      │
│       ↓                                 │
│  足够时开始播放                           │
└─────────────────────────────────────────┘
```

### 代码示例：网络检测

```cpp
bool IsNetworkUrl(const char* url) {
    return strncmp(url, "http://", 7) == 0 ||
           strncmp(url, "https://", 8) == 0;
}

void SetNetworkOptions(AVDictionary** opts) {
    // 连接超时 10 秒
    av_dict_set(opts, "timeout", "10000000", 0);
    // 读取超时 5 秒
    av_dict_set(opts, "rw_timeout", "5000000", 0);
    // TCP 无延迟
    av_dict_set(opts, "tcp_nodelay", "1", 0);
}
```

### 代码示例：重连机制

```cpp
class NetworkPlayer {
public:
    bool PlayWithRetry(const char* url) {
        for (int i = 0; i < max_retries_; i++) {
            if (TryPlay(url)) {
                return true;
            }
            std::cerr << "重试 " << (i + 1) << "/" << max_retries_ << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        return false;
    }
    
private:
    bool TryPlay(const char* url) {
        // 打开输入
        int ret = avformat_open_input(&fmt_ctx_, url, nullptr, &opts_);
        if (ret < 0) {
            HandleError(ret);
            return false;
        }
        // ... 播放逻辑
        return true;
    }
    
    void HandleError(int error_code) {
        switch (error_code) {
            case AVERROR_EXIT:
                std::cerr << "连接被中断" << std::endl;
                break;
            case AVERROR(ETIMEDOUT):
                std::cerr << "连接超时" << std::endl;
                break;
            default:
                char errbuf[256];
                av_strerror(error_code, errbuf, sizeof(errbuf));
                std::cerr << "错误: " << errbuf << std::endl;
        }
    }
};
```

## 项目结构

```
project-02/
├── CMakeLists.txt
├── README.md
├── include/
│   └── live/
│       ├── network_player.h
│       └── ... (继承 project-01)
└── src/
    ├── main.cpp
    └── network_player.cpp
```

## 运行测试

```bash
# 测试网络播放
./player "https://example.com/video.mp4"

# 测试本地文件（向后兼容）
./player test.mp4
```

## 验收标准

- [ ] 能播放 HTTP/HTTPS 视频
- [ ] 网络断开时能自动重连
- [ ] 显示缓冲进度
- [ ] 错误提示清晰（超时、404、网络不可用）

## 扩展挑战

1. 支持 FTP 协议
2. 实现边下边存（下载同时保存到本地）
3. 支持多线程下载加速

---

**完成本项目后，你将掌握：**
- FFmpeg 网络选项配置
- 网络超时和错误处理
- 自动重连机制
- HTTP 流播放原理

# 第十四章：高级采集技术

> **本章目标**：掌握屏幕采集、窗口采集、多摄像头切换，实现专业级采集能力。

第十章实现了基础的摄像头和麦克风采集。但专业直播场景需要更多能力：
- **屏幕采集**：游戏直播、在线教学
- **窗口采集**：仅采集特定应用窗口
- **多摄像头**：主摄像头 + 特写摄像头切换
- **采集优化**：分辨率适配、帧率控制

**阅读指南**：
- 第 1-2 节：屏幕采集原理与实现
- 第 3-4 节：窗口采集、区域采集
- 第 5-6 节：多摄像头管理、画中画
- 第 7-8 节：采集参数优化、性能调优

---

## 目录

1. [屏幕采集原理](#1-屏幕采集原理)
2. [跨平台屏幕采集实现](#2-跨平台屏幕采集实现)
3. [窗口采集与区域采集](#3-窗口采集与区域采集)
4. [采集光标与叠加层](#4-采集光标与叠加层)
5. [多摄像头管理](#5-多摄像头管理)
6. [画中画与多路合成](#6-画中画与多路合成)
7. [采集参数动态调整](#7-采集参数动态调整)
8. [性能优化](#8-性能优化)
9. [本章总结](#9-本章总结)

---

## 1. 屏幕采集原理

### 1.1 屏幕采集 vs 摄像头采集

| 特性 | 摄像头采集 | 屏幕采集 |
|:---|:---|:---|
| 数据源 | 摄像头设备 | 显卡帧缓冲 |
| 分辨率 | 固定（1920x1080 等） | 随显示器变化 |
| 帧率 | 30/60fps | 与显示器刷新率同步 |
| 延迟 | 10-50ms | 1-5ms |
| CPU 占用 | 低 | 高（需要拷贝数据） |

### 1.2 屏幕采集的挑战

**分辨率高**：
- 4K 显示器：3840x2160 = 829万像素
- 每帧原始数据：12 MB（RGB24）
- 60fps 时：720 MB/s 的数据量

**性能优化方向**：
- 使用 GPU 纹理共享，避免 CPU 拷贝
- 降低采集分辨率（如 1080p）
- 限制采集帧率（如 30fps）

---

## 2. 跨平台屏幕采集实现

### 2.1 macOS 屏幕采集

使用 `CGDisplayStream` API：

```cpp
#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>

class MacOSScreenCapture {
public:
    bool Init(int display_id = kCGDirectMainDisplay) {
        display_id_ = display_id;
        
        // 获取显示器尺寸
        CGRect bounds = CGDisplayBounds(display_id);
        width_ = CGRectGetWidth(bounds);
        height_ = CGRectGetHeight(bounds);
        
        // 创建显示流
        CGDisplayStreamFrameAvailableHandler handler = 
            ^(CGDisplayStreamFrameStatus status,
              uint64_t display_time,
              IOSurfaceRef frame_surface,
              CGDisplayStreamUpdateRef update_ref) {
            if (status == kCGDisplayStreamFrameStatusFrameComplete && frame_surface) {
                OnFrame(frame_surface);
            }
        };
        
        display_stream_ = CGDisplayStreamCreate(
            display_id,
            width_, height_,
            kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
            nullptr,  // options
            handler
        );
        
        return display_stream_ != nullptr;
    }
    
    bool Start() {
        CGError err = CGDisplayStreamStart(display_stream_);
        return err == kCGErrorSuccess;
    }
    
    void Stop() {
        if (display_stream_) {
            CGDisplayStreamStop(display_stream_);
        }
    }
    
private:
    void OnFrame(IOSurfaceRef surface) {
        // 获取 YUV 数据
        IOSurfaceLock(surface, kIOSurfaceLockReadOnly, nullptr);
        
        size_t plane_count = IOSurfaceGetPlaneCount(surface);
        for (size_t i = 0; i < plane_count; i++) {
            void* base = IOSurfaceGetBaseAddressOfPlane(surface, i);
            size_t width = IOSurfaceGetWidthOfPlane(surface, i);
            size_t height = IOSurfaceGetHeightOfPlane(surface, i);
            size_t stride = IOSurfaceGetBytesPerRowOfPlane(surface, i);
            
            // 拷贝数据或直接使用
            // ...
        }
        
        IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
    }
    
    CGDisplayStreamRef display_stream_ = nullptr;
    CGDirectDisplayID display_id_;
    int width_, height_;
};
```

### 2.2 Linux 屏幕采集

使用 FFmpeg `x11grab`：

```cpp
class LinuxScreenCapture {
public:
    bool Init(const char* display = ":0.0", int x = 0, int y = 0, 
              int width = 1920, int height = 1080) {
        // 使用 FFmpeg x11grab
        AVInputFormat* ifmt = av_find_input_format("x11grab");
        if (!ifmt) {
            std::cerr << "x11grab 不可用" << std::endl;
            return false;
        }
        
        // 构造输入字符串：:0.0+100,200
        char input_str[256];
        snprintf(input_str, sizeof(input_str), "%s+%d,%d", display, x, y);
        
        AVFormatContext* fmt_ctx = nullptr;
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "video_size", 
                    (std::to_string(width) + "x" + std::to_string(height)).c_str(), 0);
        av_dict_set(&opts, "framerate", "30", 0);
        av_dict_set(&opts, "draw_mouse", "1", 0);  // 采集光标
        
        int ret = avformat_open_input(&fmt_ctx, input_str, ifmt, &opts);
        av_dict_free(&opts);
        
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            std::cerr << "打开屏幕失败: " << errbuf << std::endl;
            return false;
        }
        
        fmt_ctx_ = fmt_ctx;
        return true;
    }
    
    bool ReadFrame(AVFrame* frame) {
        AVPacket packet;
        int ret = av_read_frame(fmt_ctx_, &packet);
        if (ret < 0) return false;
        
        // 解码为 AVFrame
        // ...
        
        av_packet_unref(&packet);
        return true;
    }
    
private:
    AVFormatContext* fmt_ctx_ = nullptr;
};
```

**Wayland 支持**：
- Wayland 使用 `pipewire` 或 `wlroots` screencopy 协议
- 需要 compositor 支持

### 2.3 性能优化：降低采集负载

```cpp
class OptimizedScreenCapture {
public:
    // 限制采集区域
    void SetCaptureRegion(int x, int y, int w, int h) {
        capture_x_ = x;
        capture_y_ = y;
        capture_width_ = w;
        capture_height_ = h;
    }
    
    // 降低采集帧率
    void SetTargetFPS(int fps) {
        target_fps_ = fps;
        frame_interval_ms_ = 1000 / fps;
    }
    
    // 降低输出分辨率
    void SetOutputResolution(int width, int height) {
        output_width_ = width;
        output_height_ = height;
        // 后续使用 libswscale 缩放
    }
    
private:
    int capture_x_ = 0, capture_y_ = 0;
    int capture_width_ = 1920, capture_height_ = 1080;
    int output_width_ = 1280, output_height_ = 720;
    int target_fps_ = 30;
    int frame_interval_ms_ = 33;
};
```

---

## 3. 窗口采集与区域采集

### 3.1 窗口采集

只采集特定应用窗口，而非整个屏幕：

**macOS 窗口采集**：
```cpp
// 使用 CGWindowListCreateImage
CGImageRef CaptureWindow(CGWindowID window_id) {
    CGRect bounds;
    bounds.origin = CGPointZero;
    bounds.size = CGDisplayBounds(kCGDirectMainDisplay).size;
    
    CGImageRef image = CGWindowListCreateImage(
        bounds,
        kCGWindowListOptionIncludingWindow,
        window_id,
        kCGWindowImageBoundsIgnoreFraming
    );
    
    return image;
}
```

**Linux 窗口采集（X11）**：
```cpp
// 获取窗口 ID
Window GetWindowByName(Display* display, const char* name) {
    // 遍历窗口树，匹配窗口名称
    // ...
}

// FFmpeg 采集特定窗口
char input_str[256];
snprintf(input_str, sizeof(input_str), ":0.0+%d,%d", window_x, window_y);
```

### 3.2 区域采集

采集屏幕的某个矩形区域：

```cpp
class RegionCapture {
public:
    bool Init(int screen_x, int screen_y, int width, int height) {
        // Linux: x11grab 支持偏移
        // macOS: 需要裁剪采集后的图像
        // Windows: DXGI 支持指定输出区域
        
        region_x_ = screen_x;
        region_y_ = screen_y;
        region_width_ = width;
        region_height_ = height;
        
        return true;
    }
    
private:
    int region_x_, region_y_;
    int region_width_, region_height_;
};
```

---

## 4. 采集光标与叠加层

### 4.1 光标采集

**方案 1：系统采集光标**（推荐）
```cpp
// FFmpeg x11grab draw_mouse=1
av_dict_set(&opts, "draw_mouse", "1", 0);
```

**方案 2：手动绘制光标**
```cpp
class CursorOverlay {
public:
    void DrawCursor(uint8_t* frame_data, int x, int y) {
        // 获取光标图像
        Cursor cursor = XCreateFontCursor(display_, XC_arrow);
        XImage* image = XGetImage(display_, cursor, ...);
        
        // 叠加到帧
        OverlayImage(frame_data, image, x, y);
    }
};
```

### 4.2 叠加层（Watermark）

在采集画面上叠加文字或图片：

```cpp
class FrameOverlay {
public:
    void OverlayText(uint8_t* yuv_data, const char* text, int x, int y) {
        // 使用 FreeType 渲染文字到 RGB
        // 转换为 YUV
        // 叠加到目标位置
    }
    
    void OverlayImage(uint8_t* yuv_data, const uint8_t* rgba_overlay,
                      int overlay_x, int overlay_y, int overlay_w, int overlay_h) {
        // RGBA → YUV
        // Alpha 混合
        for (int row = 0; row < overlay_h; row++) {
            for (int col = 0; col < overlay_w; col++) {
                int src_idx = (row * overlay_w + col) * 4;
                float alpha = rgba_overlay[src_idx + 3] / 255.0f;
                
                // Y 通道混合
                int dst_y = ((overlay_y + row) * frame_width_ + (overlay_x + col));
                yuv_data[dst_y] = Blend(yuv_data[dst_y], rgba_overlay[src_idx], alpha);
            }
        }
    }
    
private:
    uint8_t Blend(uint8_t bg, uint8_t fg, float alpha) {
        return static_cast<uint8_t>(bg * (1 - alpha) + fg * alpha);
    }
    
    int frame_width_, frame_height_;
};
```

---

## 5. 多摄像头管理

### 5.1 枚举摄像头设备

```cpp
class CameraManager {
public:
    struct DeviceInfo {
        std::string id;
        std::string name;
        std::vector<std::pair<int, int>> resolutions;
    };
    
    std::vector<DeviceInfo> ListDevices() {
        std::vector<DeviceInfo> devices;
        
        // macOS: AVFoundation
        // Linux: v4l2
        // Windows: DirectShow/MediaFoundation
        
#if defined(__APPLE__)
        devices = ListAVFoundationDevices();
#elif defined(__linux__)
        devices = ListV4L2Devices();
#endif
        
        return devices;
    }
    
private:
#if defined(__APPLE__)
    std::vector<DeviceInfo> ListAVFoundationDevices() {
        std::vector<DeviceInfo> devices;
        
        AVCaptureDeviceDiscoverySession* session =
            [AVCaptureDeviceDiscoverySession 
                discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera]
                mediaType:AVMediaTypeVideo
                position:AVCaptureDevicePositionUnspecified];
        
        for (AVCaptureDevice* device in session.devices) {
            DeviceInfo info;
            info.id = [[device uniqueID] UTF8String];
            info.name = [[device localizedName] UTF8String];
            
            // 获取支持的分辨率
            for (AVCaptureDeviceFormat* format in device.formats) {
                CMVideoDimensions dims = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
                info.resolutions.push_back({dims.width, dims.height});
            }
            
            devices.push_back(info);
        }
        
        return devices;
    }
#endif
};
```

### 5.2 多摄像头切换

```cpp
class MultiCameraCapture {
public:
    bool AddCamera(const std::string& device_id) {
        auto capture = std::make_unique<CameraCapture>();
        if (!capture->Init(device_id)) {
            return false;
        }
        cameras_.push_back(std::move(capture));
        return true;
    }
    
    void SwitchToCamera(size_t index) {
        if (index < cameras_.size()) {
            active_camera_ = index;
        }
    }
    
    bool ReadFrame(AVFrame* frame) {
        if (active_camera_ >= cameras_.size()) return false;
        return cameras_[active_camera_]->ReadFrame(frame);
    }
    
private:
    std::vector<std::unique_ptr<CameraCapture>> cameras_;
    size_t active_camera_ = 0;
};
```

---

## 6. 画中画与多路合成

### 6.1 画中画（Picture-in-Picture）

主画面 + 小窗口（通常是摄像头）：

```cpp
class PictureInPicture {
public:
    void SetLayout(int main_width, int main_height,
                   int pip_width, int pip_height,
                   int pip_x, int pip_y) {
        main_width_ = main_width;
        main_height_ = main_height;
        pip_width_ = pip_width;
        pip_height_ = pip_height;
        pip_x_ = pip_x;
        pip_y_ = pip_y;
    }
    
    void Compose(uint8_t* output, 
                 const uint8_t* main_frame,
                 const uint8_t* pip_frame) {
        // 1. 拷贝主画面
        memcpy(output, main_frame, main_width_ * main_height_ * 3 / 2);
        
        // 2. 缩放并叠加小窗口
        SwsContext* sws = sws_getContext(
            pip_width_, pip_height_, AV_PIX_FMT_YUV420P,
            pip_width_, pip_height_, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        // 叠加到指定位置
        OverlayYUV(output, main_width_, main_height_,
                   pip_frame, pip_width_, pip_height_,
                   pip_x_, pip_y_);
        
        sws_freeContext(sws);
    }
    
private:
    void OverlayYUV(uint8_t* dst, int dst_w, int dst_h,
                    const uint8_t* src, int src_w, int src_h,
                    int x, int y) {
        // Y 平面
        for (int row = 0; row < src_h && (y + row) < dst_h; row++) {
            memcpy(dst + (y + row) * dst_w + x,
                   src + row * src_w,
                   std::min(src_w, dst_w - x));
        }
        
        // UV 平面（半分辨率）
        // ...
    }
    
    int main_width_, main_height_;
    int pip_width_, pip_height_;
    int pip_x_, pip_y_;
};
```

### 6.2 多路画面合成

视频会议布局：

```cpp
class MultiViewComposer {
public:
    enum Layout {
        GRID_2x2,    // 2x2 网格
        GRID_3x3,    // 3x3 网格
        SPEAKER,     // 主讲人大，其他小
        FILMSTRIP    // 底部胶片条
    };
    
    void SetLayout(Layout layout) { layout_ = layout; }
    
    void Compose(uint8_t* output,
                 const std::vector<const uint8_t*>& input_frames,
                 const std::vector<std::pair<int, int>>& input_sizes) {
        switch (layout_) {
            case GRID_2x2:
                ComposeGrid2x2(output, input_frames, input_sizes);
                break;
            case SPEAKER:
                ComposeSpeaker(output, input_frames, input_sizes);
                break;
            // ...
        }
    }
    
private:
    void ComposeGrid2x2(uint8_t* output,
                        const std::vector<const uint8_t*>& frames,
                        const std::vector<std::pair<int, int>>& sizes) {
        // 每个画面 960x540（1920x1080 的四分之一）
        int cell_w = 960;
        int cell_h = 540;
        
        for (size_t i = 0; i < frames.size() && i < 4; i++) {
            int x = (i % 2) * cell_w;
            int y = (i / 2) * cell_h;
            
            // 缩放并放置到对应位置
            ScaleAndPlace(output, 1920, 1080, x, y, cell_w, cell_h,
                          frames[i], sizes[i].first, sizes[i].second);
        }
    }
    
    Layout layout_ = GRID_2x2;
};
```

---

## 7. 采集参数动态调整

### 7.1 分辨率切换

根据性能/网络动态调整采集分辨率：

```cpp
class AdaptiveCapture {
public:
    void OnCPULoadHigh() {
        // CPU 负载高，降低分辨率
        if (current_resolution_idx_ > 0) {
            current_resolution_idx_--;
            ApplyResolution();
        }
    }
    
    void OnNetworkImproved() {
        // 网络变好，提高分辨率
        if (current_resolution_idx_ < resolutions_.size() - 1) {
            current_resolution_idx_++;
            ApplyResolution();
        }
    }
    
private:
    void ApplyResolution() {
        auto [width, height] = resolutions_[current_resolution_idx_];
        capture_>SetResolution(width, height);
    }
    
    std::vector<std::pair<int, int>> resolutions_ = {
        {640, 360},   // 360p
        {854, 480},   // 480p
        {1280, 720},  // 720p
        {1920, 1080}  // 1080p
    };
    size_t current_resolution_idx_ = 2;  // 默认 720p
};
```

### 7.2 帧率自适应

```cpp
class AdaptiveFrameRate {
public:
    void SetTargetFPS(int fps) {
        target_fps_ = fps;
        frame_interval_us_ = 1000000 / fps;
    }
    
    bool ShouldCaptureFrame() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - last_capture_time_
        ).count();
        
        if (elapsed >= frame_interval_us_) {
            last_capture_time_ = now;
            return true;
        }
        return false;
    }
    
private:
    int target_fps_ = 30;
    int64_t frame_interval_us_ = 33333;
    std::chrono::steady_clock::time_point last_capture_time_;
};
```

---

## 8. 性能优化

### 8.1 GPU 零拷贝

使用 GPU 纹理避免 CPU 拷贝：

```cpp
// macOS: IOSurface / CVPixelBuffer
// Linux: DMA-BUF / GBM
// Windows: DXGI Shared Handle

class GPUZeroCopy {
public:
    // 获取 GPU 纹理句柄，不拷贝到 CPU
    void* GetGPUTexture() {
#if defined(__APPLE__)
        return iosurface_ref_;
#elif defined(__linux__)
        return dma_buf_fd_;
#endif
    }
    
private:
#if defined(__APPLE__)
    IOSurfaceRef iosurface_ref_ = nullptr;
#elif defined(__linux__)
    int dma_buf_fd_ = -1;
#endif
};
```

### 8.2 采集性能监控

```cpp
class CaptureStats {
public:
    void OnFrameCaptured() {
        auto now = std::chrono::steady_clock::now();
        frame_count_++;
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_
        ).count();
        
        if (elapsed >= 5) {  // 每 5 秒输出统计
            float fps = frame_count_ / elapsed;
            std::cout << "采集 FPS: " << fps << std::endl;
            
            frame_count_ = 0;
            start_time_ = now;
        }
    }
    
private:
    int frame_count_ = 0;
    std::chrono::steady_clock::time_point start_time_ = 
        std::chrono::steady_clock::now();
};
```

---

## 9. 本章总结

### 核心技能

| 技能 | 实现要点 |
|:---|:---|
| 屏幕采集 | CGDisplayStream (macOS), x11grab (Linux) |
| 窗口采集 | CGWindowListCreateImage, X11 窗口 ID |
| 多摄像头 | 设备枚举、动态切换 |
| 画中画 | YUV 层叠、缩放放置 |
| 性能优化 | 分辨率/帧率自适应、GPU 零拷贝 |

### 关键参数

| 参数 | 建议值 | 说明 |
|:---|:---|:---|
| 屏幕采集分辨率 | 1080p 或更低 | 4K 采集 CPU 占用高 |
| 屏幕采集帧率 | 30fps | 60fps 数据量翻倍 |
| 画中画大小 | 240x180 ~ 320x240 | 不遮挡主画面 |
| 分辨率切换阈值 | CPU > 80% 降级 | 平滑过渡 |

### 下一步

**项目4：采集与预览工具** —— 整合 Ch10-Ch14，实现专业级采集预览工具。

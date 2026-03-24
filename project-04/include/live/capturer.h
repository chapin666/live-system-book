#pragma once
#include <memory>
#include <functional>
#include <stdint.h>

namespace live {

// 采集设备信息
struct DeviceInfo {
    int id;
    char name[256];
    int width;
    int height;
    int fps;
};

// 采集器接口
class ICapturer {
public:
    virtual ~ICapturer() = default;
    
    // 初始化和释放
    virtual bool Init(int device_id, int width, int height, int fps) = 0;
    virtual void Close() = 0;
    
    // 开始/停止采集
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    
    // 回调：每帧数据
    std::function<void(const uint8_t* data, int size, int width, int height)> on_frame;
};

// 创建视频采集器
std::unique_ptr<ICapturer> CreateVideoCapturer();

// 创建音频采集器
std::unique_ptr<ICapturer> CreateAudioCapturer();

// 预览窗口
class IPreview {
public:
    virtual ~IPreview() = default;
    virtual bool Init(int width, int height) = 0;
    virtual void Close() = 0;
    virtual void Render(const uint8_t* data, int width, int height) = 0;
    virtual bool PollEvents() = 0;  // 返回false表示用户关闭
};

std::unique_ptr<IPreview> CreatePreview();

} // namespace live
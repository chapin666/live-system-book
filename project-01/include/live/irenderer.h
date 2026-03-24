#pragma once
#include <cstdint>

namespace live {

// 渲染器接口
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // 初始化窗口
    virtual bool Init(int width, int height, const char* title) = 0;
    virtual void Close() = 0;
    
    // 渲染一帧
    virtual void RenderFrame(void* frame) = 0;
    
    // 处理事件（返回false表示用户请求退出）
    virtual bool PollEvents() = 0;
    
    // 设置窗口标题
    virtual void SetTitle(const char* title) = 0;
};

// 创建SDL渲染器
std::unique_ptr<IRenderer> CreateSDLRenderer();

} // namespace live
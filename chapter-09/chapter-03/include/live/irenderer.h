#pragma once

#include "live/idemuxer.h"

struct AVFrame;

namespace live {

// 渲染器接口
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // 初始化渲染器
    virtual ErrorCode Init(int width, int height, 
                           const std::string& title) = 0;
    
    // 渲染一帧
    virtual ErrorCode RenderFrame(const AVFrame* frame) = 0;
    
    // 处理窗口事件（返回false表示用户请求退出）
    virtual bool PollEvents() = 0;
    
    // 等待垂直同步
    virtual void Present() = 0;
    
    // 关闭渲染器
    virtual void Close() = 0;
};

using IRendererPtr = std::unique_ptr<IRenderer>;

} // namespace live

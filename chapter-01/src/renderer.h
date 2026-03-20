#ifndef RENDERER_H
#define RENDERER_H

#include <string>

extern "C" {
#include <libavutil/frame.h>
#include <SDL2/SDL.h>
}

namespace live {

// 视频渲染器：YUV -> 屏幕
class Renderer {
public:
    Renderer();
    ~Renderer();

    // 禁止拷贝
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // 初始化窗口和渲染器
    // @param width: 视频宽度
    // @param height: 视频高度
    // @param title: 窗口标题
    // @return: 成功返回true
    bool Init(int width, int height, const char* title = "Live Player");

    // 关闭并释放资源
    void Close();

    // 渲染一帧
    // @param frame: YUV420P 格式的帧
    // @return: 成功返回true
    bool RenderFrame(const AVFrame* frame);

    // 处理窗口事件（需要定期调用，否则窗口无响应）
    // @return: 用户点击关闭按钮返回false，否则返回true
    bool PollEvents();

    // 获取错误信息
    const std::string& error() const { return error_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::string error_;
    bool initialized_ = false;
};

} // namespace live

#endif // RENDERER_H

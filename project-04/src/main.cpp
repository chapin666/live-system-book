#include "live/capturer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Capture Tool - 采集预览工具" << std::endl;
    
    auto capturer = live::CreateVideoCapturer();
    auto preview = live::CreatePreview();
    
    const int width = 1280;
    const int height = 720;
    
    if (!capturer->Init(0, width, height, 30)) {
        std::cerr << "初始化采集失败" << std::endl;
        return 1;
    }
    
    if (!preview->Init(width, height)) {
        std::cerr << "初始化预览失败" << std::endl;
        return 1;
    }
    
    std::cout << "开始采集，按窗口关闭按钮退出..." << std::endl;
    
    capturer->on_frame = [&](const uint8_t* data, int size, int w, int h) {
        // 渲染预览
        // preview->Render(data, w, h);
    };
    
    capturer->Start();
    
    while (preview->PollEvents()) {
        SDL_Delay(10);
    }
    
    capturer->Stop();
    
    std::cout << "采集结束" << std::endl;
    return 0;
}
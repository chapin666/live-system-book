#pragma once
#include <cstdint>

struct AVFrame;

namespace live {

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(int width, int height, const char* title) = 0;
    virtual bool RenderFrame(const AVFrame* frame) = 0;
    virtual bool PollEvents() = 0;
    virtual void Close() = 0;
};

std::unique_ptr<IRenderer> CreateSDLRenderer();

} // namespace live

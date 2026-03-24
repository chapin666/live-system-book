#include "live/mcu_server.h"
#include <iostream>

namespace live {

class McuServer::Impl {
public:
    McuConfig config;
    bool running = false;
    std::map<std::string, McuLayout> room_layouts;
};

McuServer::McuServer() : impl_(std::make_unique<Impl>()) {}
McuServer::~McuServer() = default;

bool McuServer::Initialize(const McuConfig& config) {
    impl_>-config = config;
    std::cout << "[MCU服务器] 初始化" << std::endl;
    std::cout << "  输出分辨率: " << config.output_width << "x" << config.output_height << std::endl;
    std::cout << "  输出码率: " << (config.output_bitrate / 1000) << " kbps" << std::endl;
    return true;
}

void McuServer::Shutdown() {
    Stop();
}

bool McuServer::Start() {
    impl_>-running = true;
    std::cout << "[MCU服务器] 启动成功" << std::endl;
    return true;
}

void McuServer::Stop() {
    impl_>-running = false;
    std::cout << "[MCU服务器] 已停止" << std::endl;
}

bool McuServer::CreateRoom(const std::string& room_id) {
    impl_>-room_layouts[room_id] = McuLayout::GRID_2X2;
    std::cout << "[MCU服务器] 创建房间: " << room_id << std::endl;
    return true;
}

bool McuServer::DeleteRoom(const std::string& room_id) {
    impl_>-room_layouts.erase(room_id);
    std::cout << "[MCU服务器] 删除房间: " << room_id << std::endl;
    return true;
}

bool McuServer::AddInputStream(const std::string& room_id,
                               const std::string& stream_id,
                               int width, int height) {
    std::cout << "[MCU服务器] 房间 " << room_id << " 添加流: " << stream_id << std::endl;
    return true;
}

void McuServer::RemoveInputStream(const std::string& room_id,
                                  const std::string& stream_id) {
    std::cout << "[MCU服务器] 房间 " << room_id << " 移除流: " << stream_id << std::endl;
}

void McuServer::SetLayout(const std::string& room_id, McuLayout layout) {
    impl_>-room_layouts[room_id] = layout;
    const char* layout_name = "Unknown";
    switch (layout) {
        case McuLayout::SINGLE: layout_name = "单画面"; break;
        case McuLayout::DUAL: layout_name = "双画面"; break;
        case McuLayout::GRID_2X2: layout_name = "2x2网格"; break;
        case McuLayout::GRID_3X3: layout_name = "3x3网格"; break;
        case McuLayout::SPEAKER: layout_name = "主讲人模式"; break;
    }
    std::cout << "[MCU服务器] 房间 " << room_id << " 设置布局: " << layout_name << std::endl;
}

bool McuServer::GetMixedFrame(const std::string& room_id,
                              uint8_t* buffer, size_t* len) {
    // 简化实现
    return true;
}

} // namespace live
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace live {

// MCU 服务器配置
struct McuConfig {
    int output_width = 1280;
    int output_height = 720;
    int output_fps = 30;
    int output_bitrate = 2000000;
    std::string output_format = "h264";
};

// MCU 混屏布局
enum class McuLayout {
    SINGLE,      // 单画面
    DUAL,        // 双画面（左右）
    GRID_2X2,    // 2x2 网格
    GRID_3X3,    // 3x3 网格
    SPEAKER      // 主讲人模式
};

// MCU 服务器
class McuServer {
public:
    McuServer();
    ~McuServer();
    
    // 初始化
    bool Initialize(const McuConfig& config);
    void Shutdown();
    
    // 启动/停止
    bool Start();
    void Stop();
    
    // 房间管理
    bool CreateRoom(const std::string& room_id);
    bool DeleteRoom(const std::string& room_id);
    
    // 输入流管理
    bool AddInputStream(const std::string& room_id,
                        const std::string& stream_id,
                        int width, int height);
    void RemoveInputStream(const std::string& room_id,
                           const std::string& stream_id);
    
    // 设置布局
    void SetLayout(const std::string& room_id, McuLayout layout);
    
    // 获取混流后的输出
    bool GetMixedFrame(const std::string& room_id,
                       uint8_t* buffer, size_t* len);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
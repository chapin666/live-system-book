#pragma once
#include <string>
#include <functional>
#include <memory>

namespace live {

// 回放服务器
class PlaybackServer {
public:
    PlaybackServer();
    ~PlaybackServer();
    
    // 初始化
    bool Initialize(int http_port, const std::string& hls_root);
    void Shutdown();
    
    // 启动/停止服务
    bool Start();
    void Stop();
    
    // 获取播放地址
    std::string GetLiveUrl(const std::string& stream_id) const;
    std::string GetPlaybackUrl(const std::string& stream_id, 
                               int64_t start_time_ms) const;
    
    // 回调
    using RequestCallback = std::function<void(const std::string& path)>;
    void SetRequestCallback(RequestCallback cb);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
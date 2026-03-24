#pragma once
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <stdint.h>

namespace live {

// 视频流信息
struct RemoteStream {
    std::string peer_id;
    uint32_t ssrc;
    int width;
    int height;
    bool has_audio;
    bool has_video;
};

// SFU 客户端
class SfuClient {
public:
    SfuClient();
    ~SfuClient();
    
    // 连接 SFU 服务器
    bool Connect(const std::string& sfu_url);
    void Disconnect();
    
    // 加入/离开房间
    bool JoinRoom(const std::string& room_id, const std::string& user_id);
    bool LeaveRoom();
    
    // 发布本地流
    bool Publish(bool audio, bool video);
    void Unpublish();
    
    // 订阅远端流
    bool Subscribe(const std::string& peer_id);
    void Unsubscribe(const std::string& peer_id);
    
    // 获取房间中的流列表
    std::vector<RemoteStream> GetRemoteStreams() const;
    
    // 接收远端数据
    bool ReceiveVideoFrame(const std::string& peer_id, 
                           uint8_t* buffer, size_t* len);
    bool ReceiveAudioFrame(const std::string& peer_id,
                           uint8_t* buffer, size_t* len);
    
    // 回调
    using StreamAddedCallback = std::function<void(const RemoteStream&)>;
    using StreamRemovedCallback = std::function<void(const std::string& peer_id)>;
    using DataCallback = std::function<void(const std::string& peer_id,
                                               const uint8_t* data, size_t len)>;
    
    void SetStreamAddedCallback(StreamAddedCallback cb);
    void SetStreamRemovedCallback(StreamRemovedCallback cb);
    void SetDataCallback(DataCallback cb);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace live
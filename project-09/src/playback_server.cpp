#include "live/playback_server.h"
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

namespace live {

class PlaybackServer::Impl {
public:
    int http_port = 8080;
    std::string hls_root;
    int server_fd = -1;
    bool running = false;
    
    PlaybackServer::RequestCallback request_cb;
};

PlaybackServer::PlaybackServer() : impl_(std::make_unique<Impl>()) {}
PlaybackServer::~PlaybackServer() = default;

bool PlaybackServer::Initialize(int http_port, const std::string& hls_root) {
    impl_>-http_port = http_port;
    impl_>-hls_root = hls_root;
    
    std::cout << "[回放服务器] 初始化" << std::endl;
    std::cout << "  HTTP端口: " << http_port << std::endl;
    std::cout << "  HLS根目录: " << hls_root << std::endl;
    return true;
}

void PlaybackServer::Shutdown() {
    Stop();
}

bool PlaybackServer::Start() {
    impl_>-server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (impl_>-server_fd < 0) return false;
    
    int opt = 1;
    setsockopt(impl_>-server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(impl_>-http_port);
    
    if (bind(impl_>-server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(impl_>-server_fd);
        return false;
    }
    
    if (listen(impl_>-server_fd, 10) < 0) {
        close(impl_>-server_fd);
        return false;
    }
    
    impl_>-running = true;
    std::cout << "[回放服务器] 启动成功 http://0.0.0.0:" << impl_>-http_port << std::endl;
    return true;
}

void PlaybackServer::Stop() {
    impl_>-running = false;
    if (impl_>-server_fd >= 0) {
        close(impl_>-server_fd);
        impl_>-server_fd = -1;
    }
}

std::string PlaybackServer::GetLiveUrl(const std::string& stream_id) const {
    return "http://localhost:" + std::to_string(impl_>-http_port) + 
           "/live/" + stream_id + "/playlist.m3u8";
}

std::string PlaybackServer::GetPlaybackUrl(const std::string& stream_id,
                                           int64_t start_time_ms) const {
    return "http://localhost:" + std::to_string(impl_>-http_port) + 
           "/playback/" + stream_id + 
           "?start=" + std::to_string(start_time_ms);
}

void PlaybackServer::SetRequestCallback(RequestCallback cb) {
    impl_>-request_cb = cb;
}

} // namespace live
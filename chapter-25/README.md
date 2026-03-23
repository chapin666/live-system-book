# 第二十五章：WebTransport 与 WHEP/WHIP

> **本章目标**：了解下一代Web实时通信技术，掌握WebTransport协议和WHEP/WHIP协议的基本原理与应用场景。

WebRTC虽然是当前实时通信的主流技术，但其复杂的信令流程和协议栈也带来了一些挑战。近年来，新的标准如WebTransport、WHEP和WHIP正在兴起，它们旨在简化实时通信的部署和使用。本章将介绍这些新兴技术，帮助你了解实时通信的未来发展方向。

**本章你将学习**：
- WebTransport协议及其与WebRTC的对比
- WHIP协议（WebRTC-HTTP Ingestion Protocol）
- WHEP协议（WebRTC-HTTP Egress Protocol）
- WebTransport在实时通信中的应用
- 下一代Web实时通信技术展望

**学习本章后，你将能够**：
- 理解WebTransport的核心优势
- 使用WHIP协议简化推流开发
- 使用WHEP协议简化拉流开发
- 评估新技术在项目中的适用性

---

## 目录

1. [WebTransport 简介](#1-webtransport-简介)
2. [WHIP 协议](#2-whip-协议)
3. [WHEP 协议](#3-whep-协议)
4. [WebTransport 实时通信](#4-webtransport-实时通信)
5. [未来展望](#5-未来展望)
6. [本章总结](#6-本章总结)

---

## 1. WebTransport 简介

### 1.1 什么是WebTransport？

**WebTransport**是一个新的Web API，基于HTTP/3和QUIC协议，为客户端和服务器之间提供双向、多路复用的传输能力。

**核心特点**：
| 特性 | 说明 |
|:---|:---|
| **基于QUIC** | 使用HTTP/3底层协议，内建多路复用 |
| **低延迟** | 减少握手时间，0-RTT或1-RTT建立连接 |
| **可靠/不可靠传输** | 同时支持可靠流和不可靠数据报 |
| **拥塞控制** | 现代化的拥塞控制算法（BBR） |
| **易于部署** | 基于HTTP，更容易穿透防火墙/NAT |

### 1.2 WebTransport 协议栈

![WebTransport 协议栈](./diagrams/webtransport-stack.svg)

```
┌─────────────────────────────────────────────────────────┐
│                    应用层                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ WebSocket│  │  HTTP/2  │  │    WebTransport      │  │
│  │   API    │  │  Server  │  │        API           │  │
│  └────┬─────┘  └────┬─────┘  └──────────┬───────────┘  │
├───────┼─────────────┼────────────────────┼──────────────┤
│       │             │                    │              │
│  ┌────┴─────┐  ┌────┴─────┐  ┌──────────┴──────────┐   │
│  │   TCP    │  │   TCP    │  │       QUIC          │   │
│  │          │  │          │  │  (基于UDP)          │   │
│  └────┬─────┘  └────┬─────┘  └──────────┬──────────┘   │
│       │             │                    │              │
│  ┌────┴─────────────┴────────────────────┴──────────┐   │
│  │                      IP                           │   │
│  └───────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 1.3 WebTransport vs WebRTC

| 维度 | WebRTC | WebTransport |
|:---|:---|:---|
| **传输协议** | SCTP/DTLS over UDP | QUIC over UDP |
| **连接建立** | ICE + DTLS + SCTP (2-3 RTT) | QUIC (0-1 RTT) |
| **NAT穿透** | 需要STUN/TURN | 类似HTTP/3，相对简单 |
| **可靠性** | 可配置 (SRTP/SCTP) | 可配置 (流/数据报) |
| **API复杂度** | 高 | 低 |
| **音视频** | 内置支持 | 需要配合WebCodecs |
| **浏览器支持** | 广泛 | 逐渐普及 |
| **服务端部署** | 复杂 | 相对简单 |

### 1.4 WebTransport 核心API

```javascript
// 客户端 JavaScript API

// 1. 创建 WebTransport 连接
const transport = new WebTransport('https://example.com:4433/endpoint');

// 2. 等待连接就绪
await transport.ready;
console.log('WebTransport connected');

// 3. 创建双向流（可靠传输）
const stream = await transport.createBidirectionalStream();
const reader = stream.readable.getReader();
const writer = stream.writable.getWriter();

// 发送数据
await writer.write(new Uint8Array([1, 2, 3, 4]));

// 接收数据
const { value, done } = await reader.read();
if (!done) {
    console.log('Received:', value);
}

// 4. 发送不可靠数据报
const datagramWriter = transport.datagrams.writable.getWriter();
await datagramWriter.write(new Uint8Array([5, 6, 7, 8]));

// 5. 接收数据报
const datagramReader = transport.datagrams.readable.getReader();
const { value: datagram } = await datagramReader.read();

// 6. 关闭连接
await transport.close();
```

### 1.5 C++ 服务端实现

```cpp
// webtransport_server.h
#pragma once

#include <string>
#include <memory>
#include <functional>

namespace live {

// WebTransport 会话
class WebTransportSession {
public:
    using StreamCallback = std::function<void(uint64_t stream_id)>;
    using DataCallback = std::function<void(uint64_t stream_id, 
                                               const uint8_t* data, 
                                               size_t len)>;
    
    WebTransportSession(uint64_t session_id);
    ~WebTransportSession();
    
    // 设置回调
    void SetStreamCallback(StreamCallback callback);
    void SetDatagramCallback(DataCallback callback);
    
    // 创建双向流
    uint64_t CreateBidirectionalStream();
    
    // 发送数据（流）
    bool SendStreamData(uint64_t stream_id, 
                        const uint8_t* data, 
                        size_t len);
    
    // 发送数据报
    bool SendDatagram(const uint8_t* data, size_t len);
    
    // 关闭会话
    void Close();
    
    uint64_t GetSessionId() const { return session_id_; }
    
private:
    uint64_t session_id_;
    StreamCallback stream_callback_;
    DataCallback datagram_callback_;
    
    uint64_t next_stream_id_ = 0;
    std::atomic<bool> closed_{false};
};

// WebTransport 服务器
class WebTransportServer {
public:
    struct Config {
        std::string bind_address = "0.0.0.0";
        int port = 4433;
        std::string certificate_path;
        std::string private_key_path;
        int max_sessions = 10000;
    };
    
    bool Initialize(const Config& config);
    void Shutdown();
    
    // 设置会话回调
    using SessionCallback = std::function<void(std::shared_ptr<WebTransportSession>)>;
    void SetNewSessionCallback(SessionCallback callback);
    void SetSessionClosedCallback(SessionCallback callback);
    
    // 启动/停止
    bool Start();
    void Stop();
    
    // 获取统计
    struct Stats {
        int active_sessions;
        int total_streams;
        int64_t bytes_sent;
        int64_t bytes_received;
    };
    Stats GetStats() const;
    
private:
    void AcceptLoop();
    void HandleQUICConnection(struct quic_connection_t* conn);
    
    Config config_;
    SessionCallback new_session_callback_;
    SessionCallback closed_session_callback_;
    
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
    
    // QUIC 上下文
    void* quic_context_ = nullptr;
    int listen_socket_ = -1;
    
    std::mutex sessions_mutex_;
    std::map<uint64_t, std::shared_ptr<WebTransportSession>> sessions_;
};

} // namespace live
```

---

## 2. WHIP 协议

### 2.1 什么是WHIP？

**WHIP（WebRTC-HTTP Ingestion Protocol）**是一个基于HTTP的协议，用于简化WebRTC推流（Ingestion）过程。

**传统WebRTC推流的痛点**：
```
传统流程 (复杂):
浏览器 ←──SDP offer/answer──→ 信令服务器 ←──复杂信令──→ SFU
     ←──ICE candidate交换──→
     ←──DTLS握手───────────→
     ←──媒体传输────────────→
```

**WHIP简化后的流程**：
```
WHIP流程 (简单):
浏览器 ──HTTP POST (SDP offer)──→ WHIP服务器
       ←──SDP answer────────────┘
     
直接开始媒体传输 (ICE/DTLS自动处理)
```

### 2.2 WHIP 协议流程

![WHIP 流程](./diagrams/whip-flow.svg)

**WHIP核心思想**：
1. 客户端发送HTTP POST请求，包含SDP offer
2. 服务器返回SDP answer
3. ICE和DTLS在后台自动完成
4. 开始媒体传输

### 2.3 WHIP API 设计

```
WHIP 端点: https://whip.example.com/session

1. 创建会话:
POST /session HTTP/1.1
Content-Type: application/sdp

v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
t=0 0
a=group:BUNDLE 0 1
m=audio 9 UDP/TLS/RTP/SAVPF 111
...

响应:
HTTP/1.1 201 Created
Location: https://whip.example.com/session/abc123
Content-Type: application/sdp

v=0
...

2. 更新/修改会话:
PATCH /session/abc123
Content-Type: application/trickle-ice-sdpfrag

3. 结束会话:
DELETE /session/abc123
```

### 2.4 WHIP 客户端实现

```cpp
// whip_client.h
#pragma once

#include <string>
#include <memory>

namespace live {

// WHIP 客户端
class WHIPClient {
public:
    struct Config {
        std::string whip_endpoint;  // WHIP服务器URL
        int timeout_ms = 30000;
    };
    
    WHIPClient(const Config& config);
    ~WHIPClient();
    
    // 初始化本地PeerConnection
    bool InitializePeerConnection();
    
    // 创建推流会话
    struct SessionInfo {
        std::string session_id;
        std::string resource_url;  // 用于后续PATCH/DELETE
        std::string remote_sdp;
    };
    
    // 开始推流
    // local_sdp: 本地SDP offer
    std::pair<bool, SessionInfo> StartPublishing(const std::string& local_sdp);
    
    // 更新ICE candidate (Trickle ICE)
    bool SendIceCandidate(const std::string& candidate,
                          const std::string& mid);
    
    // 停止推流
    bool StopPublishing();
    
    // 获取状态
    bool IsPublishing() const { return is_publishing_; }
    
private:
    bool SendHttpPost(const std::string& url,
                      const std::string& body,
                      const std::string& content_type,
                      std::string& response);
    
    bool SendHttpDelete(const std::string& url);
    
    bool SendHttpPatch(const std::string& url,
                       const std::string& body);
    
    Config config_;
    bool is_publishing_ = false;
    SessionInfo current_session_;
};

// 简化的WHIP客户端实现
bool WHIPClient::StartPublishing(const std::string& local_sdp) {
    // 发送POST请求到WHIP端点
    std::string response;
    bool success = SendHttpPost(
        config_.whip_endpoint,
        local_sdp,
        "application/sdp",
        response
    );
    
    if (!success) {
        return false;
    }
    
    // 解析响应
    // 1. 获取SDP answer
    current_session_.remote_sdp = response;
    
    // 2. 解析Location头获取资源URL
    // current_session_.resource_url = ...
    
    is_publishing_ = true;
    return true;
}

bool WHIPClient::SendIceCandidate(const std::string& candidate,
                                   const std::string& mid) {
    if (!is_publishing_) return false;
    
    // 构建trickle ICE SDP片段
    std::string body = "a=" + candidate + "\r\n";
    
    return SendHttpPatch(
        current_session_.resource_url,
        body
    );
}

bool WHIPClient::StopPublishing() {
    if (!is_publishing_) return true;
    
    bool success = SendHttpDelete(current_session_.resource_url);
    
    is_publishing_ = false;
    current_session_ = SessionInfo{};
    
    return success;
}

} // namespace live
```

### 2.5 WHIP 服务端实现

```cpp
// whip_server.h
#pragma once

#include <memory>
#include <map>

namespace live {

// WHIP 会话
class WHIPSession {
public:
    WHIPSession(const std::string& session_id,
                const std::string& local_sdp);
    
    // 处理远端SDP offer
    std::string ProcessOffer(const std::string& remote_sdp);
    
    // 添加ICE candidate
    void AddRemoteIceCandidate(const std::string& candidate);
    
    // 获取本地ICE candidates
    std::vector<std::string> GetLocalIceCandidates();
    
    // 关闭会话
    void Close();
    
    std::string GetSessionId() const { return session_id_; }
    
private:
    std::string session_id_;
    std::string local_sdp_;
    std::unique_ptr<PeerConnection> pc_;
};

// WHIP HTTP服务器
class WHIPServer {
public:
    struct Config {
        std::string bind_address = "0.0.0.0";
        int port = 8080;
        std::string whip_path = "/whip";
        
        // ICE服务器配置
        std::vector<std::string> ice_servers;
        
        // 证书配置
        std::string certificate_path;
        std::string private_key_path;
    };
    
    bool Initialize(const Config& config);
    void Shutdown();
    
    // HTTP请求处理
    void HandlePost(const std::string& path,
                    const std::map<std::string, std::string>& headers,
                    const std::string& body,
                    HttpResponse& response);
    
    void HandlePatch(const std::string& path,
                     const std::map<std::string, std::string>& headers,
                     const std::string& body,
                     HttpResponse& response);
    
    void HandleDelete(const std::string& path,
                      HttpResponse& response);
    
    bool Start();
    void Stop();
    
private:
    std::string GenerateSessionId();
    
    Config config_;
    std::mutex sessions_mutex_;
    std::map<std::string, std::shared_ptr<WHIPSession>> sessions_;
    
    std::unique_ptr<HttpServer> http_server_;
};

} // namespace live
```

---

## 3. WHEP 协议

### 3.1 什么是WHEP？

**WHEP（WebRTC-HTTP Egress Protocol）**是WHIP的"对称"协议，用于简化WebRTC拉流（Playback/Egress）过程。

**WHEP与WHIP的关系**：
| 协议 | 方向 | 用途 |
|:---|:---|:---|
| WHIP | 客户端 → 服务端 | 推流（Ingestion） |
| WHEP | 客户端 ← 服务端 | 拉流（Egress/Playback） |

### 3.2 WHEP 协议流程

![WHEP 流程](./diagrams/whep-flow.svg)

### 3.3 WHEP API 设计

```
WHEP 端点: https://whep.example.com/endpoint

1. 请求播放:
POST /endpoint HTTP/1.1
Content-Type: application/sdp

v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
t=0 0
... (SDP offer with receive-only)

响应:
HTTP/1.1 201 Created
Location: https://whep.example.com/endpoint/xyz789
Content-Type: application/sdp

v=0
... (SDP answer)

2. ICE candidate交换 (Trickle ICE):
PATCH /endpoint/xyz789
Content-Type: application/trickle-ice-sdpfrag

3. 停止播放:
DELETE /endpoint/xyz789
```

### 3.4 WHEP 客户端实现

```cpp
// whep_client.h
#pragma once

#include <string>

namespace live {

// WHEP 客户端 (拉流)
class WHEPClient {
public:
    struct Config {
        std::string whep_endpoint;
        int timeout_ms = 30000;
    };
    
    WHEPClient(const Config& config);
    ~WHEPClient();
    
    // 开始拉流
    struct PlaybackInfo {
        std::string session_id;
        std::string resource_url;
        std::string remote_sdp;
    };
    
    // local_sdp: receive-only SDP offer
    std::pair<bool, PlaybackInfo> StartPlayback(const std::string& local_sdp);
    
    // 发送ICE candidate
    bool SendIceCandidate(const std::string& candidate,
                          const std::string& mid);
    
    // 停止拉流
    bool StopPlayback();
    
    bool IsPlaying() const { return is_playing_; }
    
private:
    bool SendHttpPost(const std::string& url,
                      const std::string& body,
                      std::string& response);
    
    bool SendHttpDelete(const std::string& url);
    bool SendHttpPatch(const std::string& url,
                       const std::string& body);
    
    Config config_;
    bool is_playing_ = false;
    PlaybackInfo current_session_;
};

} // namespace live
```

---

## 4. WebTransport 实时通信

### 4.1 为什么选择 WebTransport？

**WebTransport + WebCodecs 组合**：

```
┌─────────────────────────────────────────┐
│           应用层 (Application)           │
│  ┌──────────────┐  ┌────────────────┐  │
│  │  信令/控制    │  │  音视频处理     │  │
│  └──────────────┘  └────────────────┘  │
├─────────────────────────────────────────┤
│         WebTransport API                │
│  ┌──────────┐  ┌──────────┐            │
│  │  可靠流   │  │ 不可靠数据报│            │
│  └──────────┘  └──────────┘            │
├─────────────────────────────────────────┤
│              QUIC 协议                  │
└─────────────────────────────────────────┘
```

**WebCodecs API**（用于音视频编解码）：
```javascript
// 视频编码
const encoder = new VideoEncoder({
    output: (chunk, metadata) => {
        // 发送编码后的数据
        sendVideoChunk(chunk);
    },
    error: (e) => console.error(e)
});

encoder.configure({
    codec: 'vp09.00.10.08',
    width: 1920,
    height: 1080,
    bitrate: 2_000_000,
    framerate: 30
});

// 编码视频帧
const frame = new VideoFrame(videoElement);
encoder.encode(frame);

// 视频解码
const decoder = new VideoDecoder({
    output: (frame) => {
        // 渲染解码后的帧
        canvasContext.drawImage(frame, 0, 0);
    },
    error: (e) => console.error(e)
});

decoder.configure({
    codec: 'vp09.00.10.08',
    codedWidth: 1920,
    codedHeight: 1080
});
```

### 4.2 基于 WebTransport 的实时通信系统

```cpp
// wt_media_server.h
#pragma once

#include <memory>
#include <map>

namespace live {

// WebTransport 媒体流
class WTMediaStream {
public:
    enum class Type {
        PUBLISH,    // 推流
        PLAYBACK    // 拉流
    };
    
    WTMediaStream(uint64_t stream_id, Type type);
    
    // 处理视频帧
    void OnVideoFrame(const EncodedVideoFrame& frame);
    
    // 处理音频帧
    void OnAudioFrame(const EncodedAudioFrame& frame);
    
    // 转发到订阅者
    void ForwardTo(WTMediaStream* subscriber);
    
    // 设置编解码参数
    void SetVideoCodec(const std::string& codec,
                       int width, int height,
                       int bitrate);
    void SetAudioCodec(const std::string& codec,
                       int sample_rate,
                       int channels);
    
private:
    uint64_t stream_id_;
    Type type_;
    
    std::string video_codec_;
    std::string audio_codec_;
    
    std::vector<WTMediaStream*> subscribers_;
};

// WebTransport 媒体服务器
class WTMediaServer {
public:
    struct Config {
        std::string bind_address = "0.0.0.0";
        int port = 4433;
        std::string cert_path;
        std::string key_path;
    };
    
    bool Initialize(const Config& config);
    void Shutdown();
    
    // 启动服务器
    bool Start();
    void Stop();
    
    // 处理新的WebTransport会话
    void OnNewSession(std::shared_ptr<WebTransportSession> session);
    
    // 创建发布流
    std::shared_ptr<WTMediaStream> CreatePublishStream(
        const std::string& stream_id,
        std::shared_ptr<WebTransportSession> session);
    
    // 创建播放流
    std::shared_ptr<WTMediaStream> CreatePlaybackStream(
        const std::string& stream_id,
        std::shared_ptr<WebTransportSession> session);
    
    // 获取统计
    struct Stats {
        int active_sessions;
        int publish_streams;
        int playback_streams;
        int64_t bytes_sent;
        int64_t bytes_received;
    };
    Stats GetStats() const;
    
private:
    void HandleControlMessage(std::shared_ptr<WebTransportSession> session,
                               const uint8_t* data, size_t len);
    
    void HandleMediaData(std::shared_ptr<WebTransportSession> session,
                         const uint8_t* data, size_t len);
    
    Config config_;
    std::unique_ptr<WebTransportServer> wt_server_;
    
    std::mutex streams_mutex_;
    std::map<std::string, std::shared_ptr<WTMediaStream>> publish_streams_;
    std::map<std::string, std::vector<std::shared_ptr<WTMediaStream>>> stream_subscribers_;
};

} // namespace live
```

---

## 5. 未来展望

### 5.1 技术演进路线图

![未来技术路线图](./diagrams/future-roadmap.svg)

**2020-2023**: WebRTC成熟，SFU/MCU广泛应用
**2023-2025**: WHIP/WHEP标准化，简化信令
**2025+**: WebTransport + WebCodecs成为新选择

### 5.2 WebTransport vs WebRTC 选择指南

| 场景 | 推荐技术 | 理由 |
|:---|:---|:---|
| 视频会议 | WebRTC | 成熟、浏览器原生支持 |
| 低延迟直播 | WHIP/WHEP + WebRTC | 简化部署、兼容性好 |
| 游戏/控制 | WebTransport | 超低延迟、可靠/不可靠灵活选择 |
| 自定义编解码 | WebTransport + WebCodecs | 完全控制媒体处理 |
| IoT数据传输 | WebTransport | 轻量、易部署 |
| 大规模广播 | WebTransport | 更好的拥塞控制 |

### 5.3 迁移策略

```
现有系统演进路线:

阶段1: 保留WebRTC核心
┌─────────────────┐
│   WebRTC SFU    │
│  (保持稳定)     │
└────────┬────────┘
         │
阶段2: 添加WHIP/WHEP支持
┌─────────────────┐
│   WHIP/WHEP     │
│   接口层        │
├─────────────────┤
│   WebRTC SFU    │
└─────────────────┘

阶段3: 实验性WebTransport
┌─────────────────┐
│  WebTransport   │
│   (部分场景)    │
├─────────────────┤
│  WHIP/WHEP      │
├─────────────────┤
│   WebRTC SFU    │
└─────────────────┘

阶段4: 全面迁移(视情况)
┌─────────────────┐
│  WebTransport   │
│  + WebCodecs    │
└─────────────────┘
```

---

## 6. 本章总结

### 6.1 核心知识点

**WebTransport**：
- 基于QUIC的新一代Web传输API
- 支持可靠流和不可靠数据报
- 0-RTT快速连接建立
- 更简单的部署（类似HTTP）

**WHIP协议**：
- 简化WebRTC推流
- 基于HTTP POST/DELETE
- 自动处理ICE/DTLS

**WHEP协议**：
- 简化WebRTC拉流
- WHIP的对称协议
- 统一HTTP接口

### 6.2 技术对比总结

| 技术 | 协议层 | 复杂度 | 延迟 | 适用场景 |
|:---|:---|:---|:---|:---|
| WebRTC | 应用层 | 高 | 低 | 视频会议、P2P |
| WHIP/WHEP | 信令层 | 中 | 低 | 直播推/拉流 |
| WebTransport | 传输层 | 低 | 极低 | 自定义应用 |

### 6.3 课后思考

1. **协议选择**：设计一个直播系统，同时支持WebRTC和WebTransport推流，分析各自的优缺点。

2. **WHIP扩展**：如何扩展WHIP协议支持Simulcast（多码率推流）？设计API接口。

3. **性能评估**：估算WebTransport相比WebRTC在连接建立时间上能节省多少？

4. **迁移路径**：假设你有一个基于传统WebSocket信令的WebRTC系统，如何逐步迁移到WHIP/WHEP？

5. **未来趋势**：WebCodecs + WebTransport组合会取代WebRTC吗？分析各自的生存空间。

### 6.4 扩展阅读

- WebTransport W3C草案: https://w3c.github.io/webtransport/
- WHIP IETF草案: https://datatracker.ietf.org/doc/draft-ietf-wish-whip/
- WHEP IETF草案: https://datatracker.ietf.org/doc/draft-murillo-whep/
- WebCodecs API: https://w3c.github.io/webcodecs/

---

**本章结束。下一部分将进入生产部署相关内容，学习如何构建可靠的监控系统。**

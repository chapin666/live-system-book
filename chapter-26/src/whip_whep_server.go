/**
 * Chapter 25: WHIP/WHEP 服务器端示例 (Go)
 * 
 * 简化版 WHIP/WHEP 信令服务器
 */

package main

import (
	"fmt"
	"io"
	"net/http"
	"strings"
	"sync"

	"github.com/pion/webrtc/v3"
)

// WhipHandler 处理 WHIP 推流请求
type WhipHandler struct {
	mu       sync.RWMutex
	sessions map[string]*webrtc.PeerConnection
}

func NewWhipHandler() *WhipHandler {
	return &WhipHandler{
		sessions: make(map[string]*webrtc.PeerConnection),
	}
}

func (h *WhipHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodPost:
		h.handlePublish(w, r)
	case http.MethodDelete:
		h.handleStop(w, r)
	default:
		w.WriteHeader(http.StatusMethodNotAllowed)
	}
}

func (h *WhipHandler) handlePublish(w http.ResponseWriter, r *http.Request) {
	// 读取 Offer SDP
	offerSDP, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}

	// 创建 PeerConnection
	config := webrtc.Configuration{
		ICEServers: []webrtc.ICEServer{
			{URLs: []string{"stun:stun.l.google.com:19302"}},
		},
	}

	pc, err := webrtc.NewPeerConnection(config)
	if err != nil {
		http.Error(w, "Internal error", http.StatusInternalServerError)
		return
	}

	// 设置远端描述
	offer := webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  string(offerSDP),
	}

	if err := pc.SetRemoteDescription(offer); err != nil {
		pc.Close()
		http.Error(w, "Invalid SDP", http.StatusBadRequest)
		return
	}

	// 创建 Answer
	answer, err := pc.CreateAnswer(nil)
	if err != nil {
		pc.Close()
		http.Error(w, "Create answer failed", http.StatusInternalServerError)
		return
	}

	if err := pc.SetLocalDescription(answer); err != nil {
		pc.Close()
		http.Error(w, "Set local description failed", http.StatusInternalServerError)
		return
	}

	// 存储会话
	sessionID := generateSessionID()
	h.mu.Lock()
	h.sessions[sessionID] = pc
	h.mu.Unlock()

	// 处理远端轨道
	pc.OnTrack(func(track *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {
		fmt.Printf("Received %s track\n", track.Kind().String())
		// 这里可以将媒体流转发给其他订阅者
	})

	// 返回响应
	w.Header().Set("Content-Type", "application/sdp")
	w.Header().Set("Location", fmt.Sprintf("/whip/%s", sessionID))
	w.WriteHeader(http.StatusCreated)
	w.Write([]byte(answer.SDP))
}

func (h *WhipHandler) handleStop(w http.ResponseWriter, r *http.Request) {
	// 从 URL 提取 session ID
	parts := strings.Split(r.URL.Path, "/")
	if len(parts) < 3 {
		http.Error(w, "Invalid session", http.StatusBadRequest)
		return
	}

	sessionID := parts[2]

	h.mu.Lock()
	pc, exists := h.sessions[sessionID]
	if exists {
		delete(h.sessions, sessionID)
	}
	h.mu.Unlock()

	if exists {
		pc.Close()
	}

	w.WriteHeader(http.StatusOK)
}

// WhepHandler 处理 WHEP 播放请求
type WhepHandler struct {
	mu       sync.RWMutex
	sessions map[string]*webrtc.PeerConnection
}

func NewWhepHandler() *WhepHandler {
	return &WhepHandler{
		sessions: make(map[string]*webrtc.PeerConnection),
	}
}

func (h *WhepHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodPost:
		h.handleSubscribe(w, r)
	case http.MethodDelete:
		h.handleUnsubscribe(w, r)
	default:
		w.WriteHeader(http.StatusMethodNotAllowed)
	}
}

func (h *WhepHandler) handleSubscribe(w http.ResponseWriter, r *http.Request) {
	// 类似 WHIP，但添加本地轨道
	offerSDP, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "Bad request", http.StatusBadRequest)
		return
	}

	config := webrtc.Configuration{
		ICEServers: []webrtc.ICEServer{
			{URLs: []string{"stun:stun.l.google.com:19302"}},
		},
	}

	pc, err := webrtc.NewPeerConnection(config)
	if err != nil {
		http.Error(w, "Internal error", http.StatusInternalServerError)
		return
	}

	offer := webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  string(offerSDP),
	}

	if err := pc.SetRemoteDescription(offer); err != nil {
		pc.Close()
		http.Error(w, "Invalid SDP", http.StatusBadRequest)
		return
	}

	// 这里应该添加本地媒体轨道
	// 简化示例，省略添加 track 的代码

	answer, err := pc.CreateAnswer(nil)
	if err != nil {
		pc.Close()
		http.Error(w, "Create answer failed", http.StatusInternalServerError)
		return
	}

	if err := pc.SetLocalDescription(answer); err != nil {
		pc.Close()
		http.Error(w, "Set local description failed", http.StatusInternalServerError)
		return
	}

	sessionID := generateSessionID()
	h.mu.Lock()
	h.sessions[sessionID] = pc
	h.mu.Unlock()

	w.Header().Set("Content-Type", "application/sdp")
	w.Header().Set("Location", fmt.Sprintf("/whep/%s", sessionID))
	w.WriteHeader(http.StatusCreated)
	w.Write([]byte(answer.SDP))
}

func (h *WhepHandler) handleUnsubscribe(w http.ResponseWriter, r *http.Request) {
	parts := strings.Split(r.URL.Path, "/")
	if len(parts) < 3 {
		http.Error(w, "Invalid session", http.StatusBadRequest)
		return
	}

	sessionID := parts[2]

	h.mu.Lock()
	pc, exists := h.sessions[sessionID]
	if exists {
		delete(h.sessions, sessionID)
	}
	h.mu.Unlock()

	if exists {
		pc.Close()
	}

	w.WriteHeader(http.StatusOK)
}

func generateSessionID() string {
	// 简化实现，实际应使用 UUID
	return fmt.Sprintf("session-%d", len(sessionIDCounter))
}

var sessionIDCounter int

func main() {
	whip := NewWhipHandler()
	whep := NewWhepHandler()

	http.Handle("/whip/", whip)
	http.Handle("/whep/", whep)

	fmt.Println("WHIP/WHEP server starting on :8080")
	http.ListenAndServe(":8080", nil)
}

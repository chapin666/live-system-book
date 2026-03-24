/**
 * Chapter 25: WebTransport 客户端示例 (JavaScript)
 * 
 * 使用方式:
 * 1. 需要支持 WebTransport 的浏览器 (Chrome 97+)
 * 2. 需要 HTTPS 或 localhost
 */

// WebTransport 客户端类
class WebTransportClient {
    constructor(url) {
        this.url = url;
        this.transport = null;
        this.stream = null;
    }

    async connect() {
        try {
            this.transport = new WebTransport(this.url);
            await this.transport.ready;
            console.log('WebTransport connected');
            return true;
        } catch (e) {
            console.error('Connection failed:', e);
            return false;
        }
    }

    async createBidirectionalStream() {
        if (!this.transport) return null;
        
        const stream = await this.transport.createBidirectionalStream();
        this.stream = stream;
        
        // 读取数据
        this.readLoop(stream.readable);
        
        return stream.writable;
    }

    async readLoop(readable) {
        const reader = readable.getReader();
        try {
            while (true) {
                const { value, done } = await reader.read();
                if (done) break;
                console.log('Received:', new TextDecoder().decode(value));
            }
        } finally {
            reader.releaseLock();
        }
    }

    async sendData(data) {
        if (!this.stream) return;
        const writer = this.stream.writable.getWriter();
        await writer.write(new TextEncoder().encode(data));
        writer.releaseLock();
    }

    close() {
        if (this.transport) {
            this.transport.close();
        }
    }
}

// WHIP 客户端 (简化版)
class WhipClient {
    constructor(whipEndpoint) {
        this.endpoint = whipEndpoint;
        this.pc = new RTCPeerConnection({
            iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
        });
    }

    async publish(stream) {
        // 添加轨道
        stream.getTracks().forEach(track => {
            this.pc.addTrack(track, stream);
        });

        // 创建 Offer
        const offer = await this.pc.createOffer();
        await this.pc.setLocalDescription(offer);

        // 等待 ICE 收集
        await this.waitForIceGathering();

        // 发送 WHIP 请求
        const response = await fetch(this.endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/sdp',
            },
            body: this.pc.localDescription.sdp
        });

        if (!response.ok) {
            throw new Error('WHIP request failed');
        }

        // 设置 Answer
        const answer = await response.text();
        await this.pc.setRemoteDescription({
            type: 'answer',
            sdp: answer
        });

        // 获取 Location 头
        this.resourceUrl = response.headers.get('Location');
        console.log('Publishing to:', this.resourceUrl);

        return true;
    }

    async stop() {
        if (this.resourceUrl) {
            await fetch(this.resourceUrl, { method: 'DELETE' });
        }
        this.pc.close();
    }

    waitForIceGathering() {
        return new Promise((resolve) => {
            if (this.pc.iceGatheringState === 'complete') {
                resolve();
                return;
            }
            this.pc.onicegatheringstatechange = () => {
                if (this.pc.iceGatheringState === 'complete') {
                    resolve();
                }
            };
        });
    }
}

// WHEP 客户端 (简化版)
class WhepClient {
    constructor(whepEndpoint) {
        this.endpoint = whepEndpoint;
        this.pc = new RTCPeerConnection({
            iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
        });
    }

    async play(videoElement) {
        // 接收轨道
        this.pc.ontrack = (event) => {
            videoElement.srcObject = event.streams[0];
        };

        // 创建 Offer (仅接收)
        const offer = await this.pc.createOffer({
            offerToReceiveAudio: true,
            offerToReceiveVideo: true
        });
        await this.pc.setLocalDescription(offer);

        // 等待 ICE 收集
        await this.waitForIceGathering();

        // 发送 WHEP 请求
        const response = await fetch(this.endpoint, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/sdp',
            },
            body: this.pc.localDescription.sdp
        });

        if (!response.ok) {
            throw new Error('WHEP request failed');
        }

        // 设置 Answer
        const answer = await response.text();
        await this.pc.setRemoteDescription({
            type: 'answer',
            sdp: answer
        });

        this.resourceUrl = response.headers.get('Location');
        return true;
    }

    async stop() {
        if (this.resourceUrl) {
            await fetch(this.resourceUrl, { method: 'DELETE' });
        }
        this.pc.close();
    }

    waitForIceGathering() {
        return new Promise((resolve) => {
            if (this.pc.iceGatheringState === 'complete') {
                resolve();
                return;
            }
            this.pc.onicegatheringstatechange = () => {
                if (this.pc.iceGatheringState === 'complete') {
                    resolve();
                }
            };
        });
    }
}

// 使用示例
async function demo() {
    // WebTransport 示例
    const wt = new WebTransportClient('https://localhost:4433/echo');
    await wt.connect();
    
    const writable = await wt.createBidirectionalStream();
    const writer = writable.getWriter();
    await writer.write(new TextEncoder().encode('Hello WebTransport!'));
    writer.releaseLock();

    // WHIP 推流示例
    // const whip = new WhipClient('https://sfu.example.com/whip/endpoint');
    // const stream = await navigator.mediaDevices.getUserMedia({ video: true, audio: true });
    // await whip.publish(stream);

    // WHEP 播放示例
    // const whep = new WhepClient('https://sfu.example.com/whep/endpoint');
    // await whep.play(document.getElementById('video'));
}

// export { WebTransportClient, WhipClient, WhepClient };

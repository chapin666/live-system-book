#include "live/idemuxer.h"
#include "live/idecoder.h"
#include "live/irenderer.h"
#include "live/raii_utils.h"
#include <iostream>

using namespace live;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <视频文件>" << std::endl;
        return 1;
    }
    
    auto demuxer = CreateFFmpegDemuxer();
    auto decoder = CreateFFmpegDecoder();
    auto renderer = CreateSDLRenderer();
    
    if (!demuxer->Open(argv[1])) {
        return 1;
    }
    
    StreamInfo info;
    demuxer->GetVideoStreamInfo(info);
    std::cout << "视频: " << info.width << "x" << info.height << std::endl;
    
    decoder->Init(info.codec_id, info.width, info.height);
    renderer->Init(info.width, info.height, "Pipeline Player");
    
    FramePtr frame(av_frame_alloc());
    PacketPtr packet(av_packet_alloc());
    
    bool running = true;
    while (running) {
        if (!demuxer->ReadPacket(packet.get())) {
            break;
        }
        
        decoder->SendPacket(packet.get());
        av_packet_unref(packet.get());
        
        while (decoder->ReceiveFrame(frame.get())) {
            renderer->RenderFrame(frame.get());
            
            if (!renderer->PollEvents()) {
                running = false;
                break;
            }
        }
    }
    
    decoder->Flush();
    while (decoder->ReceiveFrame(frame.get())) {
        renderer->RenderFrame(frame.get());
    }
    
    return 0;
}

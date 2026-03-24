#include "live/mcu_recorder.h"
#include "live/hls_generator.h"
#include "live/playback_server.h"
#include <iostream>
#include <string>
#include <signal.h>

static volatile bool g_running = true;

void SignalHandler(int sig) {
    g_running = false;
}

void PrintUsage(const char* program) {
    std::cout << "用法: " << program << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  --output <dir>     输出目录 (默认: ./recordings)" << std::endl;
    std::cout << "  --duration <sec>   切片时长 (默认: 10秒)" << std::endl;
    std::cout << "  --port <port>      HTTP服务端口 (默认: 8080)" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << program << " --output /data/recordings --duration 6" << std::endl;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, SignalHandler);
    
    std::string output_dir = "./recordings";
    int segment_duration = 10;
    int http_port = 8080;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            segment_duration = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            http_port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  录制回放系统" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "输出目录: " << output_dir << std::endl;
    std::cout << "切片时长: " << segment_duration << "秒" << std::endl;
    std::cout << "HTTP端口: " << http_port << std::endl;
    std::cout << std::endl;
    
    // 初始化录制器
    live::RecordConfig record_config;
    record_config.output_dir = output_dir;
    record_config.segment_duration_sec = segment_duration;
    
    live::McuRecorder recorder;
    if (!recorder.Initialize(record_config)) {
        std::cerr << "录制器初始化失败" << std::endl;
        return 1;
    }
    
    // 设置片段回调
    live::HlsGenerator hls_generator;
    if (!hls_generator.Initialize(output_dir, segment_duration, 6)) {
        std::cerr << "HLS生成器初始化失败" << std::endl;
        return 1;
    }
    
    recorder.SetSegmentCallback([&hls_generator](const std::string& segment_path) {
        std::cout << "[录制] 新片段: " << segment_path << std::endl;
        hls_generator.AddSegment(segment_path, 10.0);
    });
    
    // 启动录制
    if (!recorder.StartRecording()) {
        std::cerr << "开始录制失败" << std::endl;
        return 1;
    }
    
    // 添加模拟输入流
    recorder.AddInputStream("stream_001", 1280, 720, true);
    
    // 启动回放服务
    live::PlaybackServer playback_server;
    if (!playback_server.Initialize(http_port, output_dir)) {
        std::cerr << "回放服务器初始化失败" << std::endl;
        return 1;
    }
    
    if (!playback_server.Start()) {
        std::cerr << "回放服务器启动失败" << std::endl;
        return 1;
    }
    
    std::cout << "\n服务已启动:" << std::endl;
    std::cout << "  直播地址: " << playback_server.GetLiveUrl("stream_001") << std::endl;
    std::cout << "  回放地址: " << playback_server.GetPlaybackUrl("stream_001", 0) << std::endl;
    std::cout << "\n按 Ctrl+C 停止服务" << std::endl;
    
    // 模拟录制循环
    int frame_count = 0;
    while (g_running) {
        // 模拟写入数据
        uint8_t dummy_data[1024] = {0};
        recorder.WriteVideoFrame("stream_001", dummy_data, sizeof(dummy_data), frame_count * 33);
        
        if (frame_count % 30 == 0) {
            std::cout << "\r[录制中] 帧数: " << frame_count 
                      << " 大小: " << (recorder.GetBytesWritten() / 1024) << " KB"
                      << " 片段: " << recorder.GetSegmentCount()
                      << std::flush;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(33));  // 30fps
        frame_count++;
    }
    
    std::cout << std::endl << "\n停止服务..." << std::endl;
    
    recorder.StopRecording();
    playback_server.Stop();
    hls_generator.Shutdown();
    
    std::cout << "服务已停止" << std::endl;
    return 0;
}
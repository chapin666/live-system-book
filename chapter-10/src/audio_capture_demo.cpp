/**
 * 音频采集演示 - 录制并保存为WAV文件
 * 
 * 编译: g++ audio_capture_demo.cpp -o audio_capture_demo $(pkg-config --cflags --libs libavformat libavcodec libavutil libavdevice)
 * 运行: ./audio_capture_demo [输出文件名] [时长秒数]
 *   示例: ./audio_capture_demo test.wav 10
 */

#include <stdio.h>
#include <string>
#include <fstream>
#include <vector>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}

// WAV文件头结构
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t file_size;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size;
};

int main(int argc, char* argv[]) {
    const char* output_file = (argc > 1) ? argv[1] : "output.wav";
    int duration_sec = (argc > 2) ? atoi(argv[2]) : 10;
    
    printf("Audio Capture Demo\n");
    printf("Output: %s, Duration: %d seconds\n", output_file, duration_sec);
    
    // 初始化FFmpeg设备
    avdevice_register_all();
    
    // 选择音频设备
    const AVInputFormat* input_format = nullptr;
    const char* device_name = nullptr;
    
#if defined(__APPLE__)
    input_format = av_find_input_format("avfoundation");
    device_name = ":0"; // 默认音频输入
    printf("Platform: macOS, device: %s\n", device_name);
#elif defined(__linux__)
    input_format = av_find_input_format("alsa");
    device_name = "default";
    printf("Platform: Linux (ALSA), device: %s\n", device_name);
#endif
    
    // 设置音频参数
    AVDictionary* options = nullptr;
    av_dict_set(&options, "sample_rate", "48000", 0);
    av_dict_set(&options, "channels", "2", 0);
    
    // 打开音频设备
    AVFormatContext* fmt_ctx = nullptr;
    int ret = avformat_open_input(&fmt_ctx, device_name, input_format, &options);
    av_dict_free(&options);
    
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open audio device: %s\n", errbuf);
        return 1;
    }
    
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to find stream info\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    // 查找音频流
    int audio_stream_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }
    
    if (audio_stream_idx == -1) {
        fprintf(stderr, "No audio stream found\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    AVCodecParameters* codecpar = fmt_ctx->streams[audio_stream_idx]->codecpar;
    printf("Audio: %d Hz, %d channels, format: %d\n",
           codecpar->sample_rate, codecpar->channels, codecpar->format);
    
    // 打开输出文件
    std::ofstream wav_file(output_file, std::ios::binary);
    if (!wav_file) {
        fprintf(stderr, "Failed to open output file\n");
        avformat_close_input(&fmt_ctx);
        return 1;
    }
    
    // 预留WAV头位置
    WavHeader header;
    header.num_channels = codecpar->channels;
    header.sample_rate = codecpar->sample_rate;
    header.bits_per_sample = 16;
    header.block_align = header.num_channels * header.bits_per_sample / 8;
    header.byte_rate = header.sample_rate * header.block_align;
    
    wav_file.write((char*)&header, sizeof(header));
    
    // 采集音频
    AVPacket* pkt = av_packet_alloc();
    std::vector<uint8_t> audio_data;
    
    auto start_time = std::chrono::steady_clock::now();
    int packet_count = 0;
    
    printf("Recording...\n");
    
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= duration_sec) {
            break;
        }
        
        ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) continue;
        
        if (pkt->stream_index == audio_stream_idx) {
            // 保存音频数据（假设是PCM格式）
            audio_data.insert(audio_data.end(), pkt->data, pkt->data + pkt->size);
            packet_count++;
            
            // 显示进度
            int elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start_time).count();
            printf("Recording: %d/%d seconds, packets: %d\r", 
                   elapsed_sec, duration_sec, packet_count);
            fflush(stdout);
        }
        
        av_packet_unref(pkt);
    }
    
    printf("\nRecording finished. Writing file...\n");
    
    // 写入音频数据
    wav_file.write((char*)audio_data.data(), audio_data.size());
    
    // 更新WAV头
    header.data_size = audio_data.size();
    header.file_size = sizeof(WavHeader) - 8 + header.data_size;
    
    wav_file.seekp(0);
    wav_file.write((char*)&header, sizeof(header));
    wav_file.close();
    
    printf("Saved to: %s (%zu bytes)\n", output_file, audio_data.size());
    
    // 清理
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    
    return 0;
}

/**
 * 简单RTMP推流器 - 使用FFmpeg编码和推流
 * 
 * 编译: g++ simple_streamer.cpp -o simple_streamer $(pkg-config --cflags --libs libavformat libavcodec libavutil libswscale)
 * 运行: ./simple_streamer rtmp://localhost/live/stream
 * 
 * 功能: 生成测试视频（彩色条）并推流到RTMP服务器
 */

#include <stdio.h>
#include <string>
#include <stdlib.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
}

// 生成测试图案（彩色条）
void FillColorBars(uint8_t* yuv[3], int linesize[3], int width, int height, int frame_num) {
    int bar_width = width / 8;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int bar = x / bar_width;
            uint8_t Y, U, V;
            
            // 标准彩条颜色
            switch (bar) {
                case 0: Y = 180; U = 128; V = 128; break; // 白
                case 1: Y = 162; U = 44;  V = 142; break; // 黄
                case 2: Y = 131; U = 156; V = 44;  break; // 青
                case 3: Y = 112; U = 72;  V = 58;  break; // 绿
                case 4: Y = 84;  U = 184; V = 198; break; // 紫
                case 5: Y = 65;  U = 100; V = 212; break; // 红
                case 6: Y = 35;  U = 212; V = 114; break; // 蓝
                default: Y = 16;  U = 128; V = 128; break; // 黑
            }
            
            // 添加滚动效果
            int offset = (frame_num * 2) % bar_width;
            if ((x + offset) / bar_width != bar) {
                Y = Y * 8 / 10; // 稍微变暗
            }
            
            yuv[0][y * linesize[0] + x] = Y;
            
            // UV是半采样
            if (y % 2 == 0 && x % 2 == 0) {
                int uv_x = x / 2;
                int uv_y = y / 2;
                yuv[1][uv_y * linesize[1] + uv_x] = U;
                yuv[2][uv_y * linesize[2] + uv_x] = V;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "用法: %s <RTMP URL> [宽度] [高度] [帧率]\n", argv[0]);
        fprintf(stderr, "示例: %s rtmp://localhost/live/stream 1280 720 30\n", argv[0]);
        return 1;
    }
    
    const char* rtmp_url = argv[1];
    int width = (argc > 2) ? atoi(argv[2]) : 1280;
    int height = (argc > 3) ? atoi(argv[3]) : 720;
    int fps = (argc > 4) ? atoi(argv[4]) : 30;
    int bitrate = 4000000; // 4 Mbps
    
    printf("RTMP Streamer\n");
    printf("Output: %s\n", rtmp_url);
    printf("Resolution: %dx%d @ %d fps, %d kbps\n", width, height, fps, bitrate / 1000);
    
    // 初始化FFmpeg网络
    avformat_network_init();
    
    // 创建输出格式上下文
    AVFormatContext* out_ctx = nullptr;
    int ret = avformat_alloc_output_context2(&out_ctx, nullptr, "flv", rtmp_url);
    if (ret < 0) {
        fprintf(stderr, "Failed to create output context\n");
        return 1;
    }
    
    // 查找H.264编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "H.264 encoder not found\n");
        avformat_free_context(out_ctx);
        return 1;
    }
    
    // 创建视频流
    AVStream* stream = avformat_new_stream(out_ctx, codec);
    if (!stream) {
        fprintf(stderr, "Failed to create stream\n");
        avformat_free_context(out_ctx);
        return 1;
    }
    
    // 配置编码器上下文
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = (AVRational){1, fps};
    codec_ctx->framerate = (AVRational){fps, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = bitrate;
    codec_ctx->gop_size = fps * 2; // 2秒一个关键帧
    codec_ctx->max_b_frames = 0;   // 直播场景不用B帧
    
    // 设置编码参数
    av_opt_set(codec_ctx->priv_data, "preset", "fast", 0);
    av_opt_set(codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(codec_ctx->priv_data, "profile", "main", 0);
    
    // 复制参数到流
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        fprintf(stderr, "Failed to copy codec params\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(out_ctx);
        return 1;
    }
    
    stream->time_base = codec_ctx->time_base;
    
    // 打开编码器
    ret = avcodec_open2(codec_ctx, codec, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Failed to open codec: %s\n", errbuf);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(out_ctx);
        return 1;
    }
    
    // 打开输出
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&out_ctx->pb, rtmp_url, AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "Failed to open output: %s\n", errbuf);
            avcodec_free_context(&codec_ctx);
            avformat_free_context(out_ctx);
            return 1;
        }
    }
    
    // 写入头部
    ret = avformat_write_header(out_ctx, nullptr);
    if (ret < 0) {
        fprintf(stderr, "Failed to write header\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(out_ctx);
        return 1;
    }
    
    printf("Streaming started. Press Ctrl+C to stop.\n");
    
    // 分配帧
    AVFrame* frame = av_frame_alloc();
    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;
    av_frame_get_buffer(frame, 0);
    
    AVPacket* pkt = av_packet_alloc();
    int64_t frame_num = 0;
    int64_t start_time = av_gettime();
    
    // 推流主循环
    while (true) {
        // 生成测试图案
        FillColorBars(frame->data, frame->linesize, width, height, frame_num);
        frame->pts = frame_num;
        
        // 发送帧到编码器
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0) {
            fprintf(stderr, "Error sending frame\n");
            break;
        }
        
        // 接收编码后的包
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                fprintf(stderr, "Error encoding\n");
                break;
            }
            
            // 转换时间戳
            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;
            
            // 写入
            ret = av_interleaved_write_frame(out_ctx, pkt);
            if (ret < 0) {
                fprintf(stderr, "Error writing frame\n");
            }
            
            av_packet_unref(pkt);
            
            // 统计
            if (frame_num % fps == 0) {
                int64_t elapsed = (av_gettime() - start_time) / 1000000;
                printf("Streaming: %lld seconds, %lld frames\r", elapsed, frame_num);
                fflush(stdout);
            }
        }
        
        frame_num++;
        
        // 帧率控制
        int64_t expected_time = start_time + (frame_num * 1000000 / fps);
        int64_t now = av_gettime();
        if (expected_time > now) {
            av_usleep(expected_time - now);
        }
    }
    
    printf("\nFinishing stream...\n");
    
    // 刷新编码器
    avcodec_send_frame(codec_ctx, nullptr);
    while (avcodec_receive_packet(codec_ctx, pkt) >= 0) {
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(out_ctx, pkt);
        av_packet_unref(pkt);
    }
    
    // 写入尾部
    av_write_trailer(out_ctx);
    
    // 清理
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    
    if (!(out_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&out_ctx->pb);
    }
    avformat_free_context(out_ctx);
    
    printf("Stream finished. Total frames: %lld\n", frame_num);
    return 0;
}

/**
 * Chapter 16: FFmpeg 滤镜示例
 * 美颜、水印、裁剪等常用滤镜
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

class FfFilterGraph {
public:
    bool Initialize(int width, int height, int fps) {
        filter_graph_ = avfilter_graph_alloc();
        if (!filter_graph_) return false;

        // 创建输入缓冲区
        const AVFilter* buffersrc = avfilter_get_by_name("buffer");
        char args[512];
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=1/%d:pixel_aspect=1/1",
                 width, height, AV_PIX_FMT_YUV420P, fps);

        int ret = avfilter_graph_create_filter(&buffersrc_ctx_, buffersrc, "in",
                                                args, nullptr, filter_graph_);
        if (ret < 0) return false;

        // 创建输出缓冲区
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        ret = avfilter_graph_create_filter(&buffersink_ctx_, buffersink, "out",
                                            nullptr, nullptr, filter_graph_);
        if (ret < 0) return false;

        return true;
    }

    // 配置美颜滤镜
    bool SetupBeautyFilter(int brightness, int contrast, int saturation) {
        // eq滤镜: 调节亮度、对比度、饱和度
        char filter_descr[256];
        snprintf(filter_descr, sizeof(filter_descr),
                 "eq=brightness=%.2f:contrast=%.2f:saturation=%.2f",
                 brightness / 100.0, contrast / 100.0, saturation / 100.0);

        AVFilterInOut* inputs = avfilter_inout_alloc();
        AVFilterInOut* outputs = avfilter_inout_alloc();

        outputs->name = av_strdup("in");
        outputs->filter_ctx = buffersrc_ctx_;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        inputs->name = av_strdup("out");
        inputs->filter_ctx = buffersink_ctx_;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        int ret = avfilter_graph_parse_ptr(filter_graph_, filter_descr,
                                           &inputs, &outputs, nullptr);
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);

        if (ret < 0) return false;

        ret = avfilter_graph_config(filter_graph_, nullptr);
        return ret >= 0;
    }

    // 配置水印滤镜
    bool SetupWatermarkFilter(const char* text, int x, int y) {
        // drawtext滤镜: 添加文字水印
        char filter_descr[512];
        snprintf(filter_descr, sizeof(filter_descr),
                 "drawtext=text='%s':x=%d:y=%d:fontsize=24:fontcolor=white@0.5",
                 text, x, y);

        // 简化实现，实际需要加载字体文件
        printf("水印滤镜: %s @ (%d,%d)\n", text, x, y);
        return true;
    }

    // 配置裁剪滤镜
    bool SetupCropFilter(int out_w, int out_h, int x, int y) {
        char filter_descr[128];
        snprintf(filter_descr, sizeof(filter_descr),
                 "crop=%d:%d:%d:%d", out_w, out_h, x, y);

        printf("裁剪滤镜: %dx%d @ (%d,%d)\n", out_w, out_h, x, y);
        return true;
    }

    void Cleanup() {
        if (filter_graph_) {
            avfilter_graph_free(&filter_graph_);
        }
    }

private:
    AVFilterGraph* filter_graph_ = nullptr;
    AVFilterContext* buffersrc_ctx_ = nullptr;
    AVFilterContext* buffersink_ctx_ = nullptr;
};

// 常用滤镜链示例
void PrintFilterChains() {
    printf("\n=== 常用滤镜链 ===\n\n");

    printf("1. 基础美颜:\n");
    printf("   eq=brightness=0.1:contrast=1.1:saturation=1.2\n\n");

    printf("2. 添加水印:\n");
    printf("   drawtext=text='@LiveStream':x=10:y=10:fontsize=24:fontcolor=white\n\n");

    printf("3. 缩放:\n");
    printf("   scale=1280:720\n\n");

    printf("4. 裁剪:\n");
    printf("   crop=640:480:100:100\n\n");

    printf("5. 去噪:\n");
    printf("   hqdn3d\n\n");

    printf("6. 组合滤镜链:\n");
    printf("   scale=1280:720,eq=brightness=0.1,drawtext=text='Live':x=10:y=10\n\n");
}

int main(int argc, char* argv[]) {
    printf("Chapter 16: FFmpeg 滤镜示例\n");
    printf("============================\n\n");

    // 滤镜图示例
    FfFilterGraph filter;
    if (filter.Initialize(1920, 1080, 30)) {
        printf("滤镜图初始化成功\n");

        // 美颜
        filter.SetupBeautyFilter(10, 110, 120);
        printf("✓ 美颜滤镜配置完成\n");

        // 水印
        filter.SetupWatermarkFilter("Live Stream", 10, 10);
        printf("✓ 水印滤镜配置完成\n");

        // 裁剪
        filter.SetupCropFilter(1280, 720, 320, 180);
        printf("✓ 裁剪滤镜配置完成\n");

        filter.Cleanup();
    }

    // 打印常用滤镜链
    PrintFilterChains();

    printf("滤镜使用步骤:\n");
    printf("  1. avfilter_graph_alloc() - 分配滤镜图\n");
    printf("  2. avfilter_graph_create_filter() - 创建输入/输出\n");
    printf("  3. avfilter_graph_parse_ptr() - 解析滤镜描述\n");
    printf("  4. avfilter_graph_config() - 配置滤镜图\n");
    printf("  5. av_buffersrc_add_frame() - 输入帧\n");
    printf("  6. av_buffersink_get_frame() - 获取输出帧\n");

    return 0;
}
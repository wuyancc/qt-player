#ifndef VIDEOFILTER_H
#define VIDEOFILTER_H

extern "C" {
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavutil/opt.h"
}

typedef struct VideoFilterContext {
    AVFilterGraph *filter_graph = nullptr;
    AVFilterContext *buffersrc_ctx = nullptr;
    AVFilterContext *buffersink_ctx = nullptr;

    int rotate_angle = 0;       // 0, 90, 180, 270
    bool mirror_h = false;      // 水平镜像
    bool mirror_v = false;      // 垂直镜像

    int width = 0;
    int height = 0;
    AVPixelFormat pix_fmt = AV_PIX_FMT_NONE;

    bool need_reinit = true;
} VideoFilterContext;

class VideoFilter {
public:
    VideoFilter();
    ~VideoFilter();

    // 设置滤镜参数
    void SetRotation(int angle);        // 0, 90, 180, 270
    void SetMirror(bool h, bool v);     // 水平、垂直镜像
    void Reset();

    // 初始化/释放
    int Init(int width, int height, AVPixelFormat pix_fmt);
    void DeInit();

    // 处理一帧
    int ProcessFrame(AVFrame *in_frame, AVFrame *out_frame);

private:
    int InitFilterGraph();
    const char* GetTransposeString();   // 获取transpose参数

    VideoFilterContext ctx_;
};

#endif // VIDEOFILTER_H

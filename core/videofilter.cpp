#include "videofilter.h"
#include "easylogging++.h"

VideoFilter::VideoFilter() {}

VideoFilter::~VideoFilter() {
    DeInit();
}

void VideoFilter::SetRotation(int angle) {
    angle = angle % 360;
    if (angle < 0) angle += 360;
    if (ctx_.rotate_angle != angle) {
        ctx_.rotate_angle = angle;
        ctx_.need_reinit = true;
    }
}

void VideoFilter::SetMirror(bool h, bool v) {
    if (ctx_.mirror_h != h || ctx_.mirror_v != v) {
        ctx_.mirror_h = h;
        ctx_.mirror_v = v;
        ctx_.need_reinit = true;
    }
}

void VideoFilter::Reset() {
    SetRotation(0);
    SetMirror(false, false);
}

void VideoFilter::DeInit() {
    if (ctx_.filter_graph) {
        avfilter_graph_free(&ctx_.filter_graph);
        ctx_.filter_graph = nullptr;
        ctx_.buffersrc_ctx = nullptr;
        ctx_.buffersink_ctx = nullptr;
    }
}

const char* VideoFilter::GetTransposeString() {
    // transpose参数:
    // 0 = 90度逆时针并垂直翻转, 1 = 90度顺时针,
    // 2 = 90度逆时针, 3 = 90度顺时针并垂直翻转
    switch (ctx_.rotate_angle) {
    case 90:  return "1";   // 顺时针90度
    case 180: return "2,2"; // 逆时针90度两次 = 180度 (或用hflip+vflip)
    case 270: return "2";   // 逆时针90度 = 顺时针270度
    default:  return "";
    }
}

int VideoFilter::InitFilterGraph() {
    DeInit();

    ctx_.filter_graph = avfilter_graph_alloc();
    if (!ctx_.filter_graph) {
        LOG(ERROR) << "Failed to allocate filter graph";
        return -1;
    }

    // 构建滤镜字符串
    std::string filter_str;

    // 1. 先处理旋转 (transpose)
    if (ctx_.rotate_angle == 90 || ctx_.rotate_angle == 270) {
        filter_str += "transpose=";
        filter_str += GetTransposeString();
    } else if (ctx_.rotate_angle == 180) {
        // 180度旋转可以用两次transpose=2，或者用hflip+vflip
        filter_str += "transpose=2,transpose=2";
    }

    // 2. 处理镜像
    if (ctx_.mirror_h) {
        if (!filter_str.empty()) filter_str += ",";
        filter_str += "hflip";
    }
    if (ctx_.mirror_v) {
        if (!filter_str.empty()) filter_str += ",";
        filter_str += "vflip";
    }

    // 如果没有滤镜，直接透传
    if (filter_str.empty()) {
        filter_str = "null";
    }

    LOG(INFO) << "Video filter graph: " << filter_str;

    // 创建输入buffer source
    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=1/1:pixel_aspect=1/1",
             ctx_.width, ctx_.height, ctx_.pix_fmt);

    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    int ret = avfilter_graph_create_filter(&ctx_.buffersrc_ctx, buffersrc, "in",
                                           args, nullptr, ctx_.filter_graph);
    if (ret < 0) {
        LOG(ERROR) << "Failed to create buffer source";
        return ret;
    }

    // 创建输出buffer sink
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    ret = avfilter_graph_create_filter(&ctx_.buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, ctx_.filter_graph);
    if (ret < 0) {
        LOG(ERROR) << "Failed to create buffer sink";
        return ret;
    }

    // 解析滤镜图
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    outputs->name = av_strdup("in");
    outputs->filter_ctx = ctx_.buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = ctx_.buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    ret = avfilter_graph_parse_ptr(ctx_.filter_graph, filter_str.c_str(),
                                   &inputs, &outputs, nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    if (ret < 0) {
        LOG(ERROR) << "Failed to parse filter graph";
        return ret;
    }

    ret = avfilter_graph_config(ctx_.filter_graph, nullptr);
    if (ret < 0) {
        LOG(ERROR) << "Failed to configure filter graph";
        return ret;
    }

    ctx_.need_reinit = false;
    return 0;
}

int VideoFilter::Init(int width, int height, AVPixelFormat pix_fmt) {
    // 只在尺寸或格式变化时才需要重新初始化
    if (ctx_.width != width || ctx_.height != height || ctx_.pix_fmt != pix_fmt) {
        ctx_.width = width;
        ctx_.height = height;
        ctx_.pix_fmt = pix_fmt;
        ctx_.need_reinit = true;
    }
    return 0;
}

int VideoFilter::ProcessFrame(AVFrame *in_frame, AVFrame *out_frame) {

    if (ctx_.need_reinit || !ctx_.filter_graph) {
        int ret = InitFilterGraph();
        if (ret < 0) return ret;
    }

    // 送入输入帧
    int ret = av_buffersrc_add_frame_flags(ctx_.buffersrc_ctx, in_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) {
        LOG(ERROR) << "Failed to add frame to buffer source";
        return ret;
    }

    // 从输出获取处理后的帧
    ret = av_buffersink_get_frame(ctx_.buffersink_ctx, out_frame);
    if (ret < 0) {
        return ret; // EAGAIN or EOF
    }

    return 0;
}

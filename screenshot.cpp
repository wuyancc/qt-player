#include "screenshot.h"
#include "easylogging++.h"
AVFrame *allocate_sws_frame(AVCodecContext *enc_ctx)
{
    int ret = 0;
    AVFrame *sws_frame = av_frame_alloc();
    if(sws_frame)
    {
        sws_frame->format = enc_ctx->pix_fmt;
        sws_frame->width = enc_ctx->width;
        sws_frame->height = enc_ctx->height;
        sws_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = av_frame_get_buffer(sws_frame, 32);   // 分配buffer
        if(ret <0)
        {
            av_frame_free(&sws_frame);
            return NULL;
        }
    }
    return sws_frame;
}

ScreenShot::ScreenShot()
{

}

int ScreenShot::SaveJpeg(AVFrame *src_frame, const char *file_name, int jpeg_quality)
{
    AVFormatContext* ofmt_ctx = NULL;
    AVOutputFormat* fmt = NULL;
    AVStream* video_st = NULL;
    AVCodecContext* enc_ctx = NULL;
    AVCodec* codec = NULL;
    AVFrame* picture = NULL;
    AVPacket *pkt = NULL;
    int got_picture = 0;
    int ret = 0;
    struct  SwsContext *img_convert_ctx = NULL;

    ofmt_ctx = avformat_alloc_context();
    //Guess format
    fmt = av_guess_format("mjpeg", NULL, NULL);
    ofmt_ctx->oformat = fmt;
    //Output URL
    if (avio_open(&ofmt_ctx->pb, file_name, AVIO_FLAG_READ_WRITE) < 0){
        LOG(ERROR) <<"Couldn't open output file.";
        ret = -1;
        goto fail;
    }

    video_st = avformat_new_stream(ofmt_ctx, 0);
    if (video_st==NULL){
        ret = -1;
        goto fail;
    }
    enc_ctx = video_st->codec;
    enc_ctx->codec_id = AV_CODEC_ID_MJPEG;      // mjpeg支持的编码器
    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P; // AV_CODEC_ID_MJPEG 支持的像素格式

    enc_ctx->width  = src_frame->width;
    enc_ctx->height = src_frame->height;

    enc_ctx->time_base.num = 1;
    enc_ctx->time_base.den = 25;
    //Output some information
    av_dump_format(ofmt_ctx, 0, file_name, 1);

    codec = avcodec_find_encoder(enc_ctx->codec_id);
    if (!codec){
        LOG(ERROR) << "jpeg Codec not found.";
        ret = -1;
        goto fail;
    }
    if (avcodec_open2(enc_ctx, codec,NULL) < 0){
        LOG(ERROR) << "Could not open jpeg codec.";
        ret = -1;
        goto fail;
    }
    ret = avcodec_parameters_from_context(video_st->codecpar, enc_ctx);
    if(ret < 0) {
        LOG(ERROR) << "avcodec_parameters_from_context failed";
        ret = -1;
        goto fail;
    }
    if(src_frame->format != enc_ctx->pix_fmt) {
        img_convert_ctx = sws_getContext(enc_ctx->width, enc_ctx->height,
                                         (enum AVPixelFormat)src_frame->format, enc_ctx->width, enc_ctx->height,
                                         enc_ctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
        if (!img_convert_ctx) {
            LOG(ERROR) << "Impossible to create scale context for the conversion fmt:"
                       << av_get_pix_fmt_name((enum AVPixelFormat)src_frame->format)
                       << ", s:" <<  enc_ctx->width << "x" << enc_ctx->height << " -> fmt:" << av_get_pix_fmt_name(enc_ctx->pix_fmt)
                       << ", s:" <<  enc_ctx->width << "x" << enc_ctx->height ;
            ret = -1;
            goto fail;
        }
    }

    if(jpeg_quality > 0)
    {
        if(jpeg_quality > 100)
            jpeg_quality = 100;

        enc_ctx->qcompress = (float)jpeg_quality/100.f; // 0~1.0, default is 0.5
        enc_ctx->qmin = 2;
        enc_ctx->qmax = 31;
        enc_ctx->max_qdiff = 3;

        LOG(ERROR) <<"JPEG quality is: %d" << jpeg_quality;
    }
    pkt = av_packet_alloc();
    //Write Header
    ret = avformat_write_header(ofmt_ctx, NULL);
    if(ret < 0) {
        LOG(ERROR) <<"avformat_write_header failed";
        ret = -1;
        goto fail;
    }

    if(img_convert_ctx)     // 如果需要转换pix_fmt
    {
        // 分配转换后的frame
        picture = allocate_sws_frame(enc_ctx);
        /* make sure the frame data is writable */
        ret = av_frame_make_writable(picture);
        ret = sws_scale(img_convert_ctx, (const uint8_t **) src_frame->data, src_frame->linesize, 0, src_frame->height,
                        picture->data, picture->linesize);
        picture->pts = 0;
        ret = avcodec_encode_video2(enc_ctx, pkt, picture, &got_picture);
    }
    else
    {
        ret = avcodec_encode_video2(enc_ctx, pkt, src_frame, &got_picture);
    }

    if(ret < 0){
        LOG(ERROR) <<"avcodec_encode_video2 Error.";
        ret = -1;
        goto fail;
    }
    if (got_picture==1){
        pkt->stream_index = video_st->index;
        ret = av_write_frame(ofmt_ctx, pkt);
        if(ret < 0) {
            LOG(ERROR) <<"av_write_frame Error.";
            ret = -1;
            goto fail;
        }
    }else {
        LOG(ERROR) <<"no got_picture";
        ret = -1;
        goto fail;
    }
    ret = 0;
fail:
    //Write Trailer
    ret = av_write_trailer(ofmt_ctx);
    if(ret < 0)
        LOG(ERROR) <<"av_write_trailer Error.";
    if(pkt)
        av_packet_free(&pkt);
    if (enc_ctx)
        avcodec_close(enc_ctx);
    if(picture)
        av_frame_free(&picture);
    if(ofmt_ctx && ofmt_ctx->pb)
        avio_close(ofmt_ctx->pb);
    if(ofmt_ctx)
        avformat_free_context(ofmt_ctx);
    if(img_convert_ctx)
        sws_freeContext(img_convert_ctx);

    return ret;
}


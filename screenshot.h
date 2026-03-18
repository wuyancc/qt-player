#ifndef SCREENSHOT_H
#define SCREENSHOT_H
#ifdef __cplusplus
extern "C" {
#endif
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
//#include <strings.h>
#include <stdio.h>
#ifdef __cplusplus
}
#endif

class ScreenShot
{
public:
    ScreenShot();
    /**
     * @brief SaveJpeg 将frame保存位jpeg图片
     * @param src_frame 要保存的帧
     * @param file_name 保存的图片路径
     * @param jpeg_quality  图片质量
     * @return
     */
    int SaveJpeg(AVFrame *src_frame, const char* file_name, int jpeg_quality);
};

#endif // SCREENSHOT_H

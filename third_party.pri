# 第三方库模块
# 包含 FFmpeg、SDL2 等外部依赖库的配置

win32 {
    INCLUDEPATH += $$PWD/ffmpeg-4.2.1-win32-dev/include
    INCLUDEPATH += $$PWD/SDL2/include

    LIBS += $$PWD/ffmpeg-4.2.1-win32-dev/lib/avformat.lib   \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/avcodec.lib    \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/avdevice.lib   \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/avfilter.lib   \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/avutil.lib     \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/postproc.lib   \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/swresample.lib \
            $$PWD/ffmpeg-4.2.1-win32-dev/lib/swscale.lib    \
            $$PWD/SDL2/lib/x86/SDL2.lib \
            "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x86\Ole32.Lib"
}

# 解决 FFmpeg 在 C++ 环境下的宏定义冲突问题
DEFINES += __STDC_CONSTANT_MACROS

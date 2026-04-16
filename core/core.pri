# 核心播放引擎模块
# 包含 FFmpeg 解码、渲染、视频滤镜等核心功能

SOURCES += \
    $$PWD/ff_ffplay.cpp \
    $$PWD/ff_ffplay_def.cpp \
    $$PWD/ijkmediaplayer.cpp \
    $$PWD/videofilter.cpp

HEADERS += \
    $$PWD/ff_ffplay.h \
    $$PWD/ff_ffplay_def.h \
    $$PWD/ijkmediaplayer.h \
    $$PWD/imagescaler.h \
    $$PWD/ff_fferror.h \
    $$PWD/videofilter.h

#-------------------------------------------------
#
# Project created by QtCreator 2023-04-27T17:30:38
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = media_player
TEMPLATE = app
CONFIG +=   c++17
# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

RC_ICONS = player.ico

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        homewindow.cpp \
    ff_ffplay.cpp \
    ff_ffplay_def.cpp \
    ijkmediaplayer.cpp \
    displaywind.cpp \
    ffmsg_queue.cpp \
    globalhelper.cpp \
    medialist.cpp \
    playlist.cpp \
    urldialog.cpp \
    customslider.cpp \
    util.cpp \
    sonic.cpp \
    screenshot.cpp \
    toast.cpp \
    ijksdl_timer.cpp \
    log/easylogging++.cc \
    videofilter.cpp

HEADERS += \
        homewindow.h \
    medialist.h \
    ff_ffplay.h \
    ff_ffplay_def.h \
    ijkmediaplayer.h \
    displaywind.h \
    imagescaler.h \
    ff_fferror.h \
    ffmsg.h \
    ffmsg_queue.h \
    globalhelper.h \
    playlist.h \
    urldialog.h \
    customslider.h \
    util.h \
    sonic.h \
    screenshot.h \
    toast.h \
    ijksdl_timer.h \
    log/easylogging++.h \
    videofilter.h

FORMS += \
        homewindow.ui \
    displaywind.ui \
    playlist.ui \
    urldialog.ui


win32 {
INCLUDEPATH += $$PWD/ffmpeg-4.2.1-win32-dev/include
INCLUDEPATH += $$PWD/SDL2/include
INCLUDEPATH += $$PWD/log
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
#LIBS += -lOle32
}

RESOURCES += \
    resource.qrc

# 解决 FFmpeg 在 C++ 环境下的宏定义冲突问题
DEFINES += __STDC_CONSTANT_MACROS

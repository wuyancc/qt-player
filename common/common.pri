# 公共基础模块
# 包含消息队列、工具类、日志、音频处理等

SOURCES += \
    $$PWD/ffmsg_queue.cpp \
    $$PWD/globalhelper.cpp \
    $$PWD/util.cpp \
    $$PWD/sonic.cpp \
    $$PWD/ijksdl_timer.cpp \
    $$PWD/log/easylogging++.cc

HEADERS += \
    $$PWD/ffmsg.h \
    $$PWD/ffmsg_queue.h \
    $$PWD/globalhelper.h \
    $$PWD/util.h \
    $$PWD/sonic.h \
    $$PWD/ijksdl_timer.h \
    $$PWD/log/easylogging++.h

INCLUDEPATH += $$PWD/log

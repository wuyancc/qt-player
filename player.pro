#-------------------------------------------------
#
# Project created by QtCreator 2023-04-27T17:30:38
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = media_player
TEMPLATE = app
CONFIG += c++17

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
    main.cpp

INCLUDEPATH += \
    $$PWD/core \
    $$PWD/ui \
    $$PWD/media \
    $$PWD/common \
    $$PWD/common/log

# 模块化 pri 文件
include(core/core.pri)
include(ui/ui.pri)
include(media/media.pri)
include(common/common.pri)
include(third_party.pri)

RESOURCES += \
    resource.qrc

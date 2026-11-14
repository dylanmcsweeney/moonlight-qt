QT -= core gui

TEMPLATE = app
TARGET = tst_bitstream

CONFIG += console testcase
CONFIG -= app_bundle

*-g++ {
    QMAKE_CFLAGS += -std=gnu99
}

INCLUDEPATH += $$PWD/../../h264bitstream

SOURCES += \
    $$PWD/tst_bitstream.c \
    $$PWD/../../h264bitstream/h264_nal.c \
    $$PWD/../../h264bitstream/h264_stream.c

HEADERS += \
    $$PWD/../../h264bitstream/bs.h \
    $$PWD/../../h264bitstream/h264_stream.h

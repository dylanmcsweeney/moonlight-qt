TEMPLATE = app
TARGET = tst_lifecyclebatch3

QT += core testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle

INCLUDEPATH += $$PWD/../../app

SOURCES += tst_lifecyclebatch3.cpp

HEADERS += \
    ../../app/displaybounds.h \
    ../../app/logfile.h \
    ../../app/streaming/input/steamcontroller.h

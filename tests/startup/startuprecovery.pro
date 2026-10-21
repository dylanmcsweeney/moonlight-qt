TEMPLATE = app
TARGET = tst_uistartuprecovery

QT += testlib
QT -= gui
CONFIG += console testcase c++17
CONFIG -= app_bundle

SOURCES += \
    $$PWD/tst_uistartuprecovery.cpp \
    $$PWD/../../app/settings/uistartuprecovery.cpp

HEADERS += $$PWD/../../app/settings/uistartuprecovery.h

INCLUDEPATH += $$PWD/../../app/settings

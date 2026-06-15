QT += core gui qml quick quickcontrols2

CONFIG += c++17 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = moonlight-gui-performance

SOURCES += main.cpp

RESOURCES += gui-performance.qrc

include(../../globaldefs.pri)

win32 {
    LIBS += dxgi.lib
}

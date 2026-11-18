TEMPLATE = app
TARGET = tst_keyboard
QT += quick network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
DEFINES += SDL_MAIN_HANDLED

APP = $$PWD/../../app
INCLUDEPATH += $$APP \
               $$PWD/../../moonlight-common-c/moonlight-common-c/src \
               $$PWD/../../qmdnsengine \
               $$PWD/../../qmdnsengine/qmdnsengine/src/include

win32 {
    SDL_ARCH = $$QT_ARCH
    contains(QT_ARCH, x86_64): SDL_ARCH = x64
    contains(QT_ARCH, i386): SDL_ARCH = x86
    INCLUDEPATH += $$PWD/../../libs/windows/include $$PWD/../../libs/windows/include/$$SDL_ARCH/SDL2
    LIBS += -L$$PWD/../../libs/windows/lib/$$SDL_ARCH -lSDL2
} else:macx:!disable-prebuilts {
    INCLUDEPATH += $$PWD/../../libs/mac/include $$PWD/../../libs/mac/include/SDL2
    LIBS += -L$$PWD/../../libs/mac/lib -lSDL2
    QMAKE_RPATHDIR += $$PWD/../../libs/mac/lib
} else:unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += sdl2 SDL2_ttf opus
}

HEADERS += $$APP/settings/streamingpreferences.h
SOURCES += tst_keyboard.cpp keyboard_stubs.cpp $$APP/streaming/input/keyboard.cpp

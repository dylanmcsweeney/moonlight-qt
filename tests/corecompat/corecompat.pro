TEMPLATE = app
TARGET = tst_corecompat_inputmotion
CONFIG += console c11
CONFIG -= app_bundle
QT -= gui core

COMMON = $$clean_path($$PWD/../../moonlight-common-c/moonlight-common-c)
INCLUDEPATH += $$COMMON/src \
               $$COMMON/nanors \
               $$COMMON/nanors/deps \
               $$COMMON/nanors/deps/obl \
               $$COMMON/enet/include

# InputStream.c is included by the fixture for queue inspection. Its dependencies
# come from the production streaming-core library. Standalone test builds can
# point to an existing app build via the environment inherited by sub-qmake.
CORE_BUILD_DIR = $$(MOONLIGHT_CORECOMPAT_BUILD_DIR)
isEmpty(CORE_BUILD_DIR): CORE_BUILD_DIR = $$OUT_PWD/../../moonlight-common-c
CORE_BUILD_DIR = $$clean_path($$CORE_BUILD_DIR)
win32:CONFIG(release, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/release
else:win32:CONFIG(debug, debug|release): CORE_BUILD_DIR = $$CORE_BUILD_DIR/debug
LIBS += "-L$$CORE_BUILD_DIR" -lmoonlight-common-c

*-msvc {
    QMAKE_CFLAGS += -std:c11 -experimental:c11atomics /Gy
    QMAKE_LFLAGS += /OPT:REF
}

win32 {
    contains(QT_ARCH, i386) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/x86
        LIBS += -L$$PWD/../../libs/windows/lib/x86
    }
    contains(QT_ARCH, x86_64) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/x64
        LIBS += -L$$PWD/../../libs/windows/lib/x64
    }
    contains(QT_ARCH, arm64) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/arm64
        LIBS += -L$$PWD/../../libs/windows/lib/arm64
    }

    INCLUDEPATH += $$PWD/../../libs/windows/include
    LIBS += -llibssl -llibcrypto -lws2_32 -lwinmm
}
macx:!disable-prebuilts {
    MAC_LIBS = $$clean_path($$PWD/../../libs/mac)
    !exists($$MAC_LIBS/include)|!exists($$MAC_LIBS/lib) {
        error("Missing dependencies. Please run 'python3 setup-deps.py' to fetch prebuilt libraries.")
    }

    INCLUDEPATH += $$MAC_LIBS/include
    LIBS += -L$$MAC_LIBS/lib -lssl.3 -lcrypto.3
    QMAKE_RPATHDIR += $$MAC_LIBS/lib
}

unix:if(!macx|disable-prebuilts) {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl
}
unix:!macx {
    LIBS += -pthread
}

SOURCES += tst_inputmotion.c

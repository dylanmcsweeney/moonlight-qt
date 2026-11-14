TEMPLATE = app
TARGET = tst_backendreliability

QT += core gui network testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle

INCLUDEPATH += \
    $$PWD/../../app \
    $$PWD/../../moonlight-common-c/moonlight-common-c/src

SOURCES += \
    tst_backendreliability.cpp \
    ../../app/backend/identitymanager.cpp \
    ../../app/backend/nvaddress.cpp \
    ../../app/backend/nvapp.cpp \
    ../../app/backend/nvhttp.cpp \
    ../../app/backend/nvpairingmanager.cpp

HEADERS += \
    ../../app/backend/identitymanager.h \
    ../../app/backend/nvaddress.h \
    ../../app/backend/nvapp.h \
    ../../app/backend/nvcomputer.h \
    ../../app/backend/nvhttp.h \
    ../../app/backend/nvpairingmanager.h

win32 {
    contains(QT_ARCH, x86_64) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/x64
        LIBS += -L$$PWD/../../libs/windows/lib/x64
    } else:contains(QT_ARCH, arm64) {
        INCLUDEPATH += $$PWD/../../libs/windows/include/arm64
        LIBS += -L$$PWD/../../libs/windows/lib/arm64
    }
    INCLUDEPATH += $$PWD/../../libs/windows/include
    LIBS += -llibssl -llibcrypto
}

macx:!disable-prebuilts {
    INCLUDEPATH += $$PWD/../../libs/mac/include
    LIBS += -L$$PWD/../../libs/mac/lib -lssl.3 -lcrypto.3
}

unix:if(!macx|disable-prebuilts) {
    CONFIG += link_pkgconfig
    PKGCONFIG += openssl
}

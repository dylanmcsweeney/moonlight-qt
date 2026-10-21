QT += core gui qml quick quickcontrols2 testlib
CONFIG += testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = streamprofile-tests

DEFINES += VERSION_STR=\\\"$$cat(../app/version.txt)\\\"
DEFINES += TEST_APP_GUI_DIR=\\\"$$PWD/../app/gui\\\"

INCLUDEPATH += ../app

win32: LIBS += -ldxgi

SOURCES += \
    test_streamprofiles.cpp \
    wm_stubs.cpp \
    ../app/cli/commandlineparser.cpp \
    ../app/diagnostics/diagnosticcapture.cpp \
    ../app/diagnostics/diagnosticzip.cpp \
    ../app/settings/buildidentity.cpp \
    ../app/settings/streamingpreferences.cpp \
    ../app/settings/streamprofilemanager.cpp \
    ../app/settings/uistartuprecovery.cpp \
    ../app/gui/uirenderermanager.cpp \
    ../app/streaming/vrrratepolicy.cpp

HEADERS += \
    ../app/diagnostics/diagnosticcapture.h \
    ../app/diagnostics/diagnosticzip.h \
    ../app/settings/buildidentity.h \
    ../app/settings/streamingpreferences.h \
    ../app/settings/streamprofilemanager.h \
    ../app/settings/uistartuprecovery.h \
    ../app/gui/uirenderermanager.h \
    ../app/streaming/vrrratepolicy.h

RESOURCES += tests.qrc

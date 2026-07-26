QT += core qml testlib
CONFIG += testcase c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = streamprofile-tests

DEFINES += VERSION_STR=\\\"$$cat(../app/version.txt)\\\"

INCLUDEPATH += ../app

SOURCES += \
    test_streamprofiles.cpp \
    wm_stubs.cpp \
    ../app/cli/commandlineparser.cpp \
    ../app/settings/buildidentity.cpp \
    ../app/settings/streamingpreferences.cpp \
    ../app/settings/streamprofilemanager.cpp \
    ../app/streaming/vrrratepolicy.cpp

HEADERS += \
    ../app/settings/buildidentity.h \
    ../app/settings/streamingpreferences.h \
    ../app/settings/streamprofilemanager.h \
    ../app/streaming/vrrratepolicy.h

RESOURCES += tests.qrc

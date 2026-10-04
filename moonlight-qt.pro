TEMPLATE = subdirs
SUBDIRS = \
    moonlight-common-c \
    qmdnsengine \
    app

# Build the dependencies in parallel before the final app
app.depends = qmdnsengine moonlight-common-c
win32:!winrt {
    SUBDIRS += AntiHooking
    app.depends += AntiHooking
}

!disable-h264bitstream {
    SUBDIRS += h264bitstream
    app.depends += h264bitstream
}

# PyroWave codec library (see pyrowave/VENDOR.txt). Must match the condition
# in app/app.pro. Linux decodes with Vulkan and presents through libplacebo.
win32:!winrt:contains(QT_ARCH, x86_64):!disable-pyrowave {
    SUBDIRS += pyrowave
    app.depends += pyrowave
}
linux:contains(QT_ARCH, x86_64):!disable-pyrowave:!disable-libplacebo:packagesExist(libplacebo) {
    SUBDIRS += pyrowave
    app.depends += pyrowave
}

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Run host-side profile and QML tests with normal builds. Steam Link is a
# cross-build and cannot execute them. CONFIG+=tests also enables the opt-in
# VRR, haptics, and PyroWave suites inside tests/tests.pro.
load(configure)
qtCompileTest(SL)
qtCompileTest(EGL)

!config_SL {
    SUBDIRS += tests
    tests.depends = app
}

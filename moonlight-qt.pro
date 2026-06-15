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

# Support debug and release builds from command line for CI
CONFIG += debug_and_release

# Deterministic VRR tests are deliberately opt-in. Package and normal
# application builds keep their existing target set unless CONFIG+=tests is
# supplied to qmake.
contains(CONFIG, tests) {
    SUBDIRS += tests
}

# Run our compile tests
load(configure)
qtCompileTest(SL)
qtCompileTest(EGL)

# Unit tests are host-side tools and cannot run in the Steam Link cross-build.
!config_SL {
    SUBDIRS += tests
    tests.depends = app
}

# Startup recovery, stream profile, bitstream, backend reliability, and lifecycle
# suites run in the normal test gate. The opt-in flag adds other suites.
TEMPLATE = subdirs
CONFIG += ordered

streamprofiles.file = streamprofile-tests.pro
startup.file = startup/startuprecovery.pro
bitstream.file = bitstream/bitstream.pro
backendreliability.file = backend-batch2/backendreliability.pro
lifecyclebatch3.file = lifecycle-batch3/lifecyclebatch3.pro
corecompat.file = corecompat/corecompat.pro
keyboard.file = keyboard/keyboard.pro
SUBDIRS += streamprofiles startup bitstream backendreliability lifecyclebatch3 corecompat keyboard

contains(CONFIG, tests) {
    powerActions.file = $$PWD/power/poweractions.pro
    powerActionView.file = $$PWD/power/poweractionview.pro
    SUBDIRS += powerActions powerActionView
    launchWarning.file = $$PWD/backend/launchwarning.pro
    SUBDIRS += launchWarning
    SUBDIRS += vrr haptics pyrowave
} else {
    message(Normal stream profile, startup recovery, bitstream, backend, and lifecycle suites are enabled; rerun qmake with CONFIG+=tests for optional suites)
}

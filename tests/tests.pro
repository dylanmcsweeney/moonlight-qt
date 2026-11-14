# Startup recovery, stream profile, bitstream, and backend reliability suites run
# in the normal test gate. The opt-in flag adds other platform and diagnostics suites.
TEMPLATE = subdirs
CONFIG += ordered

streamprofiles.file = streamprofile-tests.pro
startup.file = startup/startuprecovery.pro
bitstream.file = bitstream/bitstream.pro
backendreliability.file = backend-batch2/backendreliability.pro
SUBDIRS += streamprofiles startup bitstream backendreliability

contains(CONFIG, tests) {
    SUBDIRS += vrr haptics pyrowave
} else {
    message(Normal stream profile plus startup recovery plus bitstream and backend suites are enabled; rerun qmake with CONFIG+=tests for optional suites)
}

# Startup recovery and stream profile suites run in the normal test gate. The
# opt-in flag adds platform and diagnostics suites that are not part of the app build.
TEMPLATE = subdirs
CONFIG += ordered

streamprofiles.file = streamprofile-tests.pro
startup.file = startup/startuprecovery.pro
SUBDIRS += streamprofiles startup

contains(CONFIG, tests) {
    SUBDIRS += vrr haptics pyrowave
} else {
    message(Only stream profile tests are enabled; rerun qmake with CONFIG+=tests for additional suites)
}

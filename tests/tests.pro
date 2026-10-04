# The stream profile suite is part of the normal CI test gate. The opt-in flag
# adds platform and diagnostics suites that are not part of the app build.
TEMPLATE = subdirs
CONFIG += ordered

streamprofiles.file = streamprofile-tests.pro
SUBDIRS += streamprofiles

contains(CONFIG, tests) {
    SUBDIRS += vrr haptics pyrowave
} else {
    message(Only stream profile tests are enabled; rerun qmake with CONFIG+=tests for additional suites)
}

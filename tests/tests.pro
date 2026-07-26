# The test tree is intentionally opt-in.  The application and package builds
# do not enter it unless their qmake invocation explicitly adds CONFIG+=tests.
TEMPLATE = subdirs
CONFIG += ordered

streamprofiles.file = streamprofile-tests.pro

contains(CONFIG, tests) {
    SUBDIRS += streamprofiles vrr
} else {
    message(Tests are disabled; rerun qmake with CONFIG+=tests)
}

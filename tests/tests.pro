# The test tree is intentionally opt-in.  The application and package builds
# do not enter it unless their qmake invocation explicitly adds CONFIG+=tests.
TEMPLATE = subdirs
CONFIG += ordered

contains(CONFIG, tests) {
    SUBDIRS += streamprofile-tests vrr
} else {
    message(Tests are disabled; rerun qmake with CONFIG+=tests)
}

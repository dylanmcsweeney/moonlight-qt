# A header dependency makes version changes visible to incremental C++ builds.
QMAKE_INTERNAL_INCLUDED_FILES += $$PWD/version.txt
versionHeader.input = $$PWD/moonlightversion.h.in
versionHeader.output = $$OUT_PWD/moonlightversion.h
QMAKE_SUBSTITUTES += versionHeader
INCLUDEPATH += $$OUT_PWD

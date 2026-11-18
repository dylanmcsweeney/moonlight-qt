# Bitstream boundary tests

Build this suite from an out-of-source build directory:

    qmake /path/to/moonlight-qt/tests/bitstream/bitstream.pro
    make
    ./tst_bitstream

Use the Qt kit's equivalent build command on Windows (nmake for MSVC); run
tst_bitstream.exe from the selected debug or release output directory.

To build the normal test gate in a separate directory, first build Moonlight
with the same Qt kit and architecture so the core compatibility fixture can
link its production library. Point the test build at that core output:

    export MOONLIGHT_CORECOMPAT_BUILD_DIR=/path/to/app-build/moonlight-common-c
    qmake /path/to/moonlight-qt/tests/tests.pro
    make

In PowerShell, set `$env:MOONLIGHT_CORECOMPAT_BUILD_DIR` to the same directory
in the Windows app build. Select the same debug or release configuration for
both builds. Add `CONFIG+=tests` to enable the optional platform and diagnostics
suites.

The tests compile the production h264bitstream sources and cover signed
Exp-Golomb boundary encodings, writes wider than 32 bits, SPS/HRD array
clamps, guarded buffers, and malformed NAL parsing/serialization. The
FFmpeg writeBuffer() fallback for failed SPS fixup is covered by source
review; it does not have a runtime unit test because that path depends on the
full decoder setup.

# Bitstream boundary tests

Build this suite from an out-of-source build directory:

    qmake /path/to/moonlight-qt/tests/bitstream/bitstream.pro
    make
    ./tst_bitstream

Use the Qt kit's equivalent build command on Windows (nmake for MSVC); run
tst_bitstream.exe from the selected debug or release output directory.

To build the normal test gate, configure tests/tests.pro from a separate
out-of-source build directory:

    qmake /path/to/moonlight-qt/tests/tests.pro
    make

This builds the stream profile, startup recovery, bitstream, and backend
reliability suites. Add CONFIG+=tests to the qmake command to enable the
optional platform and diagnostics suites.

The tests compile the production h264bitstream sources and cover signed
Exp-Golomb boundary encodings, writes wider than 32 bits, SPS/HRD array
clamps, guarded buffers, and malformed NAL parsing/serialization. The
FFmpeg writeBuffer() fallback for failed SPS fixup is covered by source
review; it does not have a runtime unit test because that path depends on the
full decoder setup.

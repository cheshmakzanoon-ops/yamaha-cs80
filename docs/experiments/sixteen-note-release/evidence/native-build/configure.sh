#!/bin/bash
set -euo pipefail
ROOT=/workspace/scratch/ee447c520988
PREFIX=$ROOT/mpe-native-tools/sysroot
export PATH=$ROOT/pitch-power-recovery/tools/cmake/data/bin:$ROOT/pitch-power-recovery/tools/bin:$PATH
export CMAKE_BUILD_PARALLEL_LEVEL=2
export CMAKE_PREFIX_PATH=$PREFIX/usr
export CFLAGS="-idirafter $PREFIX/usr/include -idirafter $PREFIX/usr/include/x86_64-linux-gnu"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$PREFIX/usr/lib/x86_64-linux-gnu -Wl,-rpath-link,$PREFIX/usr/lib/x86_64-linux-gnu"
export PKG_CONFIG_SYSROOT_DIR=$PREFIX
export PKG_CONFIG_LIBDIR=$PREFIX/usr/lib/x86_64-linux-gnu/pkgconfig:$PREFIX/usr/lib/pkgconfig:$PREFIX/usr/share/pkgconfig
cmake -S "$ROOT/sixteen-note-release" -B "$ROOT/build-sixteen-note-release-native" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_XVFB_RUN="$PREFIX/usr/bin/xvfb-run" -DFETCHCONTENT_SOURCE_DIR_JUCE="$ROOT/mpe-native-tools/JUCE" -DCMAKE_PREFIX_PATH="$PREFIX/usr" -DCMAKE_C_FLAGS="-idirafter $PREFIX/usr/include -idirafter $PREFIX/usr/include/x86_64-linux-gnu" -DCMAKE_CXX_FLAGS="-idirafter $PREFIX/usr/include -idirafter $PREFIX/usr/include/x86_64-linux-gnu" -DCMAKE_EXE_LINKER_FLAGS="-L$PREFIX/usr/lib/x86_64-linux-gnu -Wl,-rpath-link,$PREFIX/usr/lib/x86_64-linux-gnu" -DCMAKE_MAKE_PROGRAM="$ROOT/pitch-power-recovery/tools/bin/ninja"

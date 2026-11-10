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
cmake --build "$ROOT/build-mpe-released-target-native" --target CS80Snapshots --parallel 2

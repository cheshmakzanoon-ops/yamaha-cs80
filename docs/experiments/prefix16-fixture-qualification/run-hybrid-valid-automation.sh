#!/bin/bash
set -u
cd "$(dirname "$0")"
export PYTHONPATH="$PWD/build-tools";export PATH="$PWD/build-tools/bin:$PATH"
prefix="$PWD/linux-deps/sysroot/usr"
export PATH="$prefix/bin:$PATH"
export CPLUS_INCLUDE_PATH="$prefix/include:$prefix/include/x86_64-linux-gnu:$prefix/include/freetype2"
export C_INCLUDE_PATH="$CPLUS_INCLUDE_PATH"
export LIBRARY_PATH="$prefix/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="$LIBRARY_PATH"
export PKG_CONFIG_PATH="$prefix/lib/x86_64-linux-gnu/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="$PWD/linux-deps/sysroot"
e="$PWD/hybrid-valid-automation-probe"
for variant in baseline candidate;do
 b=hybrid-baseline-automation-build;[ "$variant" != candidate ]||b=hybrid-full-build
 module=$(find "$b/CS80Reference_artefacts" -name '*.so' -print -quit)
 echo "$e/test $module audio-root-recovery/tests/parity/tolerances-v2/host-automation.json" >> "$e/run-commands.txt"
 "$e/test" "$module" audio-root-recovery/tests/parity/tolerances-v2/host-automation.json > "$e/$variant.log" 2>&1
 s=$?;echo $s > "$e/$variant-exit.txt"
done

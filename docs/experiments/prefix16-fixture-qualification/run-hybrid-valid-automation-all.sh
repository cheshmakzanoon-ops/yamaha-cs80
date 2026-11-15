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
for rate in 44100 48000 96000;do
 for mode in realtime offline;do
  for variant in baseline candidate;do
   b=hybrid-baseline-automation-build;[ "$variant" != candidate ]||b=hybrid-full-build
   module=$(find "$b/CS80Reference_artefacts" -name '*.so' -print -quit)
   name="$variant-$rate-$mode"
   echo "$e/test $module $e/tolerances/host-automation.json $rate $mode" >> "$e/all-run-commands.txt"
   "$e/test" "$module" "$e/tolerances/host-automation.json" "$rate" "$mode" > "$e/$name.log" 2>&1
   s=$?;echo $s > "$e/$name-exit.txt";echo "$name exit=$s"
  done
 done
done

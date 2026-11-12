#!/bin/bash
. build/recovery-env.sh
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/../yamaha-cs80/build/_deps/juce-src" -DFETCHCONTENT_SOURCE_DIR_CS80_LIBXML2="$PWD/../yamaha-cs80/build-native/_deps/cs80_libxml2-src" -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON >build/configure.log 2>&1
status=$?; echo "$status" > build/configure.exit
if [ "$status" -ne 0 ]; then exit "$status"; fi
cmake --build build-native --target CS80Reference_VST3 cs80_vst3_ribbon_automation_tests --parallel 2 >build/build.log 2>&1
status=$?; echo "$status" >build/build.exit; exit "$status"

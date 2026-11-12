#!/bin/bash
. build/recovery-env.sh
SDK="$PWD/../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK"
g++ -std=c++20 -O2 -Isrc -I"$SDK" tests/Vst3RibbonReleaseModesAutomationTests.cpp "$SDK/pluginterfaces/base/coreiids.cpp" "$SDK/pluginterfaces/base/funknown.cpp" "$SDK/public.sdk/source/vst/vstinitiids.cpp" -ldl -o build/cs80_vst3_ribbon_release_modes_automation_tests >build/build.log 2>&1
status=$?; echo $status >build/build.exit; exit $status

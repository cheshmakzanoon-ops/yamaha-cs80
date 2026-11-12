#!/bin/bash
. build/recovery-env.sh
SDK="$PWD/../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK"
g++ -std=c++20 -O2 -Isrc -I"$SDK" tests/Vst3KbdContourCombinedAutomationTests.cpp "$SDK/pluginterfaces/base/coreiids.cpp" "$SDK/pluginterfaces/base/funknown.cpp" "$SDK/public.sdk/source/vst/vstinitiids.cpp" -ldl -o build/cs80_vst3_kbd_contour_combined_automation_tests >build/compile.log 2>&1
status=$?; echo "$status" >build/compile.exit; exit "$status"

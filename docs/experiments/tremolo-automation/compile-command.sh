#!/usr/bin/env bash
set -eu
source build/recovery-env.sh
SDK=../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK
g++ -std=c++20 -O2 -Isrc -I"$SDK" tests/Vst3TremoloAutomationTests.cpp "$SDK/pluginterfaces/base/coreiids.cpp" "$SDK/pluginterfaces/base/funknown.cpp" "$SDK/public.sdk/source/vst/vstinitiids.cpp" -ldl -o build/tremolo-host

# From repository root; official JUCE 8.0.11 c352e2489000ffeb9e397051377d75eef2efe641.
SDK=../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK
g++ -std=c++20 -O2 -Isrc -I"$SDK" \
  tests/Vst3SubTouchCombinedAutomationTests.cpp \
  "$SDK/pluginterfaces/base/coreiids.cpp" \
  "$SDK/pluginterfaces/base/funknown.cpp" \
  "$SDK/public.sdk/source/vst/vstinitiids.cpp" \
  -ldl -o build/cs80_vst3_sub_touch_combined_standalone

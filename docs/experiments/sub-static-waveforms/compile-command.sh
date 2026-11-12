# Run from repository root. SDK is official JUCE 8.0.11, commit
# c352e2489000ffeb9e397051377d75eef2efe641, fetched in sibling build.
SDK=../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK
g++ -std=c++20 -O2 -Isrc -I"$SDK" \
  tests/Vst3SubWaveformAutomationTests.cpp \
  "$SDK/pluginterfaces/base/coreiids.cpp" \
  "$SDK/pluginterfaces/base/funknown.cpp" \
  "$SDK/public.sdk/source/vst/vstinitiids.cpp" \
  -ldl -o build/cs80_vst3_sub_waveform_standalone

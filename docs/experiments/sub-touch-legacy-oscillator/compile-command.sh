# Successful final compilation, from this repository root; no runtime invocation.
SDK=../yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK
for variant in original legacy; do
  flags=
  test "$variant" != legacy || flags=-DCS80_SUB_TOUCH_LEGACY_OSCILLATOR=1
  g++ -std=c++20 -O2 $flags -Isrc -I"$SDK" \
    tests/Vst3SubTouchAutomationTests.cpp \
    "$SDK/pluginterfaces/base/coreiids.cpp" \
    "$SDK/pluginterfaces/base/funknown.cpp" \
    "$SDK/public.sdk/source/vst/vstinitiids.cpp" \
    -ldl -o "build/cs80_vst3_sub_touch_${variant}_standalone" || exit $?
done

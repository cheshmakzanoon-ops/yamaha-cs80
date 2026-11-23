#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED 1
#define JUCE_STANDALONE_APPLICATION 1
#include <juce_core/system/juce_TargetPlatform.h>
#include <juce_core/system/juce_StandardHeader.h>
#include <juce_core/maths/juce_MathsFunctions.h>
#include <juce_core/maths/juce_Range.h>
#include <juce_core/maths/juce_NormalisableRange.h>
#include "core/FactoryPresets.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
int main()
{
    const auto& preset = cs80::programLibrary()[5];
    const auto& d = cs80::paramDef(cs80::P_kbdLevelHigh);
    const juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
    const float authored = preset.values[cs80::P_kbdLevelHigh];
    const float normalized = range.convertTo0to1(range.snapToLegalValue(authored));
    const float canonical = range.snapToLegalValue(range.convertFrom0to1(normalized));
    const float apvts = range.snapToLegalValue(range.convertFrom0to1(range.convertTo0to1(range.snapToLegalValue(canonical))));
    uint32_t a, b, c;
    std::memcpy(&a, &authored, sizeof a);
    std::memcpy(&b, &canonical, sizeof b);
    std::memcpy(&c, &apvts, sizeof c);
    std::printf("program=%s\nauthored=%.12g bits=%08x\nnormalized=%.12g\ncanonical=%.12g bits=%08x\napvts=%.12g bits=%08x\nerror=%.12g\nold_exact_assertion=%s\ncanonical_exact_assertion=%s\ninitial_0.5_rejected=%s\nassigners=%g,%g\n", preset.name.c_str(), authored, a, normalized, canonical, b, apvts, c, apvts-authored, apvts==authored?"PASS":"FAIL", apvts==canonical?"PASS":"FAIL", 0.5f!=canonical?"PASS":"FAIL", preset.values[cs80::P_sustainAssign],preset.values[cs80::P_glideAssign]);
    return apvts!=authored && apvts==canonical && 0.5f!=canonical ? 0:1;
}

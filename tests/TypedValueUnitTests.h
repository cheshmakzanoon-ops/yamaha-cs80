#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/UiComponents.h"

#include <bit>
#include <cstdint>
#include <cstdio>

namespace cs80::tests
{
template <class Check>
void testTypedValueUnits(Check check)
{
    constexpr int cutoff = cs80::chParam(0, cs80::CP_lpfCutoff);
    constexpr int attack = cs80::chParam(0, cs80::CP_vA);
    struct Valid { int parameter; const char* text; float current, expected; };
    const Valid valid[] {
        { cutoff, "1.2k", 2000.0f, 1200.0f }, { cutoff, "3.6 kHz", 2000.0f, 3600.0f },
        { cutoff, "  1200 HZ  ", 2000.0f, 1200.0f }, { cutoff, "1,2 kHz", 2000.0f, 1200.0f },
        { attack, "80 ms", 0.3f, 0.08f }, { attack, "80 msec", 0.3f, 0.08f },
        { attack, "80 msecs", 0.3f, 0.08f }, { attack, "0.8 s", 0.3f, 0.8f },
        { attack, "0.8 sec", 0.3f, 0.8f }, { attack, "0.8 secs", 0.3f, 0.8f },
        { attack, "0.8 second", 0.3f, 0.8f }, { attack, "0.8 seconds", 0.3f, 0.8f },
        { attack, "80", 0.05f, 0.08f }, { cs80::P_volume, "52 %", 0.7f, 0.52f },
        { cs80::P_masterTune, "-0.5 st", 0.0f, -0.5f },
        { cs80::P_masterTune, "-0.5 semi", 0.0f, -0.5f },
        { cs80::P_masterTune, "-0.5 semitone", 0.0f, -0.5f },
        { cs80::P_masterTune, "-0.5 semitones", 0.0f, -0.5f },
        { cs80::P_detune, "5 ct", 0.0f, 5.0f }, { cs80::P_detune, "5 cts", 0.0f, 5.0f },
        { cs80::P_detune, "5 cent", 0.0f, 5.0f }, { cs80::P_detune, "5 cents", 0.0f, 5.0f },
        { cs80::P_brilliance, "0.5 oct", 0.0f, 0.5f }, { cs80::P_brilliance, "0.5 o", 0.0f, 0.5f },
        { cs80::P_brilliance, "0.5 octave", 0.0f, 0.5f }, { cs80::P_brilliance, "0.5 octaves", 0.0f, 0.5f },
        { cs80::P_mix, "68/32", 0.5f, 0.68f },
        { cs80::P_mix, "0/100", .5f, 0.0f }, { cs80::P_mix, "100/0", .5f, 1.0f },
        { cs80::P_mix, " 68.5 / 31.5 ", .5f, .685f }, { cs80::P_mix, "68,5/31,5", .5f, .685f },
        { cs80::P_mix, "68", .5f, .68f }, { cs80::P_glideMode, "GLISS", 0.0f, 2.0f },
        { cs80::P_resonance, "5", 0.2f, 0.5f }, { cs80::P_tuningRef, "9999", 440.0f, 465.0f }
    };
    for (const auto& test : valid)
    {
        float parsed = -12345.0f;
        check(cs80::ui::parseParamValue(test.parameter, test.text, test.current, parsed)
                  && std::abs(parsed - test.expected) <= 0.0001f * juce::jmax(1.0f, std::abs(test.expected)),
              "documented units, complete aliases and existing display laws remain accepted");
    }

    struct Invalid { int parameter; const char* text; };
    const Invalid invalid[] {
        { cs80::P_mix, "68/garbage" }, { cs80::P_mix, "68/" }, { cs80::P_mix, "68//32" },
        { cs80::P_mix, "68/32 garbage" }, { cs80::P_mix, "68/32/0" },
        { cs80::P_mix, "68/31" }, { cs80::P_mix, "25/25" }, { cs80::P_mix, "0/0" },
        { cs80::P_mix, "-1/101" }, { cs80::P_mix, "101/-1" }, { cs80::P_mix, "68/-32" },
        { cs80::P_mix, "68/NaN" }, { cs80::P_mix, "68/Inf" }, { cs80::P_mix, "68/3.2e1" },
        { cs80::P_mix, "68/32%" }, { cs80::P_mix, "68/3..2" },
        { cutoff, "3 kittens" }, { cutoff, "3 kHz garbage" }, { cutoff, "1200 HzHz" },
        { cutoff, "3 kHz/ms" }, { cutoff, "3 k hz" }, { cutoff, "3 k.." },
        { attack, "80 ms garbage" }, { attack, "80 msecjunk" }, { attack, "0.8 seconds later" },
        { cs80::P_volume, "52 % garbage" }, { cs80::P_masterTune, "-0.5 staccato" },
        { cs80::P_masterTune, "-0.5 semitone junk" }, { cs80::P_detune, "5 ctfoo" },
        { cs80::P_detune, "5 centauri" }, { cs80::P_brilliance, "0.5 octopus" },
        { cs80::P_brilliance, "0.5 octave junk" }, { cutoff, "80 ms" }, { attack, "1200 Hz" }
    };
    for (const auto& test : invalid)
    {
        float parsed = -12345.0f;
        check(!cs80::ui::parseParamValue(test.parameter, test.text, cs80::paramDef(test.parameter).def, parsed)
                  && parsed == -12345.0f,
              "an unknown/trailing unit token is rejected without changing the output argument");
    }

    // Record every formatter/parser result at five points per parameter. The
    // pre-fix digest is a compatibility witness, not a cryptographic proof or
    // a claim that all existing formatter strings are parseable/round-trip exact.
    std::uint64_t digest = 14695981039346656037ull;
    int cases = 0, accepted = 0;
    bool boundedResults = true;
    auto append = [&](std::uint32_t word) {
        for (int shift = 0; shift < 32; shift += 8)
        {
            digest ^= (word >> shift) & 0xffu;
            digest *= 1099511628211ull;
        }
    };
    for (int parameter = 0; parameter < cs80::P_numParams; ++parameter)
        for (float fraction : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto& definition = cs80::paramDef(parameter);
            const float current = definition.min + fraction * (definition.max - definition.min);
            float parsed = -12345.0f;
            const bool ok = cs80::ui::parseParamValue(parameter, cs80::ui::formatParamValue(parameter, current), current, parsed);
            boundedResults &= ok ? std::isfinite(parsed) && parsed >= definition.min && parsed <= definition.max
                                 : parsed == -12345.0f;
            append(ok ? 1u : 0u);
            append(std::bit_cast<std::uint32_t>(parsed));
            ++cases;
            accepted += ok ? 1 : 0;
        }
    check(boundedResults, "formatter/parser samples stay finite and bounded or reject without mutation");
    std::printf("Typed-value formatter compatibility: %d cases, %d accepted, digest %016llx\n",
                cases, accepted, (unsigned long long) digest);

    struct Probe final : juce::AudioProcessorParameter::Listener
    {
        int begins = 0, ends = 0, changes = 0;
        void parameterValueChanged(int, float) override { ++changes; }
        void parameterGestureChanged(int, bool starting) override { starting ? ++begins : ++ends; }
    } probe;
    juce::TemporaryFile fixture("-cs80-typed-unit");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setParamValue(cutoff, 3600.0f);
    cs80::ui::ParamKnob knob(*host, cutoff);
    auto* parameter = host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cutoff));
    parameter->addListener(&probe);
    const auto before = host->getParamValue(cutoff);
    check(!knob.applyTypedValue("2 kittens"), "malformed typed units do not report a successful edit");
    check(host->getParamValue(cutoff) == before && probe.begins == 0 && probe.ends == 0 && probe.changes == 0,
          "rejecting a malformed typed edit does not mutate the parameter or notify a host gesture");
    host->setParamValue(cutoff, 3600.0f);
    probe = {};
    check(knob.applyTypedValue("2 kHz") && std::abs(host->getParamValue(cutoff) - 2000.0f) < 1.0f,
          "a valid edit still reaches the real attached parameter");
    check(probe.begins == 1 && probe.ends == 1 && probe.changes == 1,
          "a valid edit remains exactly one balanced host gesture and value change");
    parameter->removeListener(&probe);
    cs80::ui::ParamKnob mixKnob(*host,cs80::P_mix);
    auto* mixParameter=host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_mix));
    host->setParamValue(cs80::P_mix,.4f); probe={}; mixParameter->addListener(&probe);
    check(!mixKnob.applyTypedValue("68/garbage") && host->getParamValue(cs80::P_mix)==.4f
              && probe.begins==0 && probe.ends==0 && probe.changes==0,
          "malformed MIX pair neither edits audio state nor creates a host gesture");
    host->setParamValue(cs80::P_mix,.4f); probe={}; check(mixKnob.applyTypedValue("68/32") && std::abs(host->getParamValue(cs80::P_mix)-.68f)<1.e-6f
              && probe.begins==1 && probe.ends==1 && probe.changes==1,
          "valid MIX pair remains one balanced host edit");
    mixParameter->removeListener(&probe);
}
} // namespace cs80::tests

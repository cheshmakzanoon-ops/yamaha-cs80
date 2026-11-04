#pragma once

#include "HostParameterState.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>
#include <utility>

namespace cs80::tests
{
// Existing host smoke invokes this after JUCE initialization. Each fixture owns
// its processor/temp file; no user preset directory or persistent default edits.
template <typename Check>
void testSourcePerformanceHost(Check&& check)
{
    Cs80AudioProcessor host;
    auto* source = host.getAPVTS().getParameter("source_performance");
    auto* isolation = host.getAPVTS().getParameter("parity_mode");
    check(source != nullptr && host.getParameters().size() >= 245
              && host.getParameters()[244] == source && host.getParameters()[243] == isolation,
          "source performance appends host244 without moving parity243");
    // Plan 1.7 engine default flip (intentional): fresh instances and the host
    // default select the circuit engine (ENGINE on). The core ParamDef default and
    // every old-state rule below are unchanged.
    check(source != nullptr && dynamic_cast<juce::AudioParameterBool*>(source) != nullptr
              && source->getVersionHint() == 7 && source->getDefaultValue() == 1.0f
              && host.getParamValue(P_sourcePerformance) == 1.0f,
          "ENGINE (source performance) is a canonical Bool with AU hint7, Circuit by default");
    auto restore = [&](const juce::ValueTree& tree)
    {
        const auto text = tree.createXml()->toString();
        host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
    };
    const auto original = cs80::tests::copyHostParameterState(host);
    {
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        const auto xml = juce::parseXML(saved.toString());
        check(xml != nullptr && xml->getIntAttribute(Cs80AudioProcessor::stateSchemaKey)
                                    == Cs80AudioProcessor::stateSchemaVersion
                  && ! original.hasProperty(Cs80AudioProcessor::stateSchemaKey),
              "host state writes the schema marker; the live state does not keep one");
    }
    for (float enabled : {0.0f, 1.0f})
    {
        host.setParamValue(P_sourcePerformance, enabled);
        host.setParamValue(P_parityMode, 0);
        host.selectHardwareSource(0, 1);
        host.setParamValue(P_macro1, .87f);
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        host.setParamValue(P_sourcePerformance, 1 - enabled);
        host.setStateInformation(saved.getData(), (int) saved.getSize());
        check(host.getParamValue(P_sourcePerformance) == enabled
                  && host.getHardwareSource(0) == 1
                  && std::abs(host.getParamValue(P_macro1) - .87f) < 1e-6f,
              "session state restores model choice, tone identity and stored overlays");
        auto userState = cs80::tests::copyHostParameterState(host);
        userState.getChildWithProperty("id", "parity_mode").setProperty("value", 0, nullptr);
        host.setParamValue(P_parityMode, 1);
        host.setParamValue(P_sourcePerformance, 1 - enabled);
        juce::TemporaryFile preset(".cs80xml");
        check(userState.createXml()->writeTo(preset.getFile()), "source-performance temporary user state written");
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_sourcePerformance) == enabled && host.getParamValue(P_parityMode) == 1,
              "user preset recalls model choice while preserving independent isolation policy");
        host.loadFactoryPreset(22);
        check(host.getParamValue(P_sourcePerformance) == 0 && host.getParamValue(P_parityMode) == 1
                  && host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13,
              "unchanged factory preset recalls legacy model/PANEL without weakening isolation");
    }
    host.setParamValue(P_parityMode, 0);
    host.prepareToPlay(48000, 64);
    juce::AudioBuffer<float> stateBlock(2, 0);
    juce::MidiBuffer stateMidi;
    auto activateSource = [&]
    {
        host.setParamValue(P_sourcePerformance, 1);
        host.selectHardwareSource(0, 1);
        host.processBlock(stateBlock, stateMidi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance
                  && source_factory::isSourceDriven(host.getEngine().getSourceToneInfo(0).route),
              "migration fixture starts with an accepted compiled source adapter");
    };
    auto checkAcceptedLegacy = [&]
    {
        host.processBlock(stateBlock, stateMidi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::legacy
                  && !source_factory::isSourceDriven(host.getEngine().getSourceToneInfo(0).route),
              "restored legacy model clears the accepted source adapter at the next callback");
    };
    auto missing = original.createCopy();
    missing.removeChild(missing.getChildWithProperty("id", "source_performance"), nullptr);
    // An old preset may retain the same tab selector, but it means the legacy
    // bridge, not permission to inherit the live instance's compiled model.
    missing.getChildWithProperty("id", "hardware_source_1").setProperty("value", 1, nullptr);
    activateSource();
    restore(missing);
    check(host.getParamValue(P_sourcePerformance) == 0,
          "old session without model flag clears live source opt-in instead of inheriting it");
    checkAcceptedLegacy();
    {
        juce::TemporaryFile preset(".cs80xml");
        check(missing.createXml()->writeTo(preset.getFile()), "legacy temporary user state written");
        activateSource();
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_sourcePerformance) == 0,
              "old user preset without model flag restores its legacy sound policy");
        checkAcceptedLegacy();
    }
    for (const auto* malformed : {"nan", "inf", "-1", "2", "0.5", "garbage", "1oops", ""})
    {
        auto bad = original.createCopy();
        bad.getChildWithProperty("id", "source_performance").setProperty("value", malformed, nullptr);
        activateSource();
        restore(bad);
        check(host.getParamValue(P_sourcePerformance) == 0,
              "malformed/fractional source model opt-in restores off");
        checkAcceptedLegacy();
    }
    auto missingValue = original.createCopy();
    missingValue.getChildWithProperty("id", "source_performance").removeProperty("value", nullptr);
    activateSource();
    restore(missingValue);
    check(host.getParamValue(P_sourcePerformance) == 0, "missing source mode value restores off");
    checkAcceptedLegacy();

    auto setPolicy = [](Cs80AudioProcessor& processor, RenderPolicy policy)
    {
        processor.setParamValue(P_sourcePerformance, policy == RenderPolicy::legacy ? 0.0f : 1.0f);
        processor.setParamValue(P_parityMode, policy == RenderPolicy::isolation ? 1.0f : 0.0f);
    };
    const std::array<std::pair<RenderPolicy, RenderPolicy>, 4> transitions {{
        { RenderPolicy::legacy, RenderPolicy::sourcePerformance },
        { RenderPolicy::sourcePerformance, RenderPolicy::legacy },
        { RenderPolicy::sourcePerformance, RenderPolicy::isolation },
        { RenderPolicy::isolation, RenderPolicy::sourcePerformance }
    }};
    for (const auto& [before, after] : transitions)
    {
        Cs80AudioProcessor processor;
        setPolicy(processor, before);
        processor.setExpressionFromUI(.35f);
        processor.prepareToPlay(48000, 64);
        processor.serviceKeyboardUI();
        juce::AudioBuffer<float> block(2, 64);
        auto process = [&](juce::MidiBuffer& midi) { processor.processBlock(block, midi); };
        auto tick = [&] { juce::MidiBuffer midi; process(midi); };
        auto held = [&]
        {
            int count = 0;
            for (int voice = 0; voice < 8; ++voice)
            {
                const auto info = processor.getEngine().getVoiceInfo(voice);
                if (info.active && info.held && info.note == 60 && info.channel == 1) ++count;
            }
            return count;
        };
        processor.getKeyboardState().noteOn(1, 60, .8f);
        tick();
        check(held() == 1, "source transition fixture has accepted UI-held note");
        setPolicy(processor, after);
        juce::MidiBuffer replacement;
        replacement.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
        process(replacement);
        check(processor.getEngine().getRenderPolicy() == after && held() == 1,
              "effective policy transition clears old UI voice and accepts fresh host strike");
        processor.getKeyboardState().noteOff(1, 60, 0);
        tick();
        check(held() == 1 && processor.isHostKeyDown(60),
              "stale UI release after model reset cannot release new host note with identical identity");
        check(std::abs(processor.getExpressionValue() - .35f) < 1e-6f,
              "policy reset retains UI/MIDI expression mailbox");
        if (usesSourceModels(after))
        {
            const double expected = std::exp(std::log(1000000.0) + .35f * (std::log(2000.0) - std::log(1000000.0)));
            check(std::abs(processor.getEngine().getPraWahState(0).targetOhms - expected) < 1e-6,
                  "policy transition reapplies expression to source PRA after engine reset");
        }
        processor.releaseResources();
        processor.prepareToPlay(48000, 64);
        processor.serviceKeyboardUI();
        tick();
        check(held() == 0 && processor.getEngine().getRenderPolicy() == after,
              "prepare/release retains chosen policy but clears held voices");
        if (usesSourceModels(after))
            check(processor.getEngine().getPraWahState(0).targetOhms > 100000.0,
                  "prepareToPlay reapplies retained non-full expression in source policy");
    }
    // The new flag is retained but inert while the existing isolation policy wins.
    {
        Cs80AudioProcessor processor;
        setPolicy(processor, RenderPolicy::isolation);
        processor.prepareToPlay(48000, 64);
        processor.serviceKeyboardUI();
        juce::AudioBuffer<float> block(2, 64), empty(2, 0);
        juce::MidiBuffer midi;
        processor.getKeyboardState().noteOn(1, 60, .8f);
        processor.processBlock(block, midi);
        const auto before = processor.getEngine().getRankEnvelopeState(0, 0).amplitude;
        processor.setParamValue(P_sourcePerformance, 0);
        processor.processBlock(empty, midi);
        const auto after = processor.getEngine().getRankEnvelopeState(0, 0).amplitude;
        check(processor.getEngine().getRenderPolicy() == RenderPolicy::isolation
                  && processor.getEngine().getVoiceInfo(0).held && before.value == after.value
                  && before.stage == after.stage && before.elapsed == after.elapsed,
              "masked source flag change does not reset isolation notes or precise envelope state");
        processor.getKeyboardState().noteOff(1, 60, 0);
        processor.processBlock(empty, midi);
        check(!processor.getEngine().getVoiceInfo(0).held,
              "masked flag change retains UI ownership so its real release still works");
    }
}
} // namespace cs80::tests

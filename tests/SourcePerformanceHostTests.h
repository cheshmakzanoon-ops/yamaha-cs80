#pragma once

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
    // The historical Bool slot remains addressable for old sessions, but its
    // value is now fixed. The core diagnostic ParamDef default is independent.
    check(source != nullptr && isolation != nullptr && dynamic_cast<juce::AudioParameterBool*>(source) != nullptr
              && source->getVersionHint() == 7 && source->getDefaultValue() == 1.0f
              && !source->isAutomatable() && !isolation->isAutomatable()
              && host.getParamValue(P_sourcePerformance) == 1.0f,
          "fixed Circuit Bool preserves AU hint7 while retired engine slots reject automation");
    auto restore = [&](const juce::ValueTree& tree)
    {
        const auto text = tree.createXml()->toString();
        host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
    };
    const auto original = host.getAPVTS().copyState();
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
        check(host.getParamValue(P_sourcePerformance) == 1.0f && host.getParamValue(P_parityMode) == 0.0f
                  && host.getHardwareSource(0) == 1
                  && std::abs(host.getParamValue(P_macro1) - .87f) < 1e-6f,
              "session state restores tone identity and overlays on the fixed circuit engine");
        auto userState = host.getAPVTS().copyState();
        userState.getChildWithProperty("id", "parity_mode").setProperty("value", 1, nullptr);
        userState.getChildWithProperty("id", "source_performance").setProperty("value", enabled, nullptr);
        host.setParamValue(P_parityMode, 1);
        host.setParamValue(P_sourcePerformance, 1 - enabled);
        juce::TemporaryFile preset(".cs80xml");
        check(userState.createXml()->writeTo(preset.getFile()), "source-performance temporary user state written");
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_sourcePerformance) == 1.0f && host.getParamValue(P_parityMode) == 0.0f
                  && host.getHardwareSource(0) == 1
                  && std::abs(host.getParamValue(P_macro1) - .87f) < 1e-6f,
              "old user mode requests normalize without losing the selected tone or stored overlay");
        host.loadFactoryPreset(22);
        check(host.getParamValue(P_sourcePerformance) == 1 && host.getParamValue(P_parityMode) == 0
                  && host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13,
              "original factory address recalls its revoiced PANEL sound on the circuit engine");
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
    auto checkAcceptedCircuit = [&](int expectedSelector)
    {
        host.processBlock(stateBlock, stateMidi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance
                  && host.getHardwareSource(0) == expectedSelector
                  && source_factory::isSourceDriven(host.getEngine().getSourceToneInfo(0).route)
                         == (expectedSelector != 13),
              "restored state replaces the source adapter from its own selector on the circuit engine");
    };
    auto missing = original.createCopy();
    missing.removeChild(missing.getChildWithProperty("id", "source_performance"), nullptr);
    // A missing retired policy field does not erase the stored hardware tone.
    // PANEL fixtures below must also clear the previous compiled source adapter.
    missing.getChildWithProperty("id", "hardware_source_1").setProperty("value", 1, nullptr);
    activateSource();
    restore(missing);
    check(host.getParamValue(P_sourcePerformance) == 1 && host.getParamValue(P_parityMode) == 0,
          "old session without model flag normalizes to the fixed circuit engine");
    checkAcceptedCircuit(1);
    {
        juce::TemporaryFile preset(".cs80xml");
        check(missing.createXml()->writeTo(preset.getFile()), "old-format temporary user state written");
        activateSource();
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_sourcePerformance) == 1 && host.getParamValue(P_parityMode) == 0,
              "old user preset without model flag normalizes to the fixed circuit engine");
        checkAcceptedCircuit(1);
    }
    for (const auto* malformed : {"nan", "inf", "-1", "2", "0.5", "garbage", "1oops", ""})
    {
        auto bad = original.createCopy();
        bad.getChildWithProperty("id", "source_performance").setProperty("value", malformed, nullptr);
        activateSource();
        restore(bad);
        check(host.getParamValue(P_sourcePerformance) == 1 && host.getParamValue(P_parityMode) == 0,
              "malformed/fractional model requests normalize to the fixed circuit engine");
        checkAcceptedCircuit(13);
    }
    auto missingValue = original.createCopy();
    missingValue.getChildWithProperty("id", "source_performance").removeProperty("value", nullptr);
    activateSource();
    restore(missingValue);
    check(host.getParamValue(P_sourcePerformance) == 1 && host.getParamValue(P_parityMode) == 0,
          "missing source mode value normalizes to the fixed circuit engine");
    checkAcceptedCircuit(13);

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
        check(held() == 1, "retired policy fixture has accepted UI-held note");
        const auto beforeEnvelope = processor.getEngine().getRankEnvelopeState(0, 0).amplitude;
        setPolicy(processor, after);
        juce::AudioBuffer<float> empty(2, 0);
        juce::MidiBuffer emptyMidi;
        processor.processBlock(empty, emptyMidi);
        const auto afterEnvelope = processor.getEngine().getRankEnvelopeState(0, 0).amplitude;
        check(processor.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance && held() == 1
                  && beforeEnvelope.value == afterEnvelope.value
                  && beforeEnvelope.stage == afterEnvelope.stage && beforeEnvelope.elapsed == afterEnvelope.elapsed,
              "obsolete mode writes retain the UI voice and exact envelope state");
        juce::MidiBuffer replacement;
        replacement.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
        process(replacement);
        check(processor.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance && held() == 2,
              "same-pitch host takeover adds its strike without a policy-induced voice reset");
        processor.getKeyboardState().noteOff(1, 60, 0);
        tick();
        check(held() == 2 && processor.isHostKeyDown(60),
              "stale UI release cannot release either circuit after host takeover");
        check(std::abs(processor.getExpressionValue() - .35f) < 1e-6f,
              "retired policy writes retain the UI/MIDI expression mailbox");
        {
            const double expected = std::exp(std::log(1000000.0) + .35f * (std::log(2000.0) - std::log(1000000.0)));
            check(std::abs(processor.getEngine().getPraWahState(0).targetOhms - expected) < 1e-6,
                  "retired policy writes preserve the expression target of the circuit PRA");
        }
        processor.releaseResources();
        processor.prepareToPlay(48000, 64);
        processor.serviceKeyboardUI();
        tick();
        check(held() == 0 && processor.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "prepare/release clears held voices while retaining the circuit engine");
        check(processor.getEngine().getPraWahState(0).targetOhms > 100000.0,
              "prepareToPlay reapplies retained non-full expression to the circuit engine");
    }
    // Even contradictory retired flags leave ownership and envelope history intact.
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
        check(processor.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance
                  && processor.getEngine().getVoiceInfo(0).held && before.value == after.value
                  && before.stage == after.stage && before.elapsed == after.elapsed,
              "retired source flag write preserves circuit notes and precise envelope state");
        processor.getKeyboardState().noteOff(1, 60, 0);
        processor.processBlock(empty, midi);
        check(!processor.getEngine().getVoiceInfo(0).held,
              "retired flag write retains UI ownership so its real release still works");
    }
}
} // namespace cs80::tests

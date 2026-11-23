#pragma once
// ws2-engine host contracts: oversampling quality, latency and state. Invoked by
// the UI self-test after JUCE initialization; each fixture owns its processor.

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>

namespace cs80::tests
{
// Sixteen-voice mode keeps the physical expression pedal on both engines
// across real resource resets and inert retired-policy writes, before fresh MIDI.
template <typename Check>
void testExtensionExpressionLifecycleHost(Check&& check)
{
    enum class Change { qualityUp, qualityDown, offlineOn, offlineOff,
                        retiredSourceOff, retiredSourceOn, retiredIsolationOn, retiredIsolationOff, offlineOffQualityUp, prepare, releasePrepare };
    constexpr const char* changeNames[] = { "quality up", "quality down", "offline on", "offline off",
        "retired source off", "retired source on", "retired isolation on", "retired isolation off",
        "offline off + quality up", "prepare", "release + prepare" };
    for (const auto change : { Change::qualityUp, Change::qualityDown, Change::offlineOn,
                               Change::offlineOff, Change::retiredSourceOff, Change::retiredSourceOn,
                               Change::retiredIsolationOn, Change::retiredIsolationOff,
                               Change::offlineOffQualityUp, Change::prepare, Change::releasePrepare })
        for (const bool midiPedal : { false, true })
            for (const float requestedPedal : { 0.0f, 0.35f, 1.0f })
            {
                const auto verify = [&](bool ok, const char* message)
                {
                    const auto context = juce::String(changeNames[static_cast<int>(change)])
                        + (midiPedal ? " / CC11 / " : " / UI pedal / ") + juce::String(requestedPedal)
                        + ": " + message;
                    check(ok, context.toRawUTF8());
                };
                Cs80AudioProcessor host;
                host.setParamValue(P_voiceMode, 1.0f);
                host.setParamValue(P_sourcePerformance, change == Change::retiredSourceOn ? 0.0f : 1.0f);
                host.setParamValue(P_oversampling, change == Change::qualityDown ? 2.0f : 0.0f);
                host.setParamValue(P_analogNoise, 0.0f);
                host.setParamValue(P_drift, 0.0f);
                host.setParamValue(P_voiceDispersion, 0.0f);
                host.setNonRealtime(change == Change::offlineOff || change == Change::offlineOffQualityUp);
                host.prepareToPlay(48000.0, 64);
                juce::AudioBuffer<float> block(2, 512);
                juce::MidiBuffer midi;
                float pedal = requestedPedal;
                if (midiPedal)
                {
                    const int cc = juce::roundToInt(pedal * 127.0f);
                    pedal = cc / 127.0f;
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, 11, cc), 0);
                }
                else host.setExpressionFromUI(pedal);
                for (int note = 48; note < 57; ++note)
                    midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                host.processBlock(block, midi);
                midi.clear();
                verify(host.getEngine().activeVoices() == 8 && host.getExtensionEngine().activeVoices() == 1,
                      "expression lifecycle fixture activates both engines and consumes the pedal event");

                switch (change)
                {
                    case Change::qualityUp: host.setParamValue(P_oversampling, 2.0f); break;
                    case Change::qualityDown: host.setParamValue(P_oversampling, 0.0f); break;
                    case Change::offlineOn: host.setNonRealtime(true); break;
                    case Change::offlineOff: host.setNonRealtime(false); break;
                    case Change::retiredSourceOff: host.setParamValue(P_sourcePerformance, 0.0f); break;
                    case Change::retiredSourceOn: host.setParamValue(P_sourcePerformance, 1.0f); break;
                    case Change::retiredIsolationOn: host.setParamValue(P_parityMode, 1.0f); break;
                    case Change::retiredIsolationOff: host.setParamValue(P_parityMode, 0.0f); break;
                    case Change::offlineOffQualityUp:
                        host.setNonRealtime(false); host.setParamValue(P_oversampling, 2.0f); break;
                    case Change::prepare: host.prepareToPlay(48000.0, 64); break;
                    case Change::releasePrepare:
                        host.releaseResources(); host.prepareToPlay(48000.0, 64); break;
                }
                const bool retiredWrite = change == Change::retiredSourceOff || change == Change::retiredSourceOn
                    || change == Change::retiredIsolationOn || change == Change::retiredIsolationOff;
                if (retiredWrite)
                {
                    // Obsolete automation is not a resource transition. Both
                    // physical instruments must keep their original notes.
                    host.processBlock(block, midi);
                    bool retained = host.getEngine().activeVoices() == 8
                        && host.getExtensionEngine().activeVoices() == 1;
                    for (int voice = 0; voice < 8; ++voice)
                        retained = retained && host.getEngine().getVoiceInfo(voice).held
                            && host.getEngine().getVoiceInfo(voice).note == 48 + voice;
                    retained = retained && host.getExtensionEngine().getVoiceInfo(0).held
                        && host.getExtensionEngine().getVoiceInfo(0).note == 56;
                    verify(retained, "retired policy writes preserve the notes on both physical instruments");
                }
                // Fresh notes share a real resource-transition block, or play
                // alongside the preserved notes after an inert policy write.
                // No intervening pedal event is available to repair lost state.
                for (int note = 60; note < (retiredWrite ? 67 : 69); ++note)
                    midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
                host.processBlock(block, midi);
                midi.clear();
                const auto& main = host.getEngine();
                const auto& extension = host.getExtensionEngine();
                if (retiredWrite)
                {
                    int freshHeld = 0, originalHeld = 0;
                    for (const auto* engine : { &main, &extension })
                        for (int voice = 0; voice < 8; ++voice)
                        {
                            const auto info = engine->getVoiceInfo(voice);
                            if (info.held && info.note >= 60 && info.note < 67) ++freshHeld;
                            if (info.held && info.note >= 48 && info.note < 57) ++originalHeld;
                        }
                    verify(main.activeVoices() == 8 && extension.activeVoices() == 8
                              && freshHeld == 7 && originalHeld == 9,
                          "seven fresh strikes fill the unused voices without replacing the nine preserved notes");
                }
                else
                    verify(main.activeVoices() == 8 && extension.activeVoices() == 1
                              && main.getVoiceInfo(0).note == 60 && extension.getVoiceInfo(0).note == 68,
                          "resource transition clears old voices and accepts fresh MIDI on both engines");
                verify(main.getRenderPolicy() == RenderPolicy::sourcePerformance
                          && main.getRenderPolicy() == extension.getRenderPolicy()
                          && main.getOversamplingFactor() == extension.getOversamplingFactor(),
                      "both engines agree on the resulting render policy and sample rate");
                verify(host.getExpressionValue() == pedal,
                      "lifecycle transition retains the physical pedal mailbox");
                if (usesSourceModels(main.getRenderPolicy()))
                {
                    const double target = std::exp(std::log(1000000.0)
                        + pedal * (std::log(2000.0) - std::log(1000000.0)));
                    for (const auto* engine : { &main, &extension })
                        verify(std::abs(engine->getPraWahState(0).targetOhms - target) < 1e-6,
                              "both engines retain the correct expression optical target through the lifecycle change");
                }
                const float peak = std::max(block.getMagnitude(0, 0, block.getNumSamples()),
                                            block.getMagnitude(1, 0, block.getNumSamples()));
                verify(pedal == 0.0f ? peak == 0.0f : peak > 1e-5f,
                      "heel-down stays silent across all sixteen voices; nonzero expression remains audible");
            }

    // The two ordered updates can reset 8x -> 2x -> 8x. A matching final
    // factor does not make pre-transition keyboard gestures valid again.
    {
        Cs80AudioProcessor transition;
        transition.setParamValue(P_oversampling, 0.0f);
        transition.setNonRealtime(true);
        transition.prepareToPlay(48000.0, 64);
        transition.serviceKeyboardUI();
        transition.getKeyboardState().noteOn(1, 59, 0.8f);
        transition.setNonRealtime(false);
        transition.setParamValue(P_oversampling, 2.0f);
        juce::AudioBuffer<float> audio(2, 64);
        juce::MidiBuffer freshHost;
        freshHost.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        transition.processBlock(audio, freshHost);
        check(transition.getEngine().getOversamplingFactor() == 8
                  && transition.getEngine().activeVoices() == 1
                  && transition.getEngine().getVoiceInfo(0).note == 60,
              "intermediate rate resets revoke stale UI gestures but accept fresh host MIDI");
    }

    // A quality selector change which keeps 2x must not reset held notes or
    // defeat a new pedal gesture queued at that same block boundary.
    Cs80AudioProcessor host;
    host.setParamValue(P_voiceMode, 1.0f);
    host.setParamValue(P_oversampling, 0.0f);
    host.setExpressionFromUI(0.35f);
    host.prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> block(2, 64);
    juce::MidiBuffer midi;
    for (int note = 60; note < 69; ++note)
        midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8) 100), 0);
    host.processBlock(block, midi);
    midi.clear();
    host.setParamValue(P_oversampling, 1.0f);
    host.setExpressionFromUI(0.2f);
    host.processBlock(block, midi);
    check(host.getEngine().activeVoices() == 8 && host.getExtensionEngine().activeVoices() == 1,
          "same-factor quality changes leave both engines' held notes intact");
    check(host.getEngine().getPraWahState(0).targetOhms == host.getExtensionEngine().getPraWahState(0).targetOhms
              && host.getExpressionValue() == 0.2f,
          "a new pedal gesture reaches both engines without a lifecycle reset");
    host.setParamValue(P_oversampling, 2.0f);
    host.setExpressionFromUI(0.4f);
    host.processBlock(block, midi);
    check(host.getEngine().getPraWahState(0).targetOhms == host.getExtensionEngine().getPraWahState(0).targetOhms
              && host.getExpressionValue() == 0.4f,
          "a queued pedal gesture takes precedence after both engines reset");
}

template <typename Check>
void testOversamplingHost(Check&& check)
{
    testExtensionExpressionLifecycleHost(check);
    {
        Cs80AudioProcessor host;
        auto* quality = dynamic_cast<juce::AudioParameterChoice*>(
            host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_oversampling)));
        // Host 245 at this branch; workstreams integrated before ws2 may append ahead of it.
        check(quality != nullptr && quality->getVersionHint() == 9 && quality->choices.size() == 3
                  && quality->getIndex() == 1 && host.getParameters().indexOf(quality) >= 245,
              "oversampling quality is an appended choice, AU hint 9, default Standard");
        check(host.getParameters()[244] == host.getAPVTS().getParameter("source_performance"),
              "shipped host parameters keep their indexes before the new quality choice");
    }

    // Latency is re-reported after quality changes; obsolete source writes are inert.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (float mode : { 0.0f, 1.0f, 2.0f })
            for (float circuit : { 0.0f, 1.0f })
            {
                Cs80AudioProcessor host;
                host.setParamValue(P_sourcePerformance, circuit);
                host.setParamValue(P_parityMode, 0.0f);
                host.prepareToPlay(rate, 64);
                juce::AudioBuffer<float> block(2, 64);
                juce::MidiBuffer midi;
                host.processBlock(block, midi);
                host.setParamValue(P_oversampling, mode);
                host.processBlock(block, midi);
                const int expected = oversamplingFactor(oversamplingMode(mode), true, false, rate);
                check(host.getEngine().getOversamplingFactor() == expected,
                      "host quality choice uses the circuit factor rule despite retired source writes");
                // ws1 merge: plus the circuit oscillator's 8 internal samples (8/N).
                check(host.getLatencySamples() == host.getEngine().getLatencySamples()
                          && host.getLatencySamples() == 63 + 8 / expected,
                      "reported host latency equals the engine's in every mode");
            }

    // Offline bounce always renders the circuit engine at HQ, regardless of retired writes.
    for (float circuit : { 0.0f, 1.0f })
    {
        Cs80AudioProcessor host;
        host.setParamValue(P_sourcePerformance, circuit);
        host.setParamValue(P_parityMode, 0.0f);
        host.setParamValue(P_oversampling, 0.0f);
        host.setNonRealtime(true);
        host.prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> block(2, 64);
        juce::MidiBuffer midi;
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == 8,
              "offline prepare always selects circuit HQ");
        host.setNonRealtime(false);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == 2
                  && host.getLatencySamples() == 67,
              "realtime again without a new prepare restores the selected mode");
        host.setNonRealtime(true);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == 8
                  && host.getEngine().activeVoices() == 1,
              "offline switch inside processBlock precedes the block's MIDI");
    }

    // A session setting: state round trip, but presets neither set nor clear it.
    {
        Cs80AudioProcessor host;
        host.setParamValue(P_oversampling, 2.0f);
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        host.setParamValue(P_oversampling, 0.0f);
        host.setStateInformation(saved.getData(), (int) saved.getSize());
        check(host.getParamValue(P_oversampling) == 2.0f, "session state restores the quality mode");
        host.loadFactoryPreset(5);
        check(host.getParamValue(P_oversampling) == 2.0f, "factory preset recall keeps the quality mode");
        auto userState = host.getAPVTS().copyState();
        userState.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_oversampling))
            .setProperty("value", 0.0f, nullptr);
        juce::TemporaryFile preset(".cs80xml");
        check(userState.createXml()->writeTo(preset.getFile()), "quality temporary user preset written");
        host.loadUserPreset(preset.getFile());
        check(host.getParamValue(P_oversampling) == 2.0f, "user preset recall keeps the live quality mode");
        auto old = host.getAPVTS().copyState();
        old.removeChild(old.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_oversampling)), nullptr);
        const auto text = old.createXml()->toString();
        host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        check(host.getParamValue(P_oversampling) == 1.0f, "a state without the quality mode restores Standard");
    }
}
// One production circuit engine; archived core defaults remain diagnostic fixtures.
template <typename Check>
void testEngineDefaultHost(Check&& check)
{
    using Mode = Cs80AudioProcessor::EngineMode;
    check(ParamValues {}[P_sourcePerformance] == 0.0f && ParamValues {}[P_parityMode] == 0.0f,
          "core defaults stay Legacy, so default-ParamValues renders are unchanged");
    {
        Cs80AudioProcessor host;
        auto* engine = host.getAPVTS().getParameter("source_performance");
        auto* pure = host.getAPVTS().getParameter("parity_mode");
        check(engine != nullptr && pure != nullptr && engine->getName(64) == "Circuit engine (fixed)"
                  && pure->getName(64) == "Reserved engine policy"
                  && !engine->isAutomatable() && !pure->isAutomatable(),
              "retired host slots are fixed, clearly named and non-automatable");
        check(engine->getDefaultValue() == 1.0f && host.getEngineMode() == Mode::circuit
                  && host.getEffectiveEngineMode() == Mode::circuit && !host.isPure(),
              "fresh instance selects the circuit engine");
        check(engine->getText(1.0f, 64) == "Circuit" && engine->getText(0.0f, 64) == "Circuit"
                  && engine->getValueForText("Legacy (compat)") == 1.0f && engine->getValueForText("Circuit") == 1.0f,
              "retired engine value/text entry cannot select the removed playback path");
        host.prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> block(2, 64);
        juce::MidiBuffer midi;
        host.processBlock(block, midi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "a new instance renders the circuit engine");
        host.setPure(true);
        host.setEngineMode(Mode::legacy);
        host.processBlock(block, midi);
        check(!host.isPure() && host.getEngineMode() == Mode::circuit && host.getEffectiveEngineMode() == Mode::circuit
                  && host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "retired mode APIs leave the playable circuit engine unchanged");
        host.setPure(false);
        host.processBlock(block, midi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "clearing a retired isolation request keeps the same circuit engine");

        // INIT stays a neutral programming patch; startup is the Circuit showcase.
        host.setPure(true);
        host.loadFactoryPreset(5);
        check(host.getEngineMode() == Mode::circuit, "original factory addresses now use the same circuit engine");
        host.loadInitPatch();
        check(host.getEngineMode() == Mode::circuit && host.getParamValue(P_subOriginalMode) == 1.0f
                  && host.getPresetName().startsWith("Init Patch") && !host.isPure(),
              "Init Patch uses the circuit engine with active performance controls");
    }

    // Every older schema migrates to the same production policy, including
    // explicit obsolete requests and missing or malformed parameter children.
    for (int schema : { 0, 1, 2, 3, 4 })
        for (const char* stored : { "missing", "garbage", "0", "1" })
        {
            Cs80AudioProcessor host;
            auto state = host.getAPVTS().copyState();
            if (schema > 0) state.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
            auto child = state.getChildWithProperty("id", "source_performance");
            if (juce::String(stored) == "missing") state.removeChild(child, nullptr);
            else child.setProperty("value", juce::String(stored), nullptr);
            host.setEngineMode(schema >= 3 ? Mode::legacy : Mode::circuit); // live value must not leak
            const auto text = state.createXml()->toString();
            host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
            check(host.getParamValue(P_sourcePerformance) == 1.0f
                      && host.getParamValue(P_parityMode) == 0.0f,
                  "all prior schemas migrate obsolete model requests to the circuit engine");
        }
    {
        Cs80AudioProcessor host;
        host.setEngineMode(Mode::legacy);
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        const auto xml = juce::parseXML(saved.toString());
        check(xml != nullptr && xml->getIntAttribute(Cs80AudioProcessor::stateSchemaKey) == 4,
              "states are written with the circuit-only schema 4");
        Cs80AudioProcessor other;
        other.setStateInformation(saved.getData(), (int) saved.getSize());
        check(other.getEngineMode() == Mode::circuit && !other.isPure(),
              "a round trip cannot restore a removed engine choice");
    }
}
// Phase 3 listening output: an appended physical-setup choice the circuit
// engine honors; presets neither set nor clear it.
template <typename Check>
void testListeningOutputHost(Check&& check)
{
    Cs80AudioProcessor host;
    auto* quality = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_oversampling));
    auto* listening = dynamic_cast<juce::AudioParameterChoice*>(
        host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_outputJack)));
    check(listening != nullptr && listening->getVersionHint() == 9 && listening->choices.size() == 3
              && listening->getIndex() == 0
              && host.getParameters().indexOf(listening) == host.getParameters().indexOf(quality) + 1,
          "listening output is the choice after OVERSAMPLING, AU hint 9, default Left/Right");
    check(Cs80AudioProcessor::isPhysicalSetupParameter(P_outputJack), "listening output is physical setup");
    host.setParamValue(P_outputJack, 1.0f);
    host.prepareToPlay(48000.0, 64);
    juce::AudioBuffer<float> block(2, 64);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8) 100), 0);
    bool mono = true;
    for (int b = 0; b < 40; ++b)
    {
        host.processBlock(block, midi);
        midi.clear();
        for (int i = 0; i < 64; ++i) mono = mono && block.getSample(0, i) == block.getSample(1, i);
    }
    check(mono && block.getMagnitude(0, 0, 64) > 0.0f, "host GENERAL listening is mono and audible");
    host.loadFactoryPreset(5);
    check(host.getParamValue(P_outputJack) == 1.0f, "factory preset recall keeps the listening output");
    juce::MemoryBlock saved;
    host.getStateInformation(saved);
    host.setParamValue(P_outputJack, 2.0f);
    host.setStateInformation(saved.getData(), (int) saved.getSize());
    check(host.getParamValue(P_outputJack) == 1.0f, "session state restores the listening output");
}
// Phase 6 host parameter groups: the tree never changes the flattened host order.
template <typename Check>
void testParameterGroupsHost(Check&& check)
{
    Cs80AudioProcessor host;
    const auto& flat = host.getParameters();
    const auto tree = host.getParameterTree().getParameters(true);
    bool same = tree.size() == flat.size();
    for (int i = 0; same && i < flat.size(); ++i)
        same = tree[i] == flat[i];
    check(same, "the grouped tree flattens to exactly the host parameter order");

    // Independent expectation of the shipped 245, by stable ID.
    juce::StringArray expected;
    for (int i = 0; i < P_legacyNumParams; ++i) expected.add(Cs80AudioProcessor::parameterId(i));
    for (int channel = 0; channel < 2; ++channel) expected.add(Cs80AudioProcessor::hardwareSourceParameterId(channel));
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local)) expected.add(Cs80AudioProcessor::memoryParameterId(bank, local));
    for (int i = P_legacyNumParams; i <= P_sourcePerformance; ++i) expected.add(Cs80AudioProcessor::parameterId(i));
    bool order = expected.size() == 245 && flat.size() >= expected.size();
    for (int i = 0; order && i < expected.size(); ++i)
        if (auto* withId = dynamic_cast<juce::HostedAudioProcessorParameter*>(flat[i]))
            order = withId->getParameterID() == expected[i];
    check(order, "grouping keeps all 245 shipped host indexes and IDs");

    juce::StringArray topLevel;
    int grouped = 0;
    for (const auto* node : host.getParameterTree())
        if (const auto* group = node->getGroup())
        {
            topLevel.add(group->getName());
            grouped += group->getParameters(true).size();
        }
    check(topLevel.joinIntoString("|").startsWith("Global|Channel I|Channel II|Programmer|SUB OSC|KEYBOARD|FOOT|MASTER|ENGINE"),
          "top-level host groups follow the shipped blocks");
    check(grouped >= 245, "every shipped parameter belongs to a group");

    // Global subgroups carry their members' ParamDef.group.
    bool named = true;
    for (const auto* node : host.getParameterTree())
        if (const auto* global = node->getGroup(); global != nullptr && global->getName() == "Global")
            for (const auto* sub : global->getSubgroups(false))
                for (const auto* parameter : sub->getParameters(false))
                {
                    const auto* withId = dynamic_cast<const juce::HostedAudioProcessorParameter*>(parameter);
                    int index = -1;
                    for (int i = 0; withId != nullptr && i < P_numGlobals; ++i)
                        if (Cs80AudioProcessor::parameterId(i) == withId->getParameterID()) index = i;
                    named = named && index >= 0 && sub->getName() == juce::String(paramDef(index).group);
                }
    check(named, "global subgroups are contiguous ParamDef.group runs");

    const auto state = host.getAPVTS().copyState();
    bool flatState = true;
    for (const auto& child : state)
        flatState = flatState && child.hasType("PARAM");
    check(flatState && state.getNumChildren() == flat.size(), "saved state stays a flat PARAM list");
}
} // namespace cs80::tests

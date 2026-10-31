#pragma once
// ws2-engine host contracts: oversampling quality, latency and state. Invoked by
// the UI self-test after JUCE initialization; each fixture owns its processor.

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>

namespace cs80::tests
{
template <typename Check>
void testOversamplingHost(Check&& check)
{
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

    // Latency is re-reported after every mode change and equals the engine's.
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
                const int expected = oversamplingFactor(oversamplingMode(mode), circuit >= 0.5f, false, rate);
                check(host.getEngine().getOversamplingFactor() == expected,
                      "host quality choice reaches the engine factor rule");
                // ws1 merge: plus the circuit oscillator's 8 internal samples (8/N).
                check(host.getLatencySamples() == host.getEngine().getLatencySamples()
                          && host.getLatencySamples() == 63 + (circuit >= 0.5f ? 8 / expected : 0),
                      "reported host latency equals the engine's in every mode");
            }

    // Offline bounce: the circuit engine renders at HQ, legacy keeps its factor.
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
        check(host.getEngine().getOversamplingFactor() == (circuit >= 0.5f ? 8 : 2),
              "offline prepare: circuit engine HQ, legacy unchanged");
        host.setNonRealtime(false);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == 2
                  && host.getLatencySamples() == 63 + (circuit >= 0.5f ? 4 : 0),
              "realtime again without a new prepare restores the selected mode");
        host.setNonRealtime(true);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        host.processBlock(block, midi);
        check(host.getEngine().getOversamplingFactor() == (circuit >= 0.5f ? 8 : 2)
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
// Plan 1.7 engine default flip at the plugin layer: fresh instances and new
// presets use the circuit engine; old sessions keep the meaning of their schema.
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
        check(engine != nullptr && pure != nullptr && engine->getName(64) == "Engine"
                  && pure->getName(64) == "PURE CS-80", "host-visible ENGINE and PURE CS-80 names");
        check(engine->getDefaultValue() == 1.0f && host.getEngineMode() == Mode::circuit
                  && host.getEffectiveEngineMode() == Mode::circuit && !host.isPure(),
              "fresh instance selects the circuit engine");
        check(engine->getText(1.0f, 64) == "Circuit" && engine->getText(0.0f, 64) == "Legacy (compat)"
                  && engine->getValueForText("Legacy (compat)") == 0.0f && engine->getValueForText("Circuit") == 1.0f,
              "ENGINE value text round trips");
        host.prepareToPlay(48000.0, 64);
        juce::AudioBuffer<float> block(2, 64);
        juce::MidiBuffer midi;
        host.processBlock(block, midi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "a new instance renders the circuit engine");
        host.setPure(true);
        host.setEngineMode(Mode::legacy);
        host.processBlock(block, midi);
        check(host.isPure() && host.getEngineMode() == Mode::legacy && host.getEffectiveEngineMode() == Mode::circuit
                  && host.getEngine().getRenderPolicy() == RenderPolicy::isolation,
              "PURE CS-80 renders the circuit engine whatever ENGINE says");
        host.setPure(false);
        host.processBlock(block, midi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::legacy, "ENGINE Legacy (compat) renders legacy");

        // New-preset creation (the INIT action) is a fresh instance's patch.
        host.setPure(true);
        host.loadFactoryPreset(5);
        check(host.getEngineMode() == Mode::legacy, "legacy factory bank programs keep the legacy engine");
        host.loadInitPatch();
        check(host.getEngineMode() == Mode::circuit && host.getParamValue(P_subOriginalMode) == 1.0f
                  && host.getPresetName().startsWith("Init Patch") && host.isPure(),
              "new preset: Init Patch on the circuit engine, PURE (a session policy) kept");
    }

    // Old sessions: a missing or malformed ENGINE value means the default of the
    // schema that wrote the state, never the live value or a later default.
    for (int schema : { 0, 1, 2, 3 })
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
            const bool literal = juce::String(stored) == "0" || juce::String(stored) == "1";
            const float expected = literal ? juce::String(stored).getFloatValue() : (schema >= 3 ? 1.0f : 0.0f);
            check(host.getParamValue(P_sourcePerformance) == expected,
                  "ENGINE restores by schema: Legacy before schema 3, Circuit from schema 3");
        }
    {
        Cs80AudioProcessor host;
        host.setEngineMode(Mode::legacy);
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        const auto xml = juce::parseXML(saved.toString());
        check(xml != nullptr && xml->getIntAttribute(Cs80AudioProcessor::stateSchemaKey) == 3,
              "states are written with schema 3");
        Cs80AudioProcessor other;
        other.setStateInformation(saved.getData(), (int) saved.getSize());
        check(other.getEngineMode() == Mode::legacy, "a saved Legacy (compat) choice survives the flip");
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

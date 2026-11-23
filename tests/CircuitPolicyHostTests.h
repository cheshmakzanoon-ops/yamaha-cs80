#pragma once

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>
#include <limits>

namespace cs80::tests
{
// The retired policy slots must remain recognizable to old hosts without
// being able to change the instrument, cancel notes, or disable MIDI/automation.
// Run after JUCE initialization. No editor or physical audio device is needed.
template <typename Check>
void testCircuitPolicyHost(Check&& check)
{
    const auto canonical = [](const Cs80AudioProcessor& host)
    {
        return host.getParamValue(P_sourcePerformance) == 1.0f
            && host.getParamValue(P_parityMode) == 0.0f
            && host.currentValues()[P_sourcePerformance] == 1.0f
            && host.currentValues()[P_parityMode] == 0.0f
            && host.getEngineMode() == Cs80AudioProcessor::EngineMode::circuit
            && host.getEffectiveEngineMode() == Cs80AudioProcessor::EngineMode::circuit
            && !host.isPure() && host.usesHostAutomationTiming();
    };
    Cs80AudioProcessor host;
    host.prepareToPlay(48000.0, 256);
    juce::AudioBuffer<float> audio(2, 256);
    const auto render = [&](juce::MidiBuffer midi = {})
    {
        audio.clear();
        host.processBlock(audio, midi);
        check(host.getEngine().getRenderPolicy() == RenderPolicy::sourcePerformance,
              "production rendering always uses the circuit engine");
    };
    check(canonical(host), "fresh processor exposes one circuit engine");
    for (const int index : { P_parityMode, P_sourcePerformance })
    {
        auto* parameter = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index));
        auto* raw = host.getAPVTS().getRawParameterValue(Cs80AudioProcessor::parameterId(index));
        const float expected = index == P_sourcePerformance ? 1.0f : 0.0f;
        check(parameter != nullptr && raw != nullptr, "retired policy retains its published ID");
        if (parameter == nullptr || raw == nullptr) continue;
        check(parameter->getParameterIndex() == (index == P_parityMode ? 243 : 244),
              "retired policy keeps its published host position");
        check(dynamic_cast<juce::AudioParameterBool*>(parameter) != nullptr
                  && parameter->isBoolean() && parameter->getNumSteps() == 2,
              "retired policy keeps its published Boolean type");
        check(!parameter->isAutomatable(), "retired policy cannot be automated");
        for (const float value : { 0.0f, 0.49f, 0.5f, 1.0f,
                                  std::numeric_limits<float>::quiet_NaN(),
                                  std::numeric_limits<float>::infinity() })
        {
            parameter->setValueNotifyingHost(value);
            check(parameter->getValue() == expected && raw->load() == expected && canonical(host),
                  "old host policy writes cannot change host/APVTS/render agreement");
        }
        check(parameter->getValueForText("Legacy") == expected
                  && parameter->getValueForText("Pure CS-80") == expected,
              "host text entry cannot reactivate a removed engine choice");
    }

    juce::MidiBuffer start;
    start.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    render(start);
    const int held = host.getEngine().activeVoices();
    check(held > 0, "policy-write fixture holds an audible voice");
    host.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
    host.setPure(true);
    host.setParamValue(P_sourcePerformance, 0.0f);
    host.setParamValue(P_parityMode, 1.0f);
    render();
    check(canonical(host) && host.getEngine().activeVoices() == held,
          "retired API calls do not reset held notes or alter policy");

    for (const int program : { 0, 8, 38, 117, legacyProgramCount - 1,
                               programIndexForCurated(0), programIndexForCurated(curatedProgramCount() - 1) })
    {
        host.loadFactoryPreset(program);
        render();
        check(canonical(host) && host.getCurrentProgram() == program,
              "every shipped program range recalls on the same circuit engine");
    }

    // Old schemas, duplicate policy children, and old A/B state cannot restore
    // another engine. Unrelated musical settings retain their authored values.
    for (const int schema : { 0, 1, 2, 3, 4 })
    {
        auto state = host.getAPVTS().copyState();
        state.removeProperty(Cs80AudioProcessor::stateSchemaKey, nullptr);
        if (schema != 0) state.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
        state.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_mix))
            .setProperty("value", 0.37f, nullptr);
        for (const int index : { P_parityMode, P_sourcePerformance })
        {
            const auto id = Cs80AudioProcessor::parameterId(index);
            auto child = state.getChildWithProperty("id", id);
            child.setProperty("value", index == P_sourcePerformance ? 0.0f : 1.0f, nullptr);
            state.addChild(child.createCopy(), -1, nullptr);
        }
        juce::ValueTree compare("AB_COMPARE"), slot("SLOT");
        compare.setProperty("version", 1, nullptr);
        slot.setProperty("index", 1, nullptr);
        slot.setProperty(Cs80AudioProcessor::parameterId(P_mix), 0.42f, nullptr);
        slot.setProperty(Cs80AudioProcessor::parameterId(P_sourcePerformance), 0.0f, nullptr);
        slot.setProperty(Cs80AudioProcessor::parameterId(P_parityMode), 1.0f, nullptr);
        compare.addChild(slot, -1, nullptr);
        state.addChild(compare, -1, nullptr);
        const auto text = state.createXml()->toString();
        host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        render();
        check(canonical(host) && std::abs(host.getParamValue(P_mix) - 0.37f) < 1e-5f,
              "older state schemas migrate policy while retaining musical parameters");
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
            static_cast<const char*>(saved.getData()), static_cast<int>(saved.getSize())));
        check(xml != nullptr, "migrated state can be serialized");
        if (xml != nullptr)
        {
            const auto restored = juce::ValueTree::fromXml(*xml);
            check((int) restored.getProperty(Cs80AudioProcessor::stateSchemaKey) == 4,
                  "saved state explicitly marks the circuit-only schema");
            for (const int index : { P_parityMode, P_sourcePerformance })
            {
                int matches = 0;
                for (auto child : restored)
                    if (child.getProperty("id").toString() == Cs80AudioProcessor::parameterId(index))
                    {
                        ++matches;
                        check((float) child.getProperty("value") == (index == P_sourcePerformance ? 1.0f : 0.0f),
                              "saved policy values are canonical");
                    }
                check(matches == 1, "migration removes duplicate policy authorities");
            }
            const auto savedSlot = restored.getChildWithName("AB_COMPARE").getChildWithName("SLOT");
            check((float) savedSlot.getProperty(Cs80AudioProcessor::parameterId(P_sourcePerformance)) == 1.0f
                      && (float) savedSlot.getProperty(Cs80AudioProcessor::parameterId(P_parityMode)) == 0.0f,
                  "inactive A/B state also saves canonical circuit policy");
        }
        host.abToggle();
        render();
        check(canonical(host) && std::abs(host.getParamValue(P_mix) - 0.42f) < 1e-5f,
              "A/B migration recalls the sound without restoring retired policy");
    }

    // Browser REVERT is an independent recall route and must follow the same
    // invariant even when an older snapshot contains retired values.
    auto preview = host.capturePresetBrowserSound();
    preview.values[P_parityMode] = 1.0f;
    preview.values[P_sourcePerformance] = 0.0f;
    preview.values[P_mix] = 0.29f;
    check(host.restorePresetBrowserSound(preview), "browser accepts a complete historical sound snapshot");
    render();
    check(canonical(host) && std::abs(host.getParamValue(P_mix) - 0.29f) < 1e-5f,
          "browser REVERT cannot reactivate a removed mode");

    juce::MidiBuffer programChange;
    programChange.addEvent(juce::MidiMessage::controllerEvent(1, 0, 1), 0);
    programChange.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), 1);
    programChange.addEvent(juce::MidiMessage::programChange(1, 8), 2);
    render(programChange);
    check(canonical(host) && host.getCurrentProgram() == 8,
          "MIDI bank addressing remains stable and recalls the circuit instrument");
}
} // namespace cs80::tests

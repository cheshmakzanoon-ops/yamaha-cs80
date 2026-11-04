#pragma once

// [ws6-library] Host tests for the program list, Legacy Library recall, MIDI
// Program Change with bank select, A/B compare and randomise. Run from the UI
// smoke executable after JUCE initialisation; no user files are written.

#include "HostParameterState.h"
#include "core/FactoryPresets.h"
#include "core/Randomise.h"
#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

#include <cmath>
#include <string>

namespace cs80::tests
{
template <typename Check>
void testProgramLibraryHost(Check&& check)
{
    const auto& legacy = cs80::legacyLibrary();
    const auto& curated = cs80::curatedLibrary();
    const int L = cs80::legacyProgramCount;
    const int C = (int) curated.size();

    juce::AudioBuffer<float> audio(2, 256);
    auto block = [&](Cs80AudioProcessor& host, juce::MidiBuffer midi)
    {
        audio.clear();
        host.processBlock(audio, midi);
    };
    auto valuesMatch = [](const Cs80AudioProcessor& host, const cs80::ParamValues& expected)
    {
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == cs80::P_parityMode)
                continue;
            const auto& d = cs80::paramDef(i);
            if (std::abs(host.getParamValue(i) - expected[i]) > std::max(1.0e-4f, (d.max - d.min) * 1.0e-5f))
                return false;
        }
        return true;
    };

    // --- Program list: Legacy Library keeps host indexes 0..221; curated follows.
    {
        Cs80AudioProcessor host;
        check(host.getNumPrograms() == L + C && C >= 280, "host program list covers legacy and curated libraries");
        check(host.getProgramName(0) == juce::String(legacy[0].name)
                  && host.getProgramName(22) == juce::String(legacy[22].name)
                  && host.getProgramName(L) == juce::String(curated[0].name)
                  && host.getProgramName(L + C - 1) == juce::String(curated[(size_t) C - 1].name),
              "program names: legacy at their shipped indexes, curated from 222");
        host.setCurrentProgram(L + 4);
        check(host.getCurrentProgram() == L + 4 && host.getPresetName() == juce::String(curated[4].name)
                  && valuesMatch(host, curated[4].values) && host.getParamValue(cs80::P_sourcePerformance) == 1.0f,
              "setCurrentProgram recalls a curated program on the circuit engine");
        host.setCurrentProgram(22);
        check(host.getCurrentProgram() == 22 && host.getPresetName() == juce::String(legacy[22].name)
                  && valuesMatch(host, legacy[22].values) && host.getParamValue(cs80::P_sourcePerformance) == 0.0f,
              "setCurrentProgram recalls a legacy program on the legacy engine");
    }

    // --- Old sessions recall the Legacy Library by their old index and name.
    {
        Cs80AudioProcessor old;
        old.loadFactoryPreset(117);
        juce::MemoryBlock saved;
        old.getStateInformation(saved);
        Cs80AudioProcessor restored;
        restored.setStateInformation(saved.getData(), (int) saved.getSize());
        check(restored.getCurrentProgram() == 117 && restored.getPresetName() == juce::String(legacy[117].name)
                  && valuesMatch(restored, legacy[117].values),
              "session naming legacy program 117 restores the same index, name and sound");
        // A pre-library state carries only factoryIndex + presetName.
        auto tree = cs80::tests::copyHostParameterState(old);
        tree.setProperty("presetName", juce::String(legacy[40].name), nullptr);
        tree.setProperty("factoryIndex", 40, nullptr);
        const auto text = tree.createXml()->toString();
        restored.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        check(restored.getCurrentProgram() == 40 && restored.getPresetName() == juce::String(legacy[40].name),
              "pre-library state metadata keeps its legacy meaning");
    }

    // --- MIDI Program Change and bank select.
    {
        Cs80AudioProcessor host;
        host.prepareToPlay(48000.0, 256);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::programChange(1, 3), 10);
        block(host, midi);
        check(host.getCurrentProgram() == L + 3 && host.getPresetName() == juce::String(curated[3].name)
                  && valuesMatch(host, curated[3].values)
                  && host.getEngine().getRenderPolicy() == cs80::RenderPolicy::sourcePerformance,
              "Program Change in bank 0 recalls curated program 3 within the block");
        midi.clear();
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 1), 0);
        midi.addEvent(juce::MidiMessage::programChange(1, 22), 1);
        block(host, midi);
        check(host.getCurrentProgram() == 22 && host.getPresetName() == juce::String(legacy[22].name)
                  && host.getEngine().getRenderPolicy() == cs80::RenderPolicy::legacy,
              "bank MSB 1 addresses the Legacy Library by its old program number");
        midi.clear();
        midi.addEvent(juce::MidiMessage::programChange(1, 23), 0);
        block(host, midi);
        check(host.getCurrentProgram() == 23, "bank select persists until the next bank select");
        midi.clear();
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 1), 0);
        midi.addEvent(juce::MidiMessage::programChange(1, 5), 0);
        block(host, midi);
        check(host.getCurrentProgram() == L + 133 && host.getPresetName() == juce::String(curated[133].name),
              "bank LSB pages through the curated library");
        midi.clear();
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 2), 0);
        midi.addEvent(juce::MidiMessage::programChange(1, 127), 0);
        block(host, midi);
        check(host.getCurrentProgram() == L + 133, "an unassigned program number changes nothing");

        // Same-engine program change keeps sounding voices; the next program's
        // values apply from its sample position on.
        midi.clear();
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), 0);
        midi.addEvent(juce::MidiMessage::programChange(1, 0), 0);
        block(host, midi);
        midi.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        block(host, midi);
        midi.clear();
        midi.addEvent(juce::MidiMessage::programChange(1, 1), 128);
        block(host, midi);
        check(host.getEngine().activeVoices() > 0 && host.getCurrentProgram() == L + 1
                  && std::abs(host.getEngine().getParams()[cs80::chParam(0, cs80::CP_fA)] - curated[1].values[cs80::chParam(0, cs80::CP_fA)]) < 1.0e-6f,
              "Program Change between circuit programs keeps the held note and reaches the engine");

        // MPE: only the master channel selects programs.
        host.setParamValue(cs80::P_mpeOn, 1.0f);
        midi.clear();
        midi.addEvent(juce::MidiMessage::programChange(2, 7), 0);
        block(host, midi);
        check(host.getCurrentProgram() == L + 1, "MPE member channel Program Change is ignored");
        midi.clear();
        midi.addEvent(juce::MidiMessage::programChange(1, 7), 0);
        block(host, midi);
        check(host.getCurrentProgram() == L + 7, "MPE master channel Program Change is accepted");
        host.setParamValue(cs80::P_mpeOn, 0.0f);

        // PURE CS-80 ignores Program Change.
        host.setParamValue(cs80::P_parityMode, 1.0f);
        block(host, {});
        midi.clear();
        midi.addEvent(juce::MidiMessage::programChange(1, 9), 0);
        block(host, midi);
        check(host.getCurrentProgram() == L + 7 && host.getParamValue(cs80::P_parityMode) == 1.0f,
              "PURE CS-80 ignores MIDI Program Change");
        host.setParamValue(cs80::P_parityMode, 0.0f);

        // A renamed MIDI-recalled program keeps its index and the new name.
        host.setPresetName("Renamed");
        check(host.getPresetName() == "Renamed" && host.getCurrentProgram() == L + 7,
              "rename after Program Change keeps the index");
    }

    // --- A/B compare.
    {
        Cs80AudioProcessor host;
        host.setCurrentProgram(L + 10);
        const float original = host.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff));
        host.setParamValue(cs80::P_footswitchPlugged, 1.0f);
        check(host.getAbSlot() == 0 && ! host.isAbSlotStored(1), "A/B starts on A with B empty");
        host.abToggle();
        check(host.getAbSlot() == 1 && std::abs(host.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) - original) < 0.01f
                  && host.getPresetName() == juce::String(curated[10].name),
              "first toggle starts B as a copy of A");
        host.setParamValue(cs80::chParam(0, cs80::CP_lpfCutoff), 2345.0f);
        host.setMemoryValue(2, cs80::CP_lpfCutoff, 777.0f);
        host.selectHardwareSource(0, 11);
        host.abToggle();
        check(host.getAbSlot() == 0 && std::abs(host.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) - original) < 0.01f
                  && host.getHardwareSource(0) == 13 && host.getPresetName() == juce::String(curated[10].name),
              "toggle back recalls A's panel, selectors and name");
        check(host.getParamValue(cs80::P_footswitchPlugged) == 1.0f, "A/B leaves physical setup alone");
        host.abToggle();
        check(std::abs(host.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) - 2345.0f) < 0.5f
                  && std::abs(host.getMemoryValue(2, cs80::CP_lpfCutoff) - 777.0f) < 0.05f && host.getHardwareSource(0) == 11,
              "B keeps its edits, memory banks and selectors");

        // Session persistence of the inactive slot.
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        Cs80AudioProcessor restored;
        restored.setStateInformation(saved.getData(), (int) saved.getSize());
        check(restored.getAbSlot() == 1 && restored.isAbSlotStored(0)
                  && std::abs(restored.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) - 2345.0f) < 0.5f,
              "session restores the active slot and keeps the other");
        restored.abToggle();
        check(restored.getAbSlot() == 0 && std::abs(restored.getParamValue(cs80::chParam(0, cs80::CP_lpfCutoff)) - original) < 0.01f,
              "restored A slot recalls its sound");
        check(! cs80::tests::copyHostParameterState(restored).getChildWithName("AB_COMPARE").isValid(),
              "A/B slot is not duplicated into the live parameter state");

        // Copy A -> B.
        Cs80AudioProcessor copy;
        copy.setCurrentProgram(L + 20);
        copy.abCopyCurrentTo(1);
        copy.setParamValue(cs80::P_detune, 31.0f);
        copy.abToggle();
        check(copy.getAbSlot() == 1 && std::abs(copy.getParamValue(cs80::P_detune) - curated[20].values[cs80::P_detune]) < 1.0e-3f,
              "copy A->B stores the sound before the edit");
        juce::MemoryBlock fresh;
        Cs80AudioProcessor().getStateInformation(fresh);
        copy.setStateInformation(fresh.getData(), (int) fresh.getSize());
        check(copy.getAbSlot() == 0 && ! copy.isAbSlotStored(1), "a session without A/B data clears both slots");
    }

    // --- Library browser: prev/next through the current filter, Legacy section, audition.
    {
        juce::TemporaryFile folder;
        check(folder.getFile().createDirectory().wasOk(), "isolated preset folder");
        Cs80AudioProcessor host(folder.getFile());
        host.setCurrentProgram(L);
        {
            cs80::ui::PresetBrowser browser(host);
            browser.loadAdjacent(1);
            check(host.getCurrentProgram() == L + 1, "NEXT steps through the factory library");
            browser.loadAdjacent(-1);
            browser.loadAdjacent(-1);
            check(host.getCurrentProgram() == L + C - 1, "PREV wraps within the current filter (legacy excluded)");
            check(! browser.isAuditioning(), "audition starts off by default");
        }
        const auto saved = host.saveUserPreset("Browser Step");
        check(saved.wasSaved(), "user patch saved into the isolated folder");
        {
            cs80::ui::PresetBrowser browser(host);
            host.setCurrentProgram(L + C - 1);
            browser.loadAdjacent(1);
            check(host.getPresetName() == "Browser Step", "NEXT reaches user patches in the same filter");
            browser.loadAdjacent(1);
            check(host.getCurrentProgram() == L, "and wraps back to the first factory program");
        }
        host.setCurrentProgram(22);
        {
            cs80::ui::PresetBrowser browser(host);
            browser.loadAdjacent(1);
            check(host.getCurrentProgram() == 23 && host.getPresetName() == juce::String(legacy[23].name),
                  "a legacy sound opens the Legacy Library section; NEXT stays in it");
        }
        folder.getFile().deleteRecursively();
    }

    // --- Randomise within original ranges.
    {
        Cs80AudioProcessor a, b;
        a.setCurrentProgram(L + 30);
        b.setCurrentProgram(L + 30);
        a.setParamValue(cs80::P_footswitchPlugged, 1.0f);
        a.randomiseWithinOriginalRanges(0x1234abcdu);
        b.randomiseWithinOriginalRanges(0x1234abcdu);
        bool same = true, inRange = true;
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            const auto& d = cs80::paramDef(i);
            same = same && (Cs80AudioProcessor::isPhysicalSetupParameter(i) || std::abs(a.getParamValue(i) - b.getParamValue(i)) <= 1.0e-5f * std::max(1.0f, d.max - d.min));
            inRange = inRange && a.getParamValue(i) >= d.min && a.getParamValue(i) <= d.max;
        }
        check(same && inRange, "randomise is seedable and stays in range");
        check(a.getPresetName() == "Random 1234ABCD" && a.getHardwareSource(0) == 13 && a.getHardwareSource(1) == 13,
              "randomised patch is a named PANEL sound");
        check(std::abs(a.getParamValue(cs80::P_volume) - curated[30].values[cs80::P_volume]) < 1.0e-4f
                  && a.getParamValue(cs80::P_sourcePerformance) == 1.0f && a.getParamValue(cs80::P_footswitchPlugged) == 1.0f,
              "randomise keeps volume, engine policy and physical setup");
        const auto expected = cs80::library::randomiseWithinOriginalRanges(curated[30].values, 0x1234abcdu);
        check(std::abs(a.getParamValue(cs80::chParam(1, cs80::CP_lpfCutoff)) - expected[cs80::chParam(1, cs80::CP_lpfCutoff)]) < 0.05f,
              "processor randomise applies the core rule set");
    }
}
} // namespace cs80::tests

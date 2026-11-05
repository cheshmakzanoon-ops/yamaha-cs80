#pragma once

// [ws6-library] Host tests for the program list, Legacy Library recall, MIDI
// Program Change with bank select, A/B compare and randomise. Run from the UI
// smoke executable after JUCE initialisation; no user files are written.

#include "AbCompareHistoryHostTests.h"
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
        auto tree = old.getAPVTS().copyState();
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
        check(! restored.getAPVTS().copyState().getChildWithName("AB_COMPARE").isValid(),
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

    // --- A/B also owns non-parameter sound data: MSEG shapes and matrix routing.
    {
        auto a = cs80::modern::ModernConfig {};
        a.slots[0] = { cs80::modern::ModSource::mseg1, cs80::modern::ModSource::velocity,
                       cs80::modern::ModDestination::lpCutoff, cs80::modern::ModTarget::rankI };
        a.msegs[0].points[1].level = 0.25f;
        a.msegs[0].sync = true;
        a = cs80::modern::sanitizeConfig(a);
        auto b = a;
        b.slots[0] = { cs80::modern::ModSource::lfo2, cs80::modern::ModSource::none,
                       cs80::modern::ModDestination::pitch, cs80::modern::ModTarget::rankII };
        b.msegs[0].points[1].level = 0.75f;
        b.msegs[0].sync = false;
        b = cs80::modern::sanitizeConfig(b);

        Cs80AudioProcessor host;
        host.getModernLayer().setConfig(a);
        host.setParamValue(cs80::P_mod1Amount, 0.25f);
        host.abToggle();
        check(host.getModernLayer().getConfig() == a,
              "first A/B toggle copies the current MSEG shapes and routing");
        host.getModernLayer().setConfig(b);
        host.setParamValue(cs80::P_mod1Amount, 0.75f);
        host.abToggle();
        check(host.getModernLayer().getConfig() == a && host.getParamValue(cs80::P_mod1Amount) == 0.25f,
              "A/B restores A's routing and MSEG shapes together with its amounts");

        // Both active-slot choices survive two saves/restores. The receiver's
        // pre-existing routing must not leak into either restored slot.
        for (int active = 0; active < 2; ++active)
        {
            if (host.getAbSlot() != active)
                host.abToggle();
            const auto& current = active == 0 ? a : b;
            const auto& other = active == 0 ? b : a;
            check(host.getModernLayer().getConfig() == current,
                  "A/B recalls each slot's own modern sound data");
            juce::MemoryBlock saved;
            host.getStateInformation(saved);
            auto serialized = juce::parseXML(juce::String::fromUTF8(
                static_cast<const char*>(saved.getData()), (int) saved.getSize()));
            check(serialized != nullptr, "modern A/B session serializes");
            if (serialized != nullptr)
            {
                const auto tree = juce::ValueTree::fromXml(*serialized);
                const auto nested = tree.getChildWithName("AB_COMPARE").getChildWithName("SLOT")
                                        .getChildWithName(cs80::ModernLayerState::treeType);
                check(cs80::ModernLayerState::fromTree(tree.getChildWithName(cs80::ModernLayerState::treeType))
                          == current,
                      "active modern A/B configuration stays in the session's top-level state");
                check(nested.isValid() && cs80::ModernLayerState::fromTree(nested) == other,
                      "inactive modern A/B configuration is serialized inside its slot");
            }
            Cs80AudioProcessor restored;
            restored.setStateInformation(saved.getData(), (int) saved.getSize());
            check(restored.getAbSlot() == active && restored.getModernLayer().getConfig() == current
                      && restored.getParamValue(cs80::P_mod1Amount) == (active == 0 ? 0.25f : 0.75f),
                  "session restore preserves the active modern A/B sound");
            restored.abToggle();
            check(restored.getModernLayer().getConfig() == other
                      && restored.getParamValue(cs80::P_mod1Amount) == (active == 0 ? 0.75f : 0.25f),
                  "session restore preserves the inactive modern A/B sound");
            restored.getStateInformation(saved);
            host.getModernLayer().resetToDefaults();
            host.setStateInformation(saved.getData(), (int) saved.getSize());
            host.abToggle();
            check(host.getModernLayer().getConfig() == current,
                  "repeated A/B session saves retain both modulation configurations");
        }

        // Before the modern child was stored per slot, both slots shared the
        // session's top-level config. Preserve that sound when reading old data.
        juce::MemoryBlock saved;
        host.getStateInformation(saved);
        auto xml = juce::parseXML(juce::String::fromUTF8(
            static_cast<const char*>(saved.getData()), (int) saved.getSize()));
        check(xml != nullptr, "A/B modern-data fixture serializes");
        if (xml != nullptr)
        {
            auto legacy = juce::ValueTree::fromXml(*xml);
            auto slot = legacy.getChildWithName("AB_COMPARE").getChildWithName("SLOT");
            slot.removeChild(slot.getChildWithName(cs80::ModernLayerState::treeType), nullptr);
            const auto text = legacy.createXml()->toString();
            const auto current = host.getModernLayer().getConfig();
            host.getModernLayer().resetToDefaults();
            host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
            host.abToggle();
            check(host.getModernLayer().getConfig() == current,
                  "old A/B states without per-slot modern data retain their shared session config");

            auto oldest = legacy.createCopy();
            oldest.removeChild(oldest.getChildWithName(cs80::ModernLayerState::treeType), nullptr);
            const auto oldestText = oldest.createXml()->toString();
            host.getModernLayer().setConfig(b);
            host.setStateInformation(oldestText.toRawUTF8(), oldestText.getNumBytesAsUTF8());
            host.abToggle();
            check(host.getModernLayer().getConfig() == cs80::modern::ModernConfig {},
                  "A/B states without any modern data restore defaults rather than prior live edits");

            // A present but malformed nested child is sanitized independently;
            // it is not mistaken for missing legacy data and replaced by A.
            auto broken = legacy.createCopy();
            auto nested = cs80::ModernLayerState::toTree(a);
            nested.getChildWithName("MATRIX").getChild(0).setProperty("source", 1000, nullptr);
            broken.getChildWithName("AB_COMPARE").getChildWithName("SLOT").addChild(nested, -1, nullptr);
            const auto expected = cs80::ModernLayerState::fromTree(nested);
            const auto brokenText = broken.createXml()->toString();
            host.setStateInformation(brokenText.toRawUTF8(), brokenText.getNumBytesAsUTF8());
            check(host.getModernLayer().getConfig() == current,
                  "malformed inactive A/B modulation data does not alter the active sound");
            host.abToggle();
            check(host.getModernLayer().getConfig() == expected,
                  "inactive A/B modulation data uses the normal modern-state sanitation");
        }

        host.getModernLayer().resetToDefaults();
        host.abCopyCurrentTo(1 - host.getAbSlot());
        host.getModernLayer().setConfig(b);
        host.abToggle();
        check(host.getModernLayer().getConfig() == cs80::modern::ModernConfig {},
              "A/B copy captures an explicitly default modern configuration");
        host.getModernLayer().setConfig(a);
        host.abCopyCurrentTo(1 - host.getAbSlot());
        host.getModernLayer().setConfig(b);
        host.abToggle();
        host.prepareToPlay(48000.0, 256);
        block(host, {});
        check(host.getModernLayer().getConfig() == a && host.getEngine().getModernConfig() == a,
              "A/B copy restores modern sound data and publishes it to the audio engine");

        host.abCopyCurrentTo(1 - host.getAbSlot());
        host.getModernLayer().setConfig(b);
        host.getUndoManager().clearUndoHistory();
        {
            Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Compare modulation");
            host.abToggle();
        }
        check(host.getModernLayer().getConfig() == a && host.undo()
                  && host.getModernLayer().getConfig() == b,
              "modern-only A/B recall joins the toolbar's undo transaction");
        check(host.redo() && host.getModernLayer().getConfig() == a,
              "modern-only A/B recall can be redone");
        check(!host.getAPVTS().copyState().getChildWithName("AB_COMPARE").isValid(),
              "modern A/B snapshots stay outside the live parameter tree");
    }

    testAbCompareHistoryHost(check);

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

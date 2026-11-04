#pragma once

#include "plugin/PluginProcessor.h"

#include <cmath>
#include <memory>

namespace cs80::tests
{
template <typename Check>
void testPresetRecallHost(Check&& check)
{
    // Scala/KBM data belongs to the session, not the sound. A preset must not
    // select or disable a tuning source while leaving its associated data behind.
    const juce::String liveScale = "! just\nLive just tuning\n12\n16/15\n9/8\n6/5\n5/4\n4/3\n45/32\n3/2\n8/5\n5/3\n9/5\n15/8\n2/1\n";
    const juce::String writerScale = "! equal\nWriter equal tuning\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n1200.0\n";
    juce::TemporaryFile fixture("-cs80-preset-recall");
    auto writer = std::make_unique<Cs80AudioProcessor>(fixture.getFile().getChildFile("presets"));
    check(writer->loadScalaTuning(writerScale).isEmpty(), "preset writer loads its own session tuning");
    const auto sound = chParam(0, CP_lpfCutoff);
    writer->setParamValue(sound, 1800.0f);
    // These four controls deliberately remain part of USER preset recall.
    for (int index : { P_footswitchPlugged, P_footswitchPressed, P_footMode, P_outputLevel })
        writer->setParamValue(index, 0.0f);

    auto exercise = [&](const juce::File& file, int liveMode)
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        check(host->loadScalaTuning(liveScale).isEmpty(), "preset reader loads independent live tuning");
        host->setParamValue(P_tuningMode, (float) liveMode);
        host->setParamValue(sound, 650.0f);
        for (int index : { P_footswitchPlugged, P_footswitchPressed, P_footMode, P_outputLevel })
            host->setParamValue(index, 1.0f);
        // Repeat recall to cover retained tree/state, not only a fresh load.
        for (int repeat = 0; repeat < 2; ++repeat)
        {
            host->loadUserPreset(file);
            check(host->getParamValue(P_tuningMode) == (float) liveMode,
                  "user preset preserves the live tuning source");
            check(host->getScalaDescription() == "Live just tuning",
                  "user preset leaves the live Scala data untouched");
            check(std::abs(host->getParamValue(sound) - 1800.0f) < 0.01f,
                  "tuning preservation still recalls the user preset sound");
            for (int index : { P_footswitchPlugged, P_footswitchPressed, P_footMode, P_outputLevel })
                check(host->getParamValue(index) == 0.0f,
                      "user preset retains its documented pedal and output recall behavior");
        }
        if (liveMode == 1)
        {
            host->prepareToPlay(48000.0, 256);
            juce::AudioBuffer<float> block(2, 256);
            block.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
            host->processBlock(block, midi);
            const float expectedHz = midiToHz(60.0f) * 1.25f;
            const float actualHz = host->getEngine().getRankInfo(0, 0).oscillatorHz;
            check(std::abs(12.0f * std::log2(actualHz / expectedHz)) < 0.05f,
                  "preset recall still plays the live Scala tuning on the audio thread");
        }
    };

    juce::MemoryBlock session;
    for (int savedMode = 0; savedMode <= 2; ++savedMode)
    {
        writer->setParamValue(P_tuningMode, (float) savedMode);
        const auto saved = writer->saveUserPreset("Tuning source " + juce::String(savedMode));
        check(saved.wasSaved(), "preset-recall fixture saves a real user preset");
        if (!saved.wasSaved()) return;
        for (int liveMode = 0; liveMode <= 2; ++liveMode)
            exercise(saved.file, liveMode);
        if (savedMode != 1) continue;
        writer->getStateInformation(session);
        auto xml = juce::XmlDocument::parse(saved.file);
        check(xml != nullptr, "preset-recall fixture parses the saved preset");
        if (xml == nullptr) return;
        auto oldState = juce::ValueTree::fromXml(*xml);
        oldState.removeProperty(Cs80AudioProcessor::stateSchemaKey, nullptr);
        oldState.removeChild(oldState.getChildWithProperty("id", "tuning_mode"), nullptr);
        const auto oldFile = fixture.getFile().getChildFile("old.cs80xml");
        check(oldState.createXml()->writeTo(oldFile), "legacy preset fixture omits tuning mode");
        exercise(oldFile, 1);
        auto malformedState = juce::ValueTree::fromXml(*xml);
        malformedState.getChildWithProperty("id", "tuning_mode").setProperty("value", "nan", nullptr);
        const auto malformedFile = fixture.getFile().getChildFile("malformed.cs80xml");
        check(malformedState.createXml()->writeTo(malformedFile), "malformed preset fixture has invalid tuning mode");
        exercise(malformedFile, 1);
    }

    // Reopening a project restores BOTH its source and the associated tuning.
    writer->loadScalaTuning(liveScale);
    writer->setParamValue(P_tuningMode, 0.0f);
    writer->setStateInformation(session.getData(), (int) session.getSize());
    check(writer->getParamValue(P_tuningMode) == 1.0f
              && writer->getScalaDescription() == "Writer equal tuning",
          "host session restore still recalls its saved tuning source and Scala data");
}
} // namespace cs80::tests

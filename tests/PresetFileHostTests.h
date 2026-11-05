#pragma once

#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace cs80::tests
{
template <typename Check>
void testPresetFileHost(Check&& check)
{
    // Imported files may contain repeated parameter ids. JUCE uses the last
    // occurrence, so it must be the one normalized before it reaches APVTS.
    // All I/O is restricted to this newly allocated temporary directory.
    juce::TemporaryFile fixture("-cs80-preset-files");
    const auto root = fixture.getFile();
    check(root.createDirectory().wasOk(), "preset-file fixture directory created");
    if (! root.isDirectory()) return;
    auto writer = std::make_unique<Cs80AudioProcessor>(root.getChildFile("written"));
    auto reader = std::make_unique<Cs80AudioProcessor>(root.getChildFile("round-trip"));
    const auto saved = writer->saveUserPreset("Duplicate import");
    check(saved.wasSaved(), "preset-file source is a real saved user preset");
    if (! saved.wasSaved()) return;
    auto xml = juce::XmlDocument::parse(saved.file);
    check(xml != nullptr, "preset-file source parses");
    if (! xml) return;
    const auto original = juce::ValueTree::fromXml(*xml);

    const auto append = [](juce::ValueTree& tree, const juce::String& id, const juce::var& value)
    {
        juce::ValueTree child("PARAM");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", value, nullptr);
        child.setProperty("futureProperty", "kept on final occurrence", nullptr);
        tree.appendChild(child, nullptr);
    };
    const auto count = [](const juce::ValueTree& tree, const juce::String& id)
    {
        int result = 0;
        for (const auto& child : tree)
            if (child.getProperty("id").toString() == id) ++result;
        return result;
    };
    const auto write = [&](const juce::ValueTree& tree)
    {
        const auto file = root.getChildFile("import.cs80xml");
        const bool ok = tree.createXml()->writeTo(file);
        check(ok, "preset-file import fixture written");
        return ok ? file : juce::File();
    };
    const auto restoreSession = [&](const juce::ValueTree& tree)
    {
        const auto text = tree.createXml()->toString();
        reader->setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
    };

    // Check the complete real host layout, not just the core parameters: four
    // memory banks and both hardware-source selectors must also be canonical.
    auto duplicates = original.createCopy();
    for (auto* parameter : writer->getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            append(duplicates, ranged->paramID, ranged->convertFrom0to1(0.75f));
    juce::ValueTree future("FUTURE_DATA");
    future.setProperty("id", "unknown_parameter", nullptr);
    future.setProperty("value", "opaque", nullptr);
    duplicates.appendChild(future, nullptr);
    duplicates.appendChild(future.createCopy(), nullptr);
    const auto duplicateFile = write(duplicates);
    if (duplicateFile == juce::File()) return;

    for (int repeat = 0; repeat < 2; ++repeat)
    {
        for (int id : { P_parityMode, P_oversampling, P_outputJack, P_tuningMode })
            reader->setParamValue(id, 0.0f);
        reader->loadUserPreset(duplicateFile);
        const auto state = reader->getAPVTS().copyState();
        for (auto* parameter : reader->getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
            if (ranged == nullptr) continue;
            const bool session = ranged->paramID == "parity_mode" || ranged->paramID == "oversampling"
                              || ranged->paramID == "output_jack" || ranged->paramID == "tuning_mode";
            const float expected = session ? 0.0f : ranged->convertFrom0to1(0.75f);
            const float actual = reader->getAPVTS().getRawParameterValue(ranged->paramID)->load();
            check(std::abs(actual - expected) <= std::max(1.0e-5f, std::abs(expected) * 1.0e-5f),
                  "duplicate import retains last sound value and live session settings");
            check(count(state, ranged->paramID) == 1,
                  "duplicate import leaves exactly one child for every known host parameter");
            check(state.getChildWithProperty("id", ranged->paramID)["futureProperty"].toString()
                      == "kept on final occurrence",
                  "duplicate import preserves extra properties on the final parameter");
        }
        check(count(state, "unknown_parameter") == 2,
              "duplicate normalization does not discard unknown future data");
    }

    // The result must remain canonical across actual export and re-import.
    const auto roundTrip = reader->saveUserPreset("Canonical import");
    check(roundTrip.wasSaved(), "normalized import exports successfully");
    if (roundTrip.wasSaved())
    {
        auto exported = juce::XmlDocument::parse(roundTrip.file);
        check(exported != nullptr, "normalized export is valid XML");
        if (exported)
        {
            const auto state = juce::ValueTree::fromXml(*exported);
            for (auto* parameter : reader->getParameters())
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                    check(count(state, ranged->paramID) == 1,
                          "export writes one canonical occurrence for each known parameter");
        }
        writer->loadUserPreset(roundTrip.file);
        check(std::abs(writer->getParamValue(P_macro1) - 0.75f) < 1.0e-6f,
              "canonical export re-imports the last duplicate sound value");
    }

    // A valid first occurrence must not shield a malformed final policy from
    // the existing schema fallback. Cover both recall paths and old/new files.
    for (int schema : { 1, Cs80AudioProcessor::stateSchemaVersion })
        for (bool session : { false, true })
        {
            auto malformed = original.createCopy();
            malformed.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
            const bool modern = schema >= 3;
            append(malformed, "source_performance", modern ? "0.25" : "0.75");
            append(malformed, "sub_original_mode", "0.75");
            append(malformed, "parity_mode", "0.75");
            reader->setParamValue(P_parityMode, 0.0f);
            if (session) restoreSession(malformed);
            else if (const auto file = write(malformed); file != juce::File()) reader->loadUserPreset(file);
            check(reader->getParamValue(P_sourcePerformance) == (modern ? 1.0f : 0.0f),
                  "final duplicate ENGINE policy is normalized by its writing schema");
            check(reader->getParamValue(P_subOriginalMode) == 0.0f,
                  "final duplicate original SUB mode cannot bypass literal policy validation");
            check(reader->getParamValue(P_parityMode) == 0.0f,
                  "final duplicate PURE policy cannot bypass validation or live preservation");
        }

    // Host sessions still restore their stored setup, unlike user presets.
    restoreSession(duplicates);
    check(reader->getParamValue(P_parityMode) == 1.0f
              && reader->getParamValue(P_oversampling) == 2.0f
              && reader->getParamValue(P_outputJack) == 2.0f
              && reader->getParamValue(P_tuningMode) == 1.5f,
          "host session restores the final duplicate setup values");
    const auto sessionState = reader->getAPVTS().copyState();
    check(count(sessionState, "source_performance") == 1 && count(sessionState, "macro1") == 1,
          "host session and user-preset restoration share canonical parameter identity");

    auto triple = original.createCopy();
    append(triple, "macro1", 0.4f);
    triple.appendChild(future.createCopy(), nullptr);
    append(triple, "macro1", 0.9f);
    append(triple, Cs80AudioProcessor::hardwareSourceParameterId(0), "-1e999");
    append(triple, Cs80AudioProcessor::memoryParameterId(0, CP_lpfCutoff), "1e999");
    if (const auto file = write(triple); file != juce::File()) reader->loadUserPreset(file);
    const auto tripleState = reader->getAPVTS().copyState();
    check(std::abs(reader->getParamValue(P_macro1) - 0.9f) < 1.0e-6f
              && count(tripleState, "macro1") == 1 && count(tripleState, "unknown_parameter") == 1,
          "three interleaved occurrences retain only the final known value and unknown data");
    check(reader->getHardwareSource(0) == 13,
          "nonfinite final duplicate hardware source restores PANEL fallback");
    check(std::abs(reader->getMemoryValue(0, CP_lpfCutoff) - kChannelDefs[CP_lpfCutoff].def) < 0.02f,
          "nonfinite final duplicate memory control restores its declared default");

    // A root factory index is metadata for the custom sound, not a request to
    // recall a program. Hosts require a valid index while synchronizing state.
    // Cover both restore paths, repeated loads, and their subsequent exports.
    const int lastProgram = reader->getNumPrograms() - 1;
    check(lastProgram > 0, "factory-index fixtures have multiple host programs");
    if (lastProgram <= 0) return;
    for (bool session : { false, true })
        for (bool missing : { false, true })
            for (int index : { std::numeric_limits<int>::min(), -1000000, -2, -1,
                               0, 1, legacyProgramCount - 1, legacyProgramCount,
                               lastProgram / 2, lastProgram, lastProgram + 1,
                               1000000, std::numeric_limits<int>::max() })
            {
                // One missing-property fixture is enough per restore path.
                if (missing && index != 0) continue;
                auto indexed = original.createCopy();
                indexed.setProperty("presetName", "Custom index fixture", nullptr);
                if (missing) indexed.removeProperty("factoryIndex", nullptr);
                else indexed.setProperty("factoryIndex", index, nullptr);
                append(indexed, "macro1", 0.73125f);
                append(indexed, "macro2", 0.26875f);
                const int expected = missing ? 0 : std::clamp(index, 0, lastProgram);
                for (int repeat = 0; repeat < 2; ++repeat)
                {
                    // Start at a different valid program to expose stale metadata.
                    reader->setCurrentProgram(expected == 0 ? lastProgram : 0);
                    if (session) restoreSession(indexed);
                    else if (const auto file = write(indexed); file != juce::File())
                        reader->loadUserPreset(file);
                    const auto metadata = reader->getPresetMetadata();
                    check(reader->getCurrentProgram() == expected && metadata.factoryIndex == expected,
                          "restore bounds factory index before exposing it to the host");
                    check(metadata.name == "Custom index fixture",
                          "factory-index normalization retains the custom preset name");
                    check(std::abs(reader->getParamValue(P_macro1) - 0.73125f) < 1.0e-6f
                              && std::abs(reader->getParamValue(P_macro2) - 0.26875f) < 1.0e-6f,
                          "factory-index normalization does not recall factory parameters");
                }

                const auto checkExport = [&](const juce::ValueTree& state)
                {
                    check((int) state.getProperty("factoryIndex", -1) == expected,
                          "subsequent export writes the normalized factory index");
                    check(state.getProperty("presetName").toString() == "Custom index fixture",
                          "subsequent export retains the custom preset name");
                };
                const auto checkReimport = [&]
                {
                    const auto metadata = writer->getPresetMetadata();
                    check(writer->getCurrentProgram() == expected && metadata.factoryIndex == expected,
                          "round-trip reimport retains the normalized factory index");
                    check(metadata.name == "Custom index fixture"
                              && std::abs(writer->getParamValue(P_macro1) - 0.73125f) < 1.0e-6f
                              && std::abs(writer->getParamValue(P_macro2) - 0.26875f) < 1.0e-6f,
                          "round-trip reimport retains the custom name and sound");
                };
                const auto prepareReimport = [&]
                {
                    writer->setCurrentProgram(expected == 0 ? lastProgram : 0);
                    writer->setPresetName("Before index round-trip");
                    writer->setParamValue(P_macro1, 0.125f);
                    writer->setParamValue(P_macro2, 0.875f);
                };
                juce::MemoryBlock stateData;
                reader->getStateInformation(stateData);
                auto stateXml = juce::XmlDocument::parse(juce::String::fromUTF8(
                    static_cast<const char*>(stateData.getData()), (int) stateData.getSize()));
                check(stateXml != nullptr, "factory-index host round-trip is valid XML");
                if (stateXml) checkExport(juce::ValueTree::fromXml(*stateXml));
                prepareReimport();
                writer->setStateInformation(stateData.getData(), (int) stateData.getSize());
                checkReimport();

                const auto exported = reader->saveUserPreset("Custom index fixture",
                                                            Cs80AudioProcessor::PresetOverwrite::yes);
                check(exported.wasSaved(), "factory-index user-preset round-trip saves");
                if (exported.wasSaved())
                {
                    auto exportedXml = juce::XmlDocument::parse(exported.file);
                    check(exportedXml != nullptr, "factory-index user-preset round-trip is valid XML");
                    if (exportedXml) checkExport(juce::ValueTree::fromXml(*exportedXml));
                    prepareReimport();
                    writer->loadUserPreset(exported.file);
                    checkReimport();
                }
            }
}
} // namespace cs80::tests

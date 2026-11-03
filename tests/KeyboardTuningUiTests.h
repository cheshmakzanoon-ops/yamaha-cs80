#pragma once

// Regressions for KBM imports after reopening the editor or restoring a session.
// These call the same mapping-import method as the asynchronous file chooser;
// file-dialog automation is deliberately separate from the tuning state contract.

#include "core/Tuning.h"
#include "plugin/KeyboardTuningPage.h"
#include "plugin/PluginProcessor.h"

#include <cmath>

namespace cs80::tests
{
// Deterministically place a full load/clear between a mapping import's source
// snapshot and publication. This exercises the real version-checked publisher
// without relying on thread timing or changing the production audio path.
struct ScalaMappingAccess
{
    struct Snapshot { juce::String scl; unsigned version; };
    static Snapshot capture(Cs80AudioProcessor& host)
    {
        const juce::SpinLock::ScopedLockType lock(host.scalaLock);
        return { host.scalaText, host.scalaVersion.load(std::memory_order_relaxed) };
    }
    static juce::String finish(Cs80AudioProcessor& host, const Snapshot& source, const juce::String& kbm)
    {
        return host.publishScala(source.scl, kbm, source.version);
    }
};

namespace keyboard_tuning_ui_detail
{
inline juce::ValueTree savedTuning(Cs80AudioProcessor& host)
{
    juce::MemoryBlock state;
    host.getStateInformation(state);
    const auto xml = juce::XmlDocument::parse(
        juce::String::fromUTF8(static_cast<const char*>(state.getData()), (int) state.getSize()));
    return xml != nullptr ? juce::ValueTree::fromXml(*xml).getChildWithName("WS3_TUNING")
                          : juce::ValueTree();
}

inline bool mappingButtonEnabled(cs80::ui::modern_layer::KeyboardTuningPage& page)
{
    for (auto* child : page.getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText() == "LOAD .KBM")
                return button->isEnabled();
    return false;
}
} // namespace keyboard_tuning_ui_detail

template <typename Check>
void testKeyboardTuningUiState(Check check)
{
    using namespace keyboard_tuning_ui_detail;
    using Page = cs80::ui::modern_layer::KeyboardTuningPage;
    const juce::String scale = "! reopen.scl\nReopened twelve-tone scale\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n2/1\n";
    const juce::String replacementScale = "! replacement.scl\nReplacement twelve-tone scale\n12\n101.0\n201.0\n301.0\n401.0\n501.0\n601.0\n701.0\n801.0\n901.0\n1001.0\n1101.0\n2/1\n";
    const juce::String mapping432 = "! A4 432 Hz\n0\n0\n127\n60\n69\n432.0\n12\n";
    const juce::String mapping444 = "! A4 444 Hz\n0\n0\n127\n60\n69\n444.0\n12\n";
    juce::TemporaryFile root("-cs80-tuning-reopen-ui");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    // The tuning assertion concerns the imported mapping, not the independently
    // modelled component spread or thermal drift of an individual voice card.
    host.setParamValue(cs80::P_voiceDispersion, 0.0f);
    host.setParamValue(cs80::P_drift, 0.0f);
    host.prepareToPlay(48000.0, 256);

    {
        Page page(host);
        check(! mappingButtonEnabled(page), "a fresh tuning page disables KBM import without a scale");
        check(page.loadKeyboardMappingText(mapping432).isNotEmpty() && ! savedTuning(host).isValid(),
              "KBM import without a scale reports an error without creating tuning state");
        check(page.loadScalaText(scale).isEmpty(), "the first editor loads the Scala source");
        check(page.loadKeyboardMappingText(mapping432).isEmpty(), "the first editor loads its KBM mapping");
    }

    {
        Page reopened(host);
        const auto parameters = host.currentValues();
        check(mappingButtonEnabled(reopened) && reopened.tuningText().contains("Reopened twelve-tone scale"),
              "a reopened page offers KBM import for the persisted Scala source");
        check(reopened.loadKeyboardMappingText(mapping444).isEmpty(),
              "a reopened page imports a KBM without reloading its SCL file");
        auto tuning = savedTuning(host);
        check(tuning.getProperty("scl").toString() == scale && tuning.getProperty("kbm").toString() == mapping444,
              "KBM import changes only the mapping and preserves the persisted Scala text");
        bool unchanged = true;
        const auto after = host.currentValues();
        for (int i = 0; i < cs80::P_numParams; ++i)
            unchanged = unchanged && after[i] == parameters[i];
        check(unchanged, "KBM import leaves every host parameter unchanged");
        check(reopened.loadKeyboardMappingText("invalid mapping").isNotEmpty()
                  && tuning.isEquivalentTo(savedTuning(host)),
              "a rejected KBM keeps the previous scale and mapping together");

        // Audio is checked through the normal publication path, not by reading the
        // saved text alone. Equal temperament plus this KBM puts MIDI 69 at 444 Hz.
        host.setParamValue(cs80::P_tuningMode, 1.0f);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> audio(2, 256);
        audio.clear();
        host.processBlock(audio, midi);
        check(std::abs(host.getEngine().getRankInfo(0, 0).oscillatorHz - 444.0f) < 0.1f,
              "a reopened-page KBM import reaches the audio engine");
    }

    // Combined parser/publication regression: syntactically valid tuning can
    // still be outside the core's nominal float-frequency domain. Rejection
    // preserves only the tuning component, not an entire restored session.
    {
        Page page(host);
        const auto validTuning = savedTuning(host);
        const auto validSource = ScalaMappingAccess::capture(host);
        const auto validDescription = host.getScalaDescription();
        const auto validLabel = page.tuningText();
        const auto parameters = host.currentValues();
        const juce::String widePeriod = "wide period\n1\n12000.0\n";
        const juce::String tinyReference = "0." + juce::String::repeatedString("0", 39) + "1";
        const auto unrenderableMapping = "0\n127\n127\n127\n127\n" + tinyReference + "\n12\n";
        const juce::String frequencyError = "tuning produced an unrenderable nominal float frequency";
        cs80::tuning::Scale parsedScale;
        cs80::tuning::KeyboardMapping parsedMapping;
        check(cs80::tuning::parseScale(widePeriod.toStdString(), parsedScale).empty()
                  && cs80::tuning::parseKeyboardMapping(unrenderableMapping.toStdString(), parsedMapping).empty(),
              "combined rejection fixtures are valid Scala and KBM syntax");
        auto tuningUnchanged = [&]
        {
            const auto current = ScalaMappingAccess::capture(host);
            return validTuning.isEquivalentTo(savedTuning(host))
                && current.scl == validSource.scl && current.version == validSource.version
                && host.getScalaDescription() == validDescription && host.hasScalaTuning();
        };
        auto parametersUnchanged = [&](int except = -1)
        {
            const auto current = host.currentValues();
            for (int i = 0; i < cs80::P_numParams; ++i)
                if (i != except && current[i] != parameters[i]) return false;
            return true;
        };
        auto retainedAudioTuning = [&]
        {
            juce::AudioBuffer<float> audio(2, 256);
            audio.clear();
            juce::MidiBuffer noEvents;
            host.processBlock(audio, noEvents);
            const auto voice = host.getEngine().getVoiceInfo(0);
            return voice.active && voice.note == 69
                && std::abs(host.getEngine().getRankInfo(0, 0).oscillatorHz - 444.0f) < 0.1f;
        };
        check(page.loadScalaText(widePeriod) == frequencyError,
              "the tuning page rejects a valid wide-period Scala table outside the frequency domain");
        check(tuningUnchanged() && parametersUnchanged() && page.tuningText() == validLabel
                  && mappingButtonEnabled(page) && page.statusText().contains(frequencyError),
              "rejected Scala import preserves source, generation, description, controls and every parameter");
        check(retainedAudioTuning(), "rejected Scala import leaves the sounding note at its previous mapped pitch");
        check(page.loadKeyboardMappingText(unrenderableMapping) == frequencyError,
              "mapping-only import rejects a valid KBM with unrenderable fallback frequencies");
        check(tuningUnchanged() && parametersUnchanged() && page.tuningText() == validLabel
                  && mappingButtonEnabled(page) && page.statusText().contains(frequencyError),
              "rejected KBM import preserves source, generation, description, controls and every parameter");
        check(retainedAudioTuning(), "rejected KBM import leaves the sounding note at its previous mapped pitch");

        juce::MemoryBlock validSession;
        host.getStateInformation(validSession);
        const auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
            static_cast<const char*>(validSession.getData()), (int) validSession.getSize()));
        check(xml != nullptr, "combined rejection fixture has a readable saved session");
        if (xml != nullptr)
        {
            const auto session = juce::ValueTree::fromXml(*xml);
            for (bool badMapping : { false, true })
            {
                auto candidate = session.createCopy();
                auto tuning = candidate.getChildWithName("WS3_TUNING");
                auto volume = candidate.getChildWithProperty("id", Cs80AudioProcessor::parameterId(cs80::P_volume));
                check(tuning.isValid() && volume.isValid(), "rejected-session fixture contains tuning and volume fields");
                if (!tuning.isValid() || !volume.isValid()) continue;
                const float targetVolume = badMapping ? 0.75f : 0.25f;
                check(host.getParamValue(cs80::P_volume) != targetVolume, "rejected-session fixture changes an ordinary parameter");
                tuning.setProperty("scl", badMapping ? scale : widePeriod, nullptr);
                tuning.setProperty("kbm", badMapping ? unrenderableMapping : juce::String(), nullptr);
                volume.setProperty("value", targetVolume, nullptr);
                const auto text = candidate.createXml()->toString();
                host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
                check(tuningUnchanged(), "rejected session tuning preserves the previously published tuning component");
                check(host.getParamValue(cs80::P_volume) == targetVolume && parametersUnchanged(cs80::P_volume),
                      "a rejected tuning component does not prevent ordinary session parameters from restoring");
                Page reopened(host);
                check(mappingButtonEnabled(reopened) && reopened.tuningText() == validLabel,
                      "the reopened page shows the retained scale after a session tuning rejection");
                check(retainedAudioTuning(), "rejected session tuning leaves the sounding note at its previous mapped pitch");
            }
        }
    }

    juce::MemoryBlock state;
    host.getStateInformation(state);
    Cs80AudioProcessor restored(root.getFile().getChildFile("restored"));
    restored.setStateInformation(state.getData(), (int) state.getSize());
    {
        Page page(restored);
        check(mappingButtonEnabled(page) && page.loadKeyboardMappingText(mapping432).isEmpty(),
              "a restored-session editor imports a KBM using its saved Scala source");
        const auto tuning = savedTuning(restored);
        check(tuning.getProperty("scl").toString() == scale && tuning.getProperty("kbm").toString() == mapping432,
              "mapping changes after session restore preserve the saved scale");
    }

    {
        Page page(host);
        check(page.loadScalaText(scale, mapping432).isEmpty(), "an open editor starts from a known scale and mapping");
        check(restored.loadScalaTuning(replacementScale).isEmpty(), "the replacement session has a different Scala source");
        juce::MemoryBlock replacement;
        restored.getStateInformation(replacement);
        host.setStateInformation(replacement.getData(), (int) replacement.getSize());
        check(page.loadKeyboardMappingText(mapping444).isEmpty(),
              "an open page imports a KBM after external session replacement");
        const auto tuning = savedTuning(host);
        check(tuning.getProperty("scl").toString() == replacementScale
                  && tuning.getProperty("kbm").toString() == mapping444
                  && page.tuningText().contains("Replacement twelve-tone scale"),
              "KBM import uses the current processor scale, never a stale page-local copy");

        host.clearScalaTuning();
        check(page.loadKeyboardMappingText(mapping432).isNotEmpty() && ! savedTuning(host).isValid(),
              "KBM import cannot resurrect a scale cleared outside the page");
        check(! mappingButtonEnabled(page) && page.tuningText().contains("no scale loaded"),
              "the mapping button and description follow an externally cleared scale");
    }

    // Scala permits empty and whitespace-only descriptions. Source presence,
    // rather than the display label, determines whether a mapping can be loaded.
    for (const auto& description : { juce::String(), juce::String(" \t ") })
    {
        const auto unnamed = "! unnamed.scl\n" + description
            + "\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n2/1\n";
        {
            Page page(host);
            check(page.loadScalaText(unnamed).isEmpty() && host.hasScalaTuning()
                      && host.getScalaDescription().isEmpty() && mappingButtonEnabled(page),
                  "an unnamed Scala scale remains loaded and enables KBM import");
            check(page.tuningText().contains("unnamed scale")
                      && page.loadKeyboardMappingText(mapping432).isEmpty(),
                  "an unnamed Scala scale has a useful label and accepts its mapping");
        }
        Page reopened(host);
        check(mappingButtonEnabled(reopened) && reopened.loadKeyboardMappingText(mapping444).isEmpty()
                  && savedTuning(host).getProperty("scl").toString() == unnamed,
              "an unnamed Scala scale accepts a replacement KBM after editor reopen");
    }

    // Force the read -> write -> publish ordering of a concurrent session load.
    check(host.loadScalaTuning(scale, mapping432).isEmpty(), "version-guard fixture loads its original source");
    const auto original = ScalaMappingAccess::capture(host);
    check(ScalaMappingAccess::finish(host, original, mapping444).isEmpty(),
          "a version-checked mapping publication succeeds when its source is still current");
    const auto beforeReplacement = ScalaMappingAccess::capture(host);
    check(host.loadScalaTuning(replacementScale, mapping432).isEmpty(),
          "the forced interleaving replaces the source before stale publication");
    const auto replacement = savedTuning(host);
    check(ScalaMappingAccess::finish(host, beforeReplacement, mapping444).isNotEmpty()
              && replacement.isEquivalentTo(savedTuning(host)),
          "the version guard rejects a stale mapping without overwriting the replacement scale");
    const auto beforeClear = ScalaMappingAccess::capture(host);
    host.clearScalaTuning();
    check(ScalaMappingAccess::finish(host, beforeClear, mapping444).isNotEmpty()
              && ! host.hasScalaTuning() && ! savedTuning(host).isValid(),
          "the version guard rejects a stale mapping without resurrecting a cleared scale");
}
} // namespace cs80::tests

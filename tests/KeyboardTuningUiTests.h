#pragma once

// Regressions for KBM imports after reopening the editor or restoring a session.
// These call the same mapping-import method as the asynchronous file chooser;
// file-dialog automation is deliberately separate from the tuning state contract.

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
    struct Snapshot
    {
        juce::String scl, kbm, description;
        cs80::tuning::TuningTable table;
        unsigned version;
    };
    static Snapshot capture(Cs80AudioProcessor& host)
    {
        const juce::SpinLock::ScopedLockType lock(host.scalaLock);
        return { host.scalaText, host.kbmText, host.scalaDescription, host.scalaSlot,
                 host.scalaVersion.load(std::memory_order_relaxed) };
    }
    static bool retained(Cs80AudioProcessor& host, const Snapshot& before)
    {
        const auto after = capture(host);
        return after.scl == before.scl && after.kbm == before.kbm
            && after.description == before.description && after.version == before.version
            && after.table.pitch == before.table.pitch && after.table.mapped == before.table.mapped;
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
    auto hostOwner = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
    auto& host = *hostOwner;
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
        const auto beforeInvalid = ScalaMappingAccess::capture(host);
        for (const auto& invalid : { juce::String("invalid mapping"),
                juce::String("0\n0\n127\n60\n69\n1e99\n12\n"),
                juce::String("2147483648\n0\n127\n60\n69\n440.0\n12\n") })
            check(reopened.loadKeyboardMappingText(invalid).isNotEmpty()
                      && tuning.isEquivalentTo(savedTuning(host)) && ScalaMappingAccess::retained(host, beforeInvalid),
                  "a rejected KBM keeps text, description, table and publication version unchanged");
        check(reopened.loadKeyboardMappingText({}).isEmpty()
                  && savedTuning(host).getProperty("scl").toString() == scale
                  && savedTuning(host).getProperty("kbm").toString().isEmpty(),
              "an empty mapping restores the existing default-mapping semantics without losing the scale");
        check(reopened.loadKeyboardMappingText(mapping444).isEmpty(),
              "the audio-path fixture restores its explicit 444 Hz mapping");

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

    juce::MemoryBlock state;
    host.getStateInformation(state);
    auto restoredOwner = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("restored"));
    auto& restored = *restoredOwner;
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

    // Simulate dialog completion after the tuning session changed while it was open.
    for (int mutation = 0; mutation < 5; ++mutation)
    {
        check(host.loadScalaTuning(scale, mapping432).isEmpty(), "dialog fixture loads original tuning");
        Page page(host);
        const auto dialogRevision = host.scalaTuningRevision();
        if (mutation == 0) host.loadScalaTuning(replacementScale, mapping432);
        if (mutation == 1) host.loadScalaKeyboardMapping(mapping444);
        if (mutation == 2) { host.clearScalaTuning(); host.loadScalaTuning(scale, mapping432); }
        if (mutation == 3) host.clearScalaTuning();
        if (mutation == 4)
        {
            juce::MemoryBlock session; host.getStateInformation(session);
            host.setStateInformation(session.getData(), (int) session.getSize());
        }
        const auto changed = ScalaMappingAccess::capture(host);
        check(page.loadKeyboardMappingText(mapping432, dialogRevision).isNotEmpty()
                  && ScalaMappingAccess::retained(host, changed),
              "stale dialog mapping cannot mutate a replaced, remapped, cleared or restored tuning session");
    }
    check(host.loadScalaTuning(scale, mapping432).isEmpty(), "fresh dialog fixture loads scale");
    {
        Page page(host);
        const auto revision = host.scalaTuningRevision();
        check(page.loadKeyboardMappingText(mapping444, revision).isEmpty()
                  && savedTuning(host).getProperty("kbm").toString() == mapping444,
              "a mapping dialog for the unchanged current session succeeds");
    }

    {
        Page page(host);
        struct Probe final : juce::AudioProcessorParameter::Listener
        {
            int begins = 0, changes = 0, ends = 0;
            void parameterValueChanged(int, float) override { ++changes; }
            void parameterGestureChanged(int, bool begin) override { begin ? ++begins : ++ends; }
        };
        const struct { int id; const char* title; } controls[] {
            {P_unisonDetune, "Unison detune"}, {P_velocityCurve, "Velocity curve"},
            {P_pressureCurve, "Aftertouch curve"}, {P_pressureThreshold, "Aftertouch threshold"},
            {P_ribbonCc, "Ribbon CC number"}
        };
        for (const auto& control : controls)
        {
            juce::Slider* slider = nullptr;
            for (auto* child : page.getChildren())
                if (auto* candidate = dynamic_cast<juce::Slider*>(child); candidate && candidate->getTitle() == control.title)
                    slider = candidate;
            check(slider != nullptr, "Keyboard page exposes its attached parameter slider");
            if (!slider) continue;
            slider->setValue((slider->getMinimum()+slider->getMaximum())*.5, juce::sendNotificationSync);
            auto* parameter = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(control.id));
            Probe probe; parameter->addListener(&probe);
            const auto before = slider->getValue();
            check(slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && slider->getValue() > before
                      && probe.begins == 1 && probe.changes == 1 && probe.ends == 1,
                  "Keyboard page arrow edit emits one complete attached host gesture");
            probe = {}; slider->setEnabled(false); const auto disabledValue = slider->getValue();
            check(!slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && slider->getValue() == disabledValue
                      && probe.begins == 0 && probe.changes == 0 && probe.ends == 0,
                  "disabled Keyboard page slider rejects arrow edits");
            slider->setEnabled(true); probe = {};
            {
                juce::Slider::ScopedDragNotification gesture(*slider);
                const auto held = slider->getValue();
                check(!slider->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)) && slider->getValue() == held
                          && probe.begins == 1 && probe.changes == 0 && probe.ends == 0,
                      "Keyboard page arrow cannot alter an already active edit");
            }
            check(probe.begins == 1 && probe.ends == 1, "existing Keyboard page edit finishes balanced");
            parameter->removeListener(&probe);
        }
    }

    // Force the read -> write -> publish ordering of a concurrent session load.
    check(host.loadScalaTuning(scale, mapping432).isEmpty(), "version-guard fixture loads its original source");
    const auto original = ScalaMappingAccess::capture(host);
    check(ScalaMappingAccess::finish(host, original, mapping444).isEmpty(),
          "a version-checked mapping publication succeeds when its source is still current");
    const auto beforeMappingChange = ScalaMappingAccess::capture(host);
    check(host.loadScalaKeyboardMapping(mapping432).isEmpty(), "a concurrent mapping-only change publishes a new version");
    const auto latestMapping = ScalaMappingAccess::capture(host);
    check(ScalaMappingAccess::finish(host, beforeMappingChange, mapping444).isNotEmpty()
              && ScalaMappingAccess::retained(host, latestMapping),
          "a stale mapping cannot overwrite a newer mapping even when the SCL text is identical");
    const auto beforeReplacement = ScalaMappingAccess::capture(host);
    check(host.loadScalaTuning(replacementScale, mapping432).isEmpty(),
          "the forced interleaving replaces the source before stale publication");
    const auto replacement = savedTuning(host);
    const auto replacementSnapshot = ScalaMappingAccess::capture(host);
    check(ScalaMappingAccess::finish(host, beforeReplacement, mapping444).isNotEmpty()
              && replacement.isEquivalentTo(savedTuning(host)) && ScalaMappingAccess::retained(host, replacementSnapshot),
          "the version guard rejects a stale mapping without overwriting the replacement scale");
    const auto beforeClear = ScalaMappingAccess::capture(host);
    host.clearScalaTuning();
    const auto cleared = ScalaMappingAccess::capture(host);
    check(ScalaMappingAccess::finish(host, beforeClear, mapping444).isNotEmpty()
              && ! host.hasScalaTuning() && ! savedTuning(host).isValid() && ScalaMappingAccess::retained(host, cleared),
          "the version guard rejects a stale mapping without resurrecting a cleared scale");
}
} // namespace cs80::tests

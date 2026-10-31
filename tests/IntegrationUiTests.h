#pragma once

// Integration wiring of the eight workstreams in the editor, run by CS80Snapshots --self-test
// (src/tools/ui_snapshot.cpp):
//   * ws3's Keyboard/Tuning page drives the processor's Scala API and the ws3 parameters;
//   * the Studio view mounts ws5's modulation/keyboard/FX pages and the tuning page beside the
//     built-in pages;
//   * the toolbar's A/B, prev/next, INIT and RANDOMISE actions reach ws6's library and ws2's
//     init patch, and A/B and randomise are one undo step each.

#include "plugin/KeyboardTuningPage.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/StudioToolbar.h"
#include "plugin/StudioView.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace cs80::tests
{
namespace integration_ui_detail
{
template <typename T>
T* findFirstOf(juce::Component& root)
{
    if (auto* hit = dynamic_cast<T*>(&root))
        return hit;
    for (auto* child : root.getChildren())
        if (auto* hit = findFirstOf<T>(*child))
            return hit;
    return nullptr;
}
} // namespace integration_ui_detail

template <typename Check, typename Flush>
void testIntegrationUi(Check check, Flush flush)
{
    using namespace integration_ui_detail;

    // ---- Keyboard/Tuning page -------------------------------------------------------------
    {
        juce::TemporaryFile root("-cs80-tuning-ui");
        Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
        host.prepareToPlay(48000.0, 256);
        cs80::ui::modern_layer::KeyboardTuningPage page(host);
        page.setSize(1100, 560);
        flush(20);
        check(page.tuningText().contains("no scale loaded"), "tuning page starts with no Scala scale");
        const juce::String scale = "! inline.scl\nInline three-note test scale\n3\n400.0\n700.0\n2/1\n";
        check(page.loadScalaText(scale).isEmpty(), "tuning page loads a valid Scala scale");
        check(page.tuningText().contains("Inline three-note test scale")
                  && host.getScalaDescription().contains("Inline three-note test scale"),
              "tuning page shows the processor's Scala description");
        check(page.loadScalaText("not a scale").isNotEmpty()
                  && host.getScalaDescription().contains("Inline three-note test scale"),
              "an invalid Scala file reports its reason and keeps the previous tuning");
        check(page.statusText().contains("MTS-ESP"), "tuning page reports the MTS-ESP state");
        page.tuningModeBox().setSelectedItemIndex(1, juce::sendNotificationSync);
        page.voiceModeBox().setSelectedItemIndex(2, juce::sendNotificationSync);
        flush(10);
        check(host.getParamValue(cs80::P_tuningMode) == 1.0f && host.getParamValue(cs80::P_voiceMode) == 2.0f,
              "tuning page selectors write tuning_mode and voice_mode");
        page.clearTuning();
        check(host.getScalaDescription().isEmpty() && page.tuningText().contains("no scale loaded"),
              "tuning page clears the Scala scale");
    }

    // ---- Editor wiring --------------------------------------------------------------------
    juce::TemporaryFile root("-cs80-integration-ui");
    Cs80AudioProcessor processor(root.getFile().getChildFile("presets"));
    processor.prepareToPlay(48000.0, 256);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flush(60);

    auto* studio = findFirstOf<cs80::ui::StudioView>(editor);
    check(studio != nullptr, "the editor has a Studio view");
    if (studio != nullptr)
    {
        using Tab = cs80::ui::StudioTab;
        auto hasPage = [studio](Tab tab, auto* type) {
            using T = std::remove_pointer_t<decltype(type)>;
            for (int s = 0; s < studio->getNumSubPages(tab); ++s)
                if (dynamic_cast<T*>(studio->getSubPage(tab, s)) != nullptr)
                    return true;
            return false;
        };
        check(studio->getNumSubPages(Tab::modulation) == 3
                  && hasPage(Tab::modulation, (cs80::ui::modern_layer::ModernModulationPage*) nullptr),
              "Studio MODULATION mounts the MSEG and matrix page after its built-in pages");
        check(studio->getNumSubPages(Tab::keyboard) == 3
                  && hasPage(Tab::keyboard, (cs80::ui::modern_layer::KeyboardTuningPage*) nullptr)
                  && hasPage(Tab::keyboard, (cs80::ui::modern_layer::KeyboardModulationPage*) nullptr),
              "Studio KEYBOARD mounts the tuning and keyboard-curve pages");
        check(studio->getNumSubPages(Tab::fx) == 2 && hasPage(Tab::fx, (cs80::ui::modern_layer::ModernFxPage*) nullptr),
              "Studio FX mounts the modern FX page");
    }

    auto* toolbar = findFirstOf<cs80::ui::StudioToolbar>(editor);
    check(toolbar != nullptr, "the editor has the Studio toolbar");
    if (toolbar == nullptr)
        return;
    const auto& actions = toolbar->getActions();
    auto& history = processor.getUndoManager();

    // A/B: the toolbar uses the processor's compare slots (ws6), one undo step per switch.
    check((bool) actions.selectCompare && (bool) actions.activeCompare && (bool) actions.copyCompare,
          "toolbar A/B actions are wired");
    if (actions.selectCompare && actions.activeCompare)
    {
        check(actions.activeCompare() == processor.getAbSlot() && processor.getAbSlot() == 0, "A is the open slot");
        actions.selectCompare(1);                      // first switch: B starts as a copy of A
        flush(10);
        check(processor.getAbSlot() == 1 && processor.isAbSlotStored(0), "selecting B uses the processor's A/B slots");
        processor.setParamValue(cs80::P_brilliance, 0.37f);
        actions.selectCompare(0);
        flush(30);
        check(processor.getAbSlot() == 0 && std::abs(processor.getParamValue(cs80::P_brilliance) - 0.37f) > 1.0e-4f,
              "returning to A recalls A's sound");
        check(history.canUndo() && history.getUndoDescription().contains("Compare"), "an A/B switch is an undo step");
    }

    // RANDOMISE: one undo step that brings back the sound before it.
    check((bool) actions.randomise, "toolbar menu offers randomise within original ranges");
    if (actions.randomise)
    {
        const auto before = processor.currentValues();
        actions.randomise();
        flush(30);
        const auto after = processor.currentValues();
        bool changed = false;
        for (int i = 0; i < cs80::P_numParams; ++i)
            changed = changed || after[i] != before[i];
        check(changed && history.getUndoDescription() == "Randomise", "randomise is one named undo step");
        processor.undo();
        flush(30);
        bool restored = true;
        for (int i = 0; i < cs80::P_numParams; ++i)
            restored = restored && std::abs(processor.currentValues()[i] - before[i]) <= 1.0e-5f;
        check(restored, "one undo brings back the sound before randomise");
    }

    // INIT: ws2's new-preset path (the circuit engine with the original SUB panel).
    if (actions.initPatch)
    {
        processor.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
        actions.initPatch();
        flush(20);
        check(processor.getEngineMode() == Cs80AudioProcessor::EngineMode::circuit
                  && processor.getParamValue(cs80::P_subOriginalMode) == 1.0f,
              "INIT loads the init patch on the circuit engine with the original SUB");
    }

    // Prev/next: through the library's filter (ws6), not the raw host list.
    if (actions.stepPreset)
    {
        const int start = processor.getCurrentPresetIndex();
        actions.stepPreset(1);
        flush(30);
        const int stepped = processor.getCurrentPresetIndex();
        check(stepped != start, "NEXT loads another program");
        actions.stepPreset(-1);
        flush(30);
        check(processor.getCurrentPresetIndex() != stepped, "PREVIOUS steps back through the same filter");
    }
}
} // namespace cs80::tests

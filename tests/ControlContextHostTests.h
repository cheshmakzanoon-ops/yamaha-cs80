#pragma once

// Host/UI contract for the parameter-context menus on panel switches, Studio
// choices and independent memory miniatures. JUCE's Button/Combo attachments
// already bracket ordinary edits with a complete host gesture; menu-driven
// edits must do the same. The menu is modern UI, not a DSP feature: PURE CS-80
// keeps all learn mappings inactive and may still edit original panel memories.
// Run by CS80Snapshots in its existing headless JUCE UI smoke suite.

#include "plugin/KeyboardTuningPage.h"
#include "plugin/MemoryLid.h"
#include "plugin/UiComponents.h"

namespace cs80::tests::control_context
{
using namespace cs80::tests::studio;

// Display-server-independent part of the same host contract. This runs on
// Linux environments that intentionally do not permit an X11 socket. Native
// pointer dispatch and editor composition stay in testParameterMenus below
// and still run in the Xvfb-backed UI CI job; no gate is skipped or weakened.
inline void testParameterMenusHeadless(const Check& check)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    const int hpIndex = cs80::chParam(0, cs80::CP_hpfOn);
    const auto hpId = Cs80AudioProcessor::parameterId(hpIndex);
    cs80::ui::LedButton hpf("HPF");
    cs80::ui::bindParameterContextMenu(hpf, processor, hpId, hpIndex);
    check(hpf.getComponentID() == hpId && static_cast<bool>(hpf.onContextMenu),
          "headless LED button has a real APVTS context-menu binding");

    auto* p = processor.getAPVTS().getParameter(hpId);
    if (p != nullptr)
    {
        juce::ButtonParameterAttachment attachment(*p, hpf);
        StudioGestureProbe probe;
        p->addListener(&probe);
        hpf.setToggleState(! hpf.getToggleState(), juce::sendNotificationSync);
        p->removeListener(&probe);
        check(probe.oneGesture(), "headless APVTS LED click has exactly one balanced host gesture");
    }

    auto menu = cs80::ui::buildParameterContextMenu(processor, hpId, hpf, hpIndex);
    const auto* reset = menuItem(menu, "Reset to default");
    const auto* learn = menuItem(menu, "MIDI learn");
    check(reset != nullptr && reset->action && learn != nullptr && learn->action,
          "headless parameter context menu exposes reset and MIDI learn");
    if (p != nullptr && reset != nullptr && reset->action)
    {
        processor.setParamValue(hpIndex, 1.0f - cs80::paramDef(hpIndex).def);
        StudioGestureProbe probe;
        p->addListener(&probe);
        reset->action();
        p->removeListener(&probe);
        check(probe.oneGesture() && p->getValue() == p->getDefaultValue(),
              "headless menu reset sends one balanced host begin/end gesture and defaults the parameter");
    }
    if (learn != nullptr && learn->action)
    {
        learn->action();
        check(processor.getMidiLearn().learningParameter() == processor.getParameters().indexOf(p),
              "headless LED MIDI learn targets the exact APVTS host parameter");
        processor.getMidiLearn().cancelLearn();
    }

    cs80::ui::ContextComboBox tuning;
    const auto tuningId = Cs80AudioProcessor::parameterId(cs80::P_tuningMode);
    tuning.addItemList({ "Equal temperament", "Scala / KBM", "MTS-ESP" }, 1);
    cs80::ui::bindParameterContextMenu(tuning, processor, tuningId, cs80::P_tuningMode);
    check(tuning.getComponentID() == tuningId && static_cast<bool>(tuning.onContextMenu),
          "headless combo exposes the same bound context-menu callback");
    if (auto* tuningParam = processor.getAPVTS().getParameter(tuningId))
    {
        juce::ComboBoxParameterAttachment attachment(*tuningParam, tuning);
        StudioGestureProbe probe;
        tuningParam->addListener(&probe);
        tuning.setSelectedId(tuning.getSelectedId() == 2 ? 1 : 2, juce::sendNotificationSync);
        tuningParam->removeListener(&probe);
        check(probe.oneGesture(), "headless APVTS combo selection has exactly one balanced host gesture");
    }
    auto comboMenu = cs80::ui::buildParameterContextMenu(processor, tuningId, tuning, cs80::P_tuningMode);
    check(menuItem(comboMenu, "MIDI learn") != nullptr,
          "headless combo builds a MIDI-learn menu without requiring a display server");

    cs80::ui::ContextToggleButton scanner("KAS scan timing");
    const auto scannerId = Cs80AudioProcessor::parameterId(cs80::P_kasScanTiming);
    cs80::ui::bindParameterContextMenu(scanner, processor, scannerId, cs80::P_kasScanTiming);
    auto scannerMenu = cs80::ui::buildParameterContextMenu(processor, scannerId, scanner, cs80::P_kasScanTiming);
    check(static_cast<bool>(scanner.onContextMenu) && menuItem(scannerMenu, "MIDI learn") != nullptr,
          "headless native-looking toggle switch retains its parameter context menu");
    if (auto* scannerParam = processor.getAPVTS().getParameter(scannerId))
    {
        juce::ButtonParameterAttachment attachment(*scannerParam, scanner);
        StudioGestureProbe probe;
        scannerParam->addListener(&probe);
        scanner.setToggleState(! scanner.getToggleState(), juce::sendNotificationSync);
        scannerParam->removeListener(&probe);
        check(probe.oneGesture(), "headless standard toggle sends exactly one complete host gesture");
    }

    // Miniature memories are appended as 104 independent host parameters, not
    // aliases for the two rank controls. Check all four bank names, not just
    // the first example exercised by the native MemoryLid integration gate.
    int hostBank[4] = { -1, -1, -1, -1 };
    for (int bank = 0; bank < 4; ++bank)
    {
        const auto id = Cs80AudioProcessor::memoryParameterId(bank, cs80::CP_saw);
        juce::Component miniature;
        auto memoryMenu = cs80::ui::buildParameterContextMenu(
            processor, id, miniature, cs80::chParam(bank % 2, cs80::CP_saw));
        const auto* arm = menuItem(memoryMenu, "MIDI learn");
        auto* memoryParameter = processor.getAPVTS().getParameter(id);
        hostBank[bank] = processor.getParameters().indexOf(memoryParameter);
        if (arm != nullptr && arm->action) arm->action();
        check(arm != nullptr && arm->action && hostBank[bank] >= 0
              && processor.getMidiLearn().learningParameter() == hostBank[bank],
              "headless memory MIDI learn uses the selected bank's independent host index");
        processor.getMidiLearn().cancelLearn();
    }
    check(hostBank[0] != hostBank[1] && hostBank[1] != hostBank[2]
          && hostBank[2] != hostBank[3], "four miniature bank host indices are unique");

    // Async popup actions must check policy at activation, not only when the
    // menu was built: the player can switch to PURE while the menu is open.
    auto staleMenu = cs80::ui::buildParameterContextMenu(processor, hpId, hpf, hpIndex);
    const auto* staleLearn = menuItem(staleMenu, "MIDI learn");
    processor.setParamValue(cs80::P_parityMode, 1.0f);
    if (staleLearn != nullptr && staleLearn->action) staleLearn->action();
    check(staleLearn != nullptr && staleLearn->action
          && processor.getMidiLearn().learningParameter() < 0,
          "MIDI learn callback from a stale popup remains inert after switching into PURE");
    auto pureMenu = cs80::ui::buildParameterContextMenu(processor, hpId, hpf, hpIndex);
    const auto* pureLearn = menuItem(pureMenu, "MIDI learn");
    check(pureLearn != nullptr && ! pureLearn->isEnabled,
          "PURE CS-80 keeps MIDI learn disabled even on switches and combos");
}

inline void testParameterMenus(const Check& check, const Flush& flush)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flush(50);

    const int hpIndex = cs80::chParam(0, cs80::CP_hpfOn);
    const auto hpId = Cs80AudioProcessor::parameterId(hpIndex);
    auto* panelSwitch = dynamic_cast<cs80::ui::LedButton*>(findById(editor, hpId));
    check(panelSwitch != nullptr && static_cast<bool>(panelSwitch->onContextMenu),
          "the main-panel HPF switch exposes a bound context menu");
    if (panelSwitch == nullptr) return;

    // A popup-button event never toggles the rocker, even if its mouse-up has
    // no right-button modifier (as JUCE mouse sources commonly report).
    int switchPopups = 0;
    auto originalSwitchMenu = panelSwitch->onContextMenu;
    panelSwitch->onContextMenu = [&switchPopups] { ++switchPopups; };
    const bool wasOn = panelSwitch->getToggleState();
    panelSwitch->mouseDown(mouseAt(*panelSwitch, { 10.0f, 10.0f },
                                   juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier)));
    panelSwitch->mouseDrag(mouseAt(*panelSwitch, { 12.0f, 10.0f },
                                   juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier)));
    panelSwitch->mouseUp(mouseAt(*panelSwitch, { 10.0f, 10.0f }, {}));
    check(switchPopups == 1 && panelSwitch->getToggleState() == wasOn && ! panelSwitch->isDown(),
          "right-click/drag on a LED switch opens only the menu, never changes or latches its press state");
    panelSwitch->onContextMenu = std::move(originalSwitchMenu);

    auto menu = cs80::ui::buildParameterContextMenu(processor, hpId, *panelSwitch, hpIndex);
    const auto* learnItem = menuItem(menu, "MIDI learn");
    const auto* resetItem = menuItem(menu, "Reset to default");
    check(learnItem != nullptr && learnItem->isEnabled && learnItem->action
          && resetItem != nullptr && resetItem->isEnabled && resetItem->action,
          "the LED switch offers MIDI learn and a reset through the host parameter");
    auto* hpParameter = processor.getAPVTS().getParameter(hpId);
    if (hpParameter != nullptr && resetItem != nullptr && resetItem->action)
    {
        processor.setParamValue(hpIndex, 1.0f - cs80::paramDef(hpIndex).def);
        flush(30);
        processor.getUndoManager().clearUndoHistory();
        StudioGestureProbe probe;
        hpParameter->addListener(&probe);
        resetItem->action();
        hpParameter->removeListener(&probe);
        flush(30);
        check(probe.oneGesture() && processor.getUndoManager().getUndoDescriptions().size() == 1,
              "menu reset is exactly one balanced host begin/end gesture and one undo step");
    }
    if (learnItem != nullptr && learnItem->action)
    {
        learnItem->action();
        const int host = processor.hostParameterIndex(hpIndex);
        check(processor.getMidiLearn().learningParameter() == host,
              "LED switch MIDI learn arms its exact host parameter index");
        auto armedMenu = cs80::ui::buildParameterContextMenu(processor, hpId, *panelSwitch, hpIndex);
        const auto* cancel = menuItem(armedMenu, "Cancel MIDI learn");
        check(cancel != nullptr && cancel->action, "armed LED menu offers cancellation");
        if (cancel != nullptr && cancel->action) cancel->action();
        check(processor.getMidiLearn().learningParameter() < 0, "MIDI learn cancellation disarms the map");
    }

    // The Studio tuning source is a real APVTS ComboBox attachment. A popup
    // must not open/select its normal list, while a normal selection must still
    // be the one balanced host gesture that JUCE promises.
    cs80::ui::modern_layer::KeyboardTuningPage keyboard(processor);
    auto* combo = dynamic_cast<cs80::ui::ContextComboBox*>(&keyboard.tuningModeBox());
    check(combo != nullptr && static_cast<bool>(combo->onContextMenu),
          "the Studio tuning combo exposes a bound context menu");
    if (combo != nullptr)
    {
        int comboPopups = 0;
        auto originalComboMenu = combo->onContextMenu;
        combo->onContextMenu = [&comboPopups] { ++comboPopups; };
        const int prior = combo->getSelectedId();
        combo->mouseDown(mouseAt(*combo, { 10.0f, 10.0f },
                                 juce::ModifierKeys(juce::ModifierKeys::rightButtonModifier)));
        combo->mouseUp(mouseAt(*combo, { 10.0f, 10.0f }, {}));
        check(comboPopups == 1 && combo->getSelectedId() == prior,
              "right-click on a combo opens only its context menu");
        combo->onContextMenu = std::move(originalComboMenu);

        const auto id = Cs80AudioProcessor::parameterId(cs80::P_tuningMode);
        auto comboMenu = cs80::ui::buildParameterContextMenu(processor, id, *combo, cs80::P_tuningMode);
        check(menuItem(comboMenu, "MIDI learn") != nullptr && menuItem(comboMenu, "Reset to default") != nullptr,
              "combo menu offers MIDI learn and reset");
        if (auto* param = processor.getAPVTS().getParameter(id))
        {
            StudioGestureProbe probe;
            param->addListener(&probe);
            combo->setSelectedId(prior == 2 ? 1 : 2, juce::sendNotificationSync);
            param->removeListener(&probe);
            check(probe.oneGesture(), "normal combo selection is exactly one balanced host gesture");
        }
    }

    // The memory lid binds 4 x 26 independent APVTS parameters. Its SAW tab
    // and a miniature cap-fader both need the same menu/learn contract rather
    // than falling back to a channel's main-panel host parameter.
    cs80::panel::MemoryLid lid(processor);
    lid.setSize(1504, 590);
    const auto memorySwitchId = Cs80AudioProcessor::memoryParameterId(0, cs80::CP_saw);
    const auto memorySliderId = Cs80AudioProcessor::memoryParameterId(0, cs80::CP_vA);
    auto* memorySwitch = dynamic_cast<cs80::ui::LedButton*>(findById(lid, memorySwitchId));
    auto* memorySlider = dynamic_cast<cs80::ui::FocusSlider*>(findById(lid, memorySliderId));
    check(memorySwitch != nullptr && static_cast<bool>(memorySwitch->onContextMenu)
          && memorySlider != nullptr && static_cast<bool>(memorySlider->onContextMenu),
          "memory-lid miniature switch and fader expose menus");
    if (memorySlider != nullptr)
    {
        auto memoryMenu = cs80::ui::buildParameterContextMenu(
            processor, memorySliderId, *memorySlider, cs80::chParam(0, cs80::CP_vA));
        const auto* learn = menuItem(memoryMenu, "MIDI learn");
        if (learn != nullptr && learn->action)
        {
            learn->action();
            auto* param = processor.getAPVTS().getParameter(memorySliderId);
            check(param != nullptr && processor.getMidiLearn().learningParameter() == processor.getParameters().indexOf(param),
                  "miniature MIDI learn arms its independent memory host parameter");
            processor.getMidiLearn().cancelLearn();
        }
        else check(false, "memory miniature's MIDI learn menu action exists");
    }

    processor.setParamValue(cs80::P_parityMode, 1.0f);
    auto pureMenu = cs80::ui::buildParameterContextMenu(processor, hpId, *panelSwitch, hpIndex);
    const auto* pureLearn = menuItem(pureMenu, "MIDI learn");
    check(pureLearn != nullptr && ! pureLearn->isEnabled,
          "PURE CS-80 disables switch MIDI learn without removing physical controls");
}
} // namespace cs80::tests::control_context

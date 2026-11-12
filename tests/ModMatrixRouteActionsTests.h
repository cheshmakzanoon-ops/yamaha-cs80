#pragma once

#include "plugin/ModernModulationUi.h"
#include "plugin/PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace cs80::tests
{
namespace matrix_actions_detail
{
using namespace cs80::modern;
using cs80::ui::modern_layer::ModMatrixPage;
inline juce::PopupMenu::Item action(const juce::PopupMenu& menu, int destination = -1)
{
    const auto name = destination < 0 ? juce::String("Clear route and amount")
        : "Copy route + amount to slot " + juce::String(destination + 1);
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text == name) return it.getItem();
    return {};
}
struct Probe : juce::AudioProcessorParameter::Listener
{
    std::vector<std::pair<int, char>> events;
    void parameterValueChanged(int index, float) override { events.emplace_back(index, 'V'); }
    void parameterGestureChanged(int index, bool start) override { events.emplace_back(index, start ? 'B' : 'E'); }
    bool matches(int index, bool changed) const
    {
        return changed ? events == std::vector<std::pair<int, char>>{{index, 'B'}, {index, 'V'}, {index, 'E'}} : events.empty();
    }
};
}

// Actual JUCE component/menu callbacks with no desktop peer or OS popup.
template<class Check>
void testModMatrixRouteActions(Check check, const juce::File& snapshot = {})
{
    using namespace matrix_actions_detail;
    juce::TemporaryFile root("-cs80-matrix-actions");
    auto host = std::make_unique<Cs80AudioProcessor>(root.getFile());
    auto& layer = host->getModernLayer();
    auto& undo = host->getUndoManager();
    ModMatrixPage page(*host);
    auto parameter = [&](int i) { return host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(i)); };
    auto values = [&]
    {
        std::vector<float> result;
        for (auto* p : host->getParameters()) result.push_back(p->getValue());
        return result;
    };
    Probe probe;
    for (auto* p : host->getParameters()) p->addListener(&probe);
    auto setup = [&]
    {
        page.setEnabled(true);
        host->setPure(false);
        ModernConfig c;
        for (int i = 0; i < kModSlots; ++i)
        {
            c.slots[(size_t) i] = {ModSource(int(ModSource::lfo1) + i % 3), ModSource::modWheel,
                                  i % 2 ? ModDestination::pitch : ModDestination::fx1Mix, ModTarget(i % 3)};
            host->setParamValue(P_mod1Amount + i, float(i - 8) / 10.f);
        }
        c.msegs[1].points[0].level = .375f;
        layer.setConfig(c);
        host->getUndoableState().syncNow();
        undo.clearUndoHistory();
        probe.events.clear();
    };
    auto invoke = [&](const juce::PopupMenu::Item& item) { if (item.action) item.action(); };
    for (int source = 0; source < kModSlots; ++source)
      for (int destination = 0; destination < kModSlots; ++destination)
    {
        if (source == destination) continue;
        setup();
        const auto before = layer.getConfig();
        auto expected = before;
        expected.slots[(size_t) destination] = before.slots[(size_t) source];
        const auto beforeValues = values();
        auto expectedValues = beforeValues;
        const int sourceIndex = host->hostParameterIndex(P_mod1Amount + source);
        const int destinationIndex = host->hostParameterIndex(P_mod1Amount + destination);
        expectedValues[(size_t) destinationIndex] = beforeValues[(size_t) sourceIndex];
        const auto menu = page.buildRouteActionsMenu(source);
        const auto item = action(menu, destination);
        check(item.isEnabled && bool(item.action) && !action(menu, source).action, "copy menu offers each other slot, never self-copy");
        check(layer.getConfig() == before && values() == beforeValues && !undo.canUndo() && probe.events.empty(),
              "opening menu is read-only and silent to host");
        invoke(item);
        check(layer.getConfig() == expected && values() == expectedValues,
              "all ordered copies replace all route fields and signed amount, preserving source, other parameters and MSEGs");
        check(probe.matches(destinationIndex, true), "copy emits exactly one balanced host gesture for destination amount only");
        check(undo.getUndoDescription() == "Copy matrix slot " + juce::String(source + 1) + " to slot " + juce::String(destination + 1)
                  && undo.getNumActionsInCurrentTransaction() == 0, "copy owns a named closed undo transaction");
        probe.events.clear();
        check(host->undo() && layer.getConfig() == before && values() == beforeValues && !undo.canUndo(),
              "one undo restores routing and amount together");
        check(probe.matches(destinationIndex, true), "undo emits one balanced amount gesture");
        probe.events.clear();
        invoke(item);
        check(layer.getConfig() == before && values() == beforeValues && undo.canRedo() && probe.events.empty(),
              "consumed callback cannot repeat after undo");
        check(host->redo() && layer.getConfig() == expected && values() == expectedValues && !undo.canRedo(),
              "one redo restores complete copied route");
        check(probe.matches(destinationIndex, true), "redo emits one balanced amount gesture");
    }
    for (int source = 0; source < kModSlots; ++source)
    {
        setup();
        const auto before = layer.getConfig();
        const auto beforeValues = values();
        auto expected = before; expected.slots[(size_t) source] = {};
        auto expectedValues = beforeValues;
        const int index = host->hostParameterIndex(P_mod1Amount + source);
        expectedValues[(size_t) index] = parameter(P_mod1Amount)->convertTo0to1(0.f);
        const bool changedAmount = expectedValues[(size_t) index] != beforeValues[(size_t) index];
        const auto item = action(page.buildRouteActionsMenu(source));
        check(item.isEnabled, "clear enabled for a non-default route, including zero amount");
        invoke(item);
        check(layer.getConfig() == expected && values() == expectedValues && probe.matches(index, changedAmount),
              "clear resets all four fields and amount without touching other state; no redundant amount gesture");
        check(undo.getUndoDescription() == "Clear matrix slot " + juce::String(source + 1)
                  && undo.getNumActionsInCurrentTransaction() == 0, "clear owns a named closed undo transaction");
        check(host->undo() && layer.getConfig() == before && values() == beforeValues && !undo.canUndo(), "clear undoes in one step");
        check(host->redo() && layer.getConfig() == expected && values() == expectedValues, "clear redoes in one step");
    }
    // Same routing but different amount, and different routing with same amount.
    for (bool amountOnly : {false, true})
    {
        setup();
        if (amountOnly) layer.setSlot(1, layer.getSlot(0));
        else host->setParamValue(P_mod1Amount + 1, host->getParamValue(P_mod1Amount));
        host->getUndoableState().syncNow(); undo.clearUndoHistory(); probe.events.clear();
        const auto before = layer.getConfig(); const auto beforeValues = values();
        invoke(action(page.buildRouteActionsMenu(0), 1));
        check(layer.getSlot(0) == layer.getSlot(1) && parameter(P_mod1Amount)->getValue() == parameter(P_mod1Amount + 1)->getValue(),
              "either routing-only or amount-only difference is actionable");
        check(probe.matches(host->hostParameterIndex(P_mod1Amount + 1), amountOnly), "metadata-only copy does not gesture or notify amount");
        check(host->undo() && layer.getConfig() == before && values() == beforeValues && !undo.canUndo(), "partial-domain action still undoes once");
    }
    // Disabled identical copy/clear and invalid indices must preserve an existing redo branch.
    setup();
    layer.setSlot(1, layer.getSlot(0));
    host->setParamValue(P_mod1Amount + 1, host->getParamValue(P_mod1Amount));
    layer.setSlot(2, {}); host->setParamValue(P_mod1Amount + 2, 0.f);
    host->getUndoableState().syncNow(); undo.clearUndoHistory();
    invoke(action(page.buildRouteActionsMenu(0), 3)); host->undo(); probe.events.clear();
    const auto noOpConfig = layer.getConfig(); const auto noOpValues = values();
    const auto identical = action(page.buildRouteActionsMenu(0), 1);
    const auto empty = action(page.buildRouteActionsMenu(2));
    check(!identical.isEnabled && !empty.isEnabled, "identical copy and already-clear slot are disabled");
    invoke(identical); invoke(empty);
    check(page.buildRouteActionsMenu(-1).getNumItems() == 0 && page.buildRouteActionsMenu(kModSlots).getNumItems() == 0,
          "out-of-range source creates no actions");
    check(layer.getConfig() == noOpConfig && values() == noOpValues && undo.canRedo() && !undo.canUndo() && probe.events.empty(),
          "no-op callbacks preserve state, notifications and redo branch");

    auto rejected = [&](const juce::PopupMenu::Item& item, const char* name)
    {
        const auto c = layer.getConfig(); const auto v = values();
        const bool canUndo = undo.canUndo(), canRedo = undo.canRedo();
        probe.events.clear(); invoke(item);
        check(layer.getConfig() == c && values() == v && undo.canUndo() == canUndo && undo.canRedo() == canRedo && probe.events.empty(), name);
    };
    // Both copy and clear reject stale metadata, amount ABA, empty gestures,
    // session redirection, disabled/PURE states, newer menus and dead owners.
    for (bool clear : {false, true})
      for (int scenario = 0; scenario < 13; ++scenario)
    {
        setup();
        const auto item = action(page.buildRouteActionsMenu(0), clear ? -1 : 1);
        const auto original = layer.getConfig();
        auto* p = parameter(P_mod1Amount + (scenario % 2));
        const float amount = p->getValue();
        if (scenario == 0 || scenario == 1)
        {
            auto changed = layer.getSlot(scenario); changed.via = ModSource::expression;
            layer.setSlot(scenario, changed); layer.setConfig(original);
        }
        else if (scenario == 2 || scenario == 3)
        {
            p->setValueNotifyingHost(.93f); p->setValueNotifyingHost(amount);
        }
        else if (scenario == 4) { p->beginChangeGesture(); p->endChangeGesture(); }
        else if (scenario == 5) { page.setEnabled(false); page.setEnabled(true); }
        else if (scenario == 6) { host->setPure(true); host->setPure(false); }
        else if (scenario == 7) { page.buildRouteActionsMenu(5); }
        else if (scenario == 8)
        {
            juce::MemoryBlock saved; host->getStateInformation(saved);
            host->setStateInformation(saved.getData(), int(saved.getSize()));
        }
        else if (scenario == 9) { p->beginChangeGesture(); }
        else if (scenario == 10)
        {
            std::thread automation([&] { p->setValueNotifyingHost(.93f); p->setValueNotifyingHost(amount); });
            automation.join();
        }
        else if (scenario == 11) { page.setEnabled(false); }
        else { host->setPure(true); }
        rejected(item, "copy and clear reject stale/ABA metadata, parameters, gestures, state restore, automation, PURE, disabled and newer menus");
        if (scenario == 9) p->endChangeGesture();
    }
    for (int mode = 0; mode < 4; ++mode)
    {
        setup();
        auto* p = parameter(P_mod1Amount);
        std::unique_ptr<Cs80AudioProcessor::ScopedUserEdit> scope;
        if (mode == 0) page.setEnabled(false);
        if (mode == 1) host->setPure(true);
        if (mode == 2) p->beginChangeGesture();
        if (mode == 3) scope = std::make_unique<Cs80AudioProcessor::ScopedUserEdit>(host->getUndoableState(), "outer edit");
        const auto item = action(page.buildRouteActionsMenu(0), 1);
        check(!item.isEnabled, "menu opened while disabled, PURE, gesturing or in another user edit is unavailable");
        page.setEnabled(true); host->setPure(false); if (mode == 2) p->endChangeGesture(); scope.reset();
        rejected(item, "initially rejected menu cannot become authorized later");
    }
    setup();
    auto* p = parameter(P_mod1Amount);
    p->beginChangeGesture();
    {
        ModMatrixPage duringGesture(*host);
        check(!action(duringGesture.buildRouteActionsMenu(0), 1).isEnabled, "page constructed during an earlier gesture still blocks route actions");
        p->endChangeGesture();
        check(action(duringGesture.buildRouteActionsMenu(0), 1).isEnabled, "earlier gesture ending does not leave new page permanently blocked");
    }
    setup();
    juce::PopupMenu::Item dead;
    { ModMatrixPage temporary(*host); dead = action(temporary.buildRouteActionsMenu(0), 1); }
    rejected(dead, "retained callback is harmless after page destruction");

    // Host listeners can close an editor synchronously inside any amount event.
    // The admitted action still finishes against the surviving processor.
    for (char event : {'B', 'V', 'E'})
    {
        setup();
        const auto before = layer.getConfig(); const auto beforeValues = values();
        auto victim = std::make_unique<ModMatrixPage>(*host);
        const auto item = action(victim->buildRouteActionsMenu(0), 1);
        struct Closer : juce::AudioProcessorParameter::Listener
        {
            char event;
            std::function<void()> close;
            void parameterValueChanged(int, float) override { if (event == 'V') close(); }
            void parameterGestureChanged(int, bool start) override
            { if (event == (start ? 'B' : 'E')) close(); }
        } closer;
        closer.event = event; closer.close = [&] { victim.reset(); };
        auto* amount = parameter(P_mod1Amount + 1);
        amount->addListener(&closer);
        invoke(item);
        amount->removeListener(&closer);
        check(victim == nullptr && layer.getSlot(1) == before.slots[0]
                  && amount->getValue() == parameter(P_mod1Amount)->getValue(),
              "route copy safely completes when a host amount begin/value/end listener destroys the page");
        check(host->undo() && layer.getConfig() == before && values() == beforeValues && !undo.canUndo(),
              "action whose page closed reentrantly still has one complete undo");
    }

    // Pending automation has not yet reached the APVTS tree when this menu opens.
    setup();
    host->setParamValue(P_mod1Amount + 1, .23f);
    const auto pending = values(); const auto pendingConfig = layer.getConfig();
    invoke(action(page.buildRouteActionsMenu(0), 1));
    check(host->undo() && values() == pending && layer.getConfig() == pendingConfig && !undo.canUndo(),
          "undo restores the latest pending destination amount rather than a stale APVTS tree value");

    // Consecutive menu actions are distinct even before the message loop runs.
    setup(); const auto start = layer.getConfig(); const auto startValues = values();
    invoke(action(page.buildRouteActionsMenu(0), 1));
    const auto first = layer.getConfig(); const auto firstValues = values();
    invoke(action(page.buildRouteActionsMenu(0)));
    const auto second = layer.getConfig(); const auto secondValues = values();
    check(host->undo() && layer.getConfig() == first && values() == firstValues, "first undo reverts only clear");
    check(host->undo() && layer.getConfig() == start && values() == startValues && !undo.canUndo(), "second undo reverts only copy");
    check(host->redo() && host->redo() && layer.getConfig() == second && values() == secondValues, "two redos restore both separate actions");

    setup();
    const auto opening = host->capturePresetBrowserSound();
    invoke(action(page.buildRouteActionsMenu(0), 15));
    invoke(action(page.buildRouteActionsMenu(0)));
    const auto copied = layer.getConfig(); const auto copiedValues = values();
    juce::MemoryBlock saved; host->getStateInformation(saved);
    setup(); host->setStateInformation(saved.getData(), int(saved.getSize()));
    check(layer.getConfig() == copied && values() == copiedValues, "host state round-trip persists routing and amount together");
    const auto preset = host->saveUserPreset("Complete matrix routes");
    check(preset.wasSaved(), "complete routes can be saved as a user preset");
    setup();
    check(preset.wasSaved() && host->loadUserPreset(preset.file).wasOk()
              && layer.getConfig() == copied && values() == copiedValues,
          "user preset round-trip persists complete copied and cleared routes");
    check(host->restorePresetBrowserSound(opening) && layer.getConfig() == opening.modern
              && host->getParamValue(P_mod1Amount) == opening.values.v[P_mod1Amount]
              && host->getParamValue(P_mod16Amount) == opening.values.v[P_mod16Amount],
          "browser REVERT restores pre-action routing and amounts using unchanged sound policy");
    // Offscreen layout checks, not a claim of OS popup accessibility/interaction.
    for (int width : {900, 1100})
    {
        page.setSize(width, 510);
        int buttons = 0;
        for (auto* child : page.getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*>(child))
            {
                ++buttons;
                check(button->getTitle().isNotEmpty() && button->getWantsKeyboardFocus() && button->getWidth() >= 24 && button->getHeight() >= 24
                          && page.getLocalBounds().contains(button->getBounds()), "each row has a named in-bounds minimum-size Actions button");
            }
        check(buttons == kModSlots, "all sixteen route actions are visible and keyboard-focusable buttons");
    }
    if (snapshot != juce::File{})
    {
        setup(); layer.sendSynchronousChangeMessage();
        const auto image = page.createComponentSnapshot(page.getLocalBounds());
        juce::FileOutputStream stream(snapshot);
        check(stream.openedOk() && image.isValid(), "offscreen route-actions snapshot can be created");
        if (stream.openedOk() && image.isValid())
        {
            stream.setPosition(0); stream.truncate();
            check(juce::PNGImageFormat().writeImageToStream(image, stream), "offscreen route-actions snapshot writes as PNG");
        }
    }
    for (auto* parameter : host->getParameters()) parameter->removeListener(&probe);
}
} // namespace cs80::tests

#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/StudioToolbar.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace cs80::tests
{
namespace toolbar_menu_detail
{
using Toolbar = cs80::ui::StudioToolbar;
inline constexpr std::array<int, 18> ids { 1, 2, 14, 3, 4, 5, 10, 11, 12, 13, 20, 30, 31,
                                          175, 200, 225, 250, 300 };
using Counts = std::array<int, ids.size()>;

inline Toolbar::Actions actions(Counts& counts)
{
    Toolbar::Actions result;
    const std::array<std::function<void()> Toolbar::Actions::*, 13> members {
        &Toolbar::Actions::savePreset, &Toolbar::Actions::initPatch, &Toolbar::Actions::randomise,
        &Toolbar::Actions::copyCompare, &Toolbar::Actions::undo, &Toolbar::Actions::redo,
        &Toolbar::Actions::toggleScope, &Toolbar::Actions::toggleControllers, &Toolbar::Actions::toggleQwerty,
        &Toolbar::Actions::toggleCircuitNotes, &Toolbar::Actions::clearMidiLearn, &Toolbar::Actions::startTour,
        &Toolbar::Actions::showEvidence
    };
    for (size_t i = 0; i < members.size(); ++i)
        result.*members[i] = [&counts, i] { ++counts[i]; };
    result.selectCompare = [](int) {};
    result.canUndo = result.canRedo = [] { return true; };
    result.midiLearnCount = [] { return 1; };
    result.zoomPresets = { 75, 100, 125, 150, 200 };
    result.zoom = [] { return 100; };
    result.setZoom = [&counts](int percent)
    {
        for (size_t i = 13; i < ids.size(); ++i)
            if (ids[i] == 100 + percent) ++counts[i];
    };
    return result;
}

inline juce::PopupMenu::Item item(const juce::PopupMenu& menu, int id)
{
    for (juce::PopupMenu::MenuItemIterator it(menu, true); it.next();)
        if (it.getItem().itemID == id) return it.getItem();
    return {};
}

inline std::vector<std::function<void()>> retain(const juce::PopupMenu& menu)
{
    std::vector<std::function<void()>> result;
    for (int id : ids) result.push_back(item(menu, id).action);
    return result;
}

inline void dispatch(const std::vector<std::function<void()>>& pending)
{
    for (const auto& action : pending)
        if (action) action();
}
}

// Real toolbar/menu actions, manually retained and dispatched like JUCE's queued
// Item::action copies. Counters outlive every component; no freed owner is used.
// This does not open a native menu or test physical popup event ordering.
template <class Check>
void testToolbarMenuLifetime(Check check)
{
    using namespace toolbar_menu_detail;
    juce::TemporaryFile fixture("-cs80-toolbar-menu-lifetime");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    Counts first {}, second {};
    auto toolbar = std::make_unique<Toolbar>(*host);
    toolbar->setActions(actions(first));
    auto menu = toolbar->buildMenu();
    auto pending = retain(menu);
    for (int id : ids)
    {
        const auto entry = item(menu, id);
        check(entry.isEnabled && bool(entry.action), "each connected toolbar command and zoom choice has an enabled action");
        if (id >= 100)
            check(entry.isTicked == (id == 200), "only the active zoom preset is ticked");
    }
    for (size_t i = 0; i < pending.size(); ++i)
    {
        auto expected = first;
        ++expected[i];
        if (pending[i]) pending[i]();
        check(first == expected, "each live menu ID invokes only its own callback once with the exact zoom value");
    }

    toolbar->setActions(actions(second));
    dispatch(pending);
    for (size_t i = 0; i < ids.size(); ++i)
        check(first[i] == 1 && second[i] == 0, "rewiring callbacks invalidates every previously retained menu action");
    pending = retain(toolbar->buildMenu());
    dispatch(pending);
    for (auto count : second)
        check(count == 1, "a freshly built menu uses the replacement callbacks");

    toolbar->setActions({});
    dispatch(pending);
    for (auto count : second)
        check(count == 1, "clearing callbacks also invalidates previously retained menu actions");
    menu = toolbar->buildMenu();
    for (int id : ids)
        check(! item(menu, id).action, "disconnected commands are omitted from the menu");

    toolbar->setActions(actions(first));
    pending = retain(toolbar->buildMenu());
    menu = {}; // The retained action must not depend on the PopupMenu's lifetime.
    dispatch(pending);
    const auto beforeClose = first;
    toolbar.reset();
    dispatch(pending);
    for (size_t i = 0; i < ids.size(); ++i)
        check(first[i] == beforeClose[i], "closing the toolbar before dispatch suppresses every retained command and zoom action");

    // Keep menu presentation rules unchanged, including disabled but connected items.
    toolbar = std::make_unique<Toolbar>(*host);
    auto disabled = actions(first);
    disabled.canUndo = disabled.canRedo = [] { return false; };
    disabled.midiLearnCount = [] { return 0; };
    disabled.selectCompare = {};
    disabled.zoomPresets.clear();
    toolbar->setActions(std::move(disabled));
    host->setPure(true);
    toolbar->refresh();
    menu = toolbar->buildMenu();
    for (int id : { 4, 5, 11, 20 })
        check(bool(item(menu, id).action) && ! item(menu, id).isEnabled,
              "undo, redo, controllers and MIDI clear retain their existing disabled menu flags");
    check(! item(menu, 3).action, "copy compare is omitted without a compare selector");
    for (size_t i = 13; i < ids.size(); ++i)
        check(! item(menu, ids[i]).action, "zoom actions are omitted without zoom presets");
    host->setPure(false);

    // An admitted callback may close its owner. The guard must not touch it after invocation.
    for (bool zoom : { false, true })
    {
        toolbar = std::make_unique<Toolbar>(*host);
        int calls = 0;
        Toolbar::Actions close;
        close.savePreset = [&] { ++calls; toolbar.reset(); };
        close.zoomPresets = { 100 };
        close.setZoom = [&](int percent) { calls += percent == 100 ? 1 : 100; toolbar.reset(); };
        toolbar->setActions(std::move(close));
        const auto callback = item(toolbar->buildMenu(), zoom ? 200 : 1).action;
        check(bool(callback), "a callback that closes its toolbar is connected");
        if (callback) callback();
        check(! toolbar && calls == 1, "a live normal or zoom callback can destroy its own toolbar exactly once");
        if (callback) callback();
        check(calls == 1, "a second dispatch after callback-owned destruction is inert");
    }

    // Likewise, replacing Actions inside a running callback must not destroy its callable mid-call.
    toolbar = std::make_unique<Toolbar>(*host);
    int rebound = 0;
    Toolbar::Actions replace;
    replace.savePreset = [&]
    {
        toolbar->setActions({});
        ++rebound;
    };
    toolbar->setActions(std::move(replace));
    const auto callback = item(toolbar->buildMenu(), 1).action;
    if (callback) callback();
    check(rebound == 1 && ! toolbar->getActions().savePreset, "a callback remains valid while replacing its own Actions");
    if (callback) callback();
    check(rebound == 1, "a callback that rewired Actions is invalid on the next dispatch");
}
}

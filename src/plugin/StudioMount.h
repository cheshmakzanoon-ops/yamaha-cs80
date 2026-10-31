#pragma once

// What the editor mounts for the modern UI layer: the Studio toolbar, the voice-monitor drawer and the
// controllers strip (A/B compare is the processor's, ws6). The editor owns one StudioMount and wires its callbacks
// in one delimited block of its constructor ([ws8-studio]), so the layout code stays untouched.

#include "HardwareKeyboard.h"
#include "StudioDrawers.h"
#include "StudioToolbar.h"
#include "StudioTour.h"

#include <functional>

class Cs80AudioProcessor;

namespace cs80::ui
{

// Editor-wide shortcuts of the modern layer: Ctrl/Cmd+Z undo, Shift+Ctrl/Cmd+Z (or Ctrl+Y) redo, Ctrl/Cmd+S save.
// A text field keeps these keys for its own editing.
class StudioShortcuts final : public juce::KeyListener
{
public:
    std::function<void()> undo, redo, save;

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        const auto mods = key.getModifiers();
        if (! mods.isCommandDown() || QwertyKeyboard::focusIsTextEntry())
            return false;
        const auto code = juce::CharacterFunctions::toUpperCase((juce::juce_wchar) key.getKeyCode());
        if (code == 'Z' && (mods.isShiftDown() ? (bool) redo : (bool) undo))
        {
            (mods.isShiftDown() ? redo : undo)();
            return true;
        }
        if (code == 'Y' && ! mods.isShiftDown() && redo)
        {
            redo();
            return true;
        }
        if (code == 'S' && save)
        {
            save();
            return true;
        }
        return false;
    }
};

class StudioMount final : private juce::Timer
{
public:
    explicit StudioMount(Cs80AudioProcessor& p)
        : processor(p), toolbar(p), scope(p), controllers(p), qwerty(p.getKeyboardState())
    {
        controllers.setComponentID("controllers_strip");
        scope.setComponentID("voice_monitor");
        // The editor state saved in the plugin state decides how the layer comes back.
        auto& state = processor.getEditorState();
        controllersWanted = state.controllersShown.load();
        qwerty.setEnabled(state.qwertyEnabled.load());
        setCircuitNotesInTooltips(state.circuitNotes.load());
        tour.setComponentID("guided_tour");
        tour.onDismissed = [this]
        {
            processor.getEditorState().tourSeen = true;
            if (tour.getPeer() != nullptr)
                TourMemory::markSeen();   // only a real window remembers it for new instances
        };
        startTimerHz(8);
    }
    ~StudioMount() override { stopTimer(); }

    // Hidden in PURE CS-80 whatever the player chose; shown again when PURE CS-80 is switched off.
    void setControllersShown(bool shown)
    {
        controllersWanted = shown;
        processor.getEditorState().controllersShown = shown;
        sync();
    }
    void setQwertyEnabled(bool shouldPlay)
    {
        qwerty.setEnabled(shouldPlay);
        processor.getEditorState().qwertyEnabled = shouldPlay;
    }
    void setCircuitNotesShown(bool shown)
    {
        setCircuitNotesInTooltips(shown);
        processor.getEditorState().circuitNotes = shown;
    }
    bool areControllersShown() const noexcept { return controllersWanted; }
    void sync()
    {
        const bool pure = processor.getParamValue(cs80::P_parityMode) >= 0.5f;
        const bool visible = controllersWanted && ! pure;
        if (controllers.isVisible() != visible)
        {
            controllers.setVisible(visible);
            if (visible)
                controllers.toFront(false);
        }
    }

    Cs80AudioProcessor& processor;
    StudioToolbar toolbar;
    ScopeDrawer scope;
    ControllersStrip controllers;
    QwertyKeyboard qwerty;
    StudioShortcuts shortcuts;
    TourOverlay tour;

private:
    void timerCallback() override
    {
        sync();
        // The tour opens by itself once: in a real window, for a player who has not dismissed it here or before.
        if (! tourChecked && toolbar.getPeer() != nullptr && toolbar.isShowing())
        {
            tourChecked = true;
            if (shouldOpenTourByItself(true, processor.getEditorState().tourSeen.load(), TourMemory::seen()))
                tour.start();
        }
    }

public:
    // The tour opens by itself only in a real window (never in a headless or offscreen editor) and only for a player
    // who has dismissed it neither in this session nor on this computer.
    static constexpr bool shouldOpenTourByItself(bool realWindow, bool seenInSession, bool seenOnComputer) noexcept
    {
        return realWindow && ! seenInSession && ! seenOnComputer;
    }

private:
    bool controllersWanted = false;
    bool tourChecked = false;
};

} // namespace cs80::ui

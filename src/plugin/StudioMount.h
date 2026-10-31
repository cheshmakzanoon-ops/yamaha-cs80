#pragma once

// What the editor mounts for the modern UI layer: the Studio toolbar, the voice-monitor drawer, the
// controllers strip and the local A/B fallback. The editor owns one StudioMount and wires its callbacks
// in one delimited block of its constructor ([ws8-studio]), so the layout code stays untouched.

#include "HardwareKeyboard.h"
#include "StudioDrawers.h"
#include "StudioToolbar.h"

#include <functional>

class Cs80AudioProcessor;

namespace cs80::ui
{

class StudioMount final : private juce::Timer
{
public:
    explicit StudioMount(Cs80AudioProcessor& p)
        : processor(p), toolbar(p), scope(p), controllers(p), compare(p), qwerty(p.getKeyboardState())
    {
        controllers.setComponentID("controllers_strip");
        scope.setComponentID("voice_monitor");
        // The editor state saved in the plugin state decides how the layer comes back.
        auto& state = processor.getEditorState();
        controllersWanted = state.controllersShown.load();
        qwerty.setEnabled(state.qwertyEnabled.load());
        setCircuitNotesInTooltips(state.circuitNotes.load());
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
    CompareSlots compare;
    QwertyKeyboard qwerty;

private:
    void timerCallback() override { sync(); }
    bool controllersWanted = false;
};

} // namespace cs80::ui

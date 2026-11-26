#pragma once

#include "UiComponents.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class Cs80AudioProcessor;

namespace cs80::ui
{

// Program browsing, A/B compare, undo/redo, Studio and the menu.
// The instrument always uses its circuit engine; there is no mode selector.
// Actions are supplied by the editor, keeping ownership and lifetimes explicit.
//
// The toolbar paints its own recessed console rail, so it can sit over the editor's toolbar
// row. It polls the display callbacks on its own 8 Hz timer.
class StudioToolbar final : public juce::Component, private juce::Timer
{
public:
    struct PresetDisplay
    {
        juce::String number;   // "044", or "USR"
        juce::String name;
        juce::String tag;      // "FACTORY  INIT", "USER PATCH"
        bool edited = false;
        bool operator==(const PresetDisplay&) const = default;
    };

    struct Actions
    {
        // Program display and stepping (direction -1 or +1). stepArmed(direction) is true while that key waits for a
        // second press that discards unsaved edits.
        std::function<PresetDisplay()> presetDisplay;
        std::function<void(int)> stepPreset;
        std::function<bool(int)> stepArmed;
        std::function<void()> openLibrary;
        std::function<bool()> libraryOpen;
        // A/B compare: slot 0 = A, 1 = B.
        std::function<void(int)> selectCompare;
        std::function<int()> activeCompare;
        std::function<void()> copyCompare;   // copy the open slot to the other one
        // Undo history.
        std::function<void()> undo, redo;
        std::function<bool()> canUndo, canRedo;
        std::function<juce::String()> undoName, redoName;
        // Studio view.
        std::function<void()> toggleStudio;
        std::function<bool()> studioOpen;
        std::function<void()> showEvidence;    // Studio > Settings
        // Menu entries.
        std::function<void()> savePreset;
        std::function<void()> initPatch;
        std::function<void()> randomise;   // within the original panel ranges (ws6), one undo step
        std::function<void()> toggleScope;
        std::function<bool()> scopeOpen;
        std::function<void()> toggleControllers;
        std::function<bool()> controllersShown;
        std::function<void()> toggleQwerty;
        std::function<bool()> qwertyEnabled;
        std::function<void()> toggleCircuitNotes;
        std::function<bool()> circuitNotesShown;
        std::function<int()> midiLearnCount;
        std::function<void()> clearMidiLearn;
        std::function<void()> startTour;
        // Display size: the editor's zoom presets (percent), and the one in force (0 when the size is not a preset).
        std::vector<int> zoomPresets;
        std::function<void(int)> setZoom;
        std::function<int()> zoom;
    };

    explicit StudioToolbar(Cs80AudioProcessor&);
    ~StudioToolbar() override;

    void setActions(Actions);
    const Actions& getActions() const noexcept { return actions; }
    // Re-reads every callback and parameter now (the timer does this eight times a second).
    void refresh();
    // The menu as built from the connected actions (also what the menu key shows).
    juce::PopupMenu buildMenu();

    // Design size of the toolbar in the editor's 1600 x 1000 design space.
    static constexpr float kDesignW = 864.0f, kDesignH = 44.0f;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class Key;
    class Display;
    void timerCallback() override;
    void showMenu();

    Cs80AudioProcessor& processor;
    Actions actions;
    std::uint64_t actionsGeneration = 0;
    std::unique_ptr<Key> prev, next, library, compareA, compareB, undoKey, redoKey,
        fxKey, studioKey, menuKey;
    std::unique_ptr<Display> display;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioToolbar)
};

} // namespace cs80::ui

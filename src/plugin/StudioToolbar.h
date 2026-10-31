#pragma once

#include "UiComponents.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>

class Cs80AudioProcessor;

namespace cs80::ui
{

// The slim toolbar of the modern layer: preset display with prev/next and LIBRARY, A/B compare,
// undo/redo, the ENGINE (CIRCUIT | LEGACY) and PURE CS-80 switches, the STUDIO key and a menu.
//
// ENGINE and PURE CS-80 are bound to the existing host parameters source_performance and
// parity_mode (IDs unchanged). Everything else goes through the Actions callbacks, so the
// integrator can connect the library's prev/next and A/B compare and the processor's undo
// history without this component knowing about them. A key whose action is not connected is
// disabled; a menu entry whose action is not connected is left out.
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
    void setEngine(bool circuit);
    void setPure(bool pure);
    void showMenu();

    Cs80AudioProcessor& processor;
    Actions actions;
    std::unique_ptr<Key> prev, next, library, compareA, compareB, undoKey, redoKey,
        circuitKey, legacyKey, pureKey, studioKey, menuKey;
    std::unique_ptr<Display> display;
    std::unique_ptr<juce::ParameterAttachment> engineAttachment, pureAttachment;
    float engineValue = 0.0f, pureValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioToolbar)
};

// A minimal local A/B compare for the toolbar until the library's compare is connected: two
// sound snapshots (every core parameter except the physical setup and PURE CS-80, plus both
// rank selectors). The first switch to an empty slot copies the open slot into it.
class CompareSlots
{
public:
    explicit CompareSlots(Cs80AudioProcessor& p) : processor(p) {}
    int active() const noexcept { return activeSlot; }
    void select(int slot);
    void copyToOther();

private:
    struct Snapshot
    {
        cs80::ParamValues values;
        std::array<int, 2> sources { 13, 13 };
    };
    Snapshot capture() const;
    void apply(const Snapshot&);

    Cs80AudioProcessor& processor;
    std::array<std::optional<Snapshot>, 2> slots;
    int activeSlot = 0;
};

} // namespace cs80::ui

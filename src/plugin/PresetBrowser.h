#pragma once

#include "UiComponents.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class Cs80AudioProcessor;
namespace cs80::tests { struct PresetSaveAccess; }

namespace cs80::ui
{

struct PresetSignature; // Drawable summary of a patch; defined in PresetBrowser.cpp.

// Raised, hardware-style plate button for the library: keycap-like actions,
// the segmented sort strip, arrow keys, a close key and the favourite star.
class PlateButton final : public juce::Button
{
public:
    enum class Kind { standard, primary, danger, previous, next, close, clear, segment, star };

    explicit PlateButton(const juce::String& text, Kind kind = Kind::standard);

    void setUiScale(float pixelsPerUnit) noexcept { scale = pixelsPerUnit; repaint(); }
    // Segment buttons only: +1 draws an up arrow on the lit segment, -1 a down arrow.
    void setArrowDirection(int direction) noexcept { arrow = direction; repaint(); }
    // DANGER keys only: lit red while the next press will do the destructive thing.
    void setArmed(bool shouldBeArmed) noexcept { armed = shouldBeArmed; repaint(); }
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    Kind kind;
    float scale = 1.0f;
    int arrow = 0;
    bool armed = false;
};

// A backlit filter tab with a colour-coded lamp and a live match count.
class ChipButton final : public juce::Button
{
public:
    ChipButton(const juce::String& label, juce::Colour accent);

    void setCount(int newCount);
    int getCount() const noexcept { return count; }
    void setStarGlyph(bool useStar) noexcept { starGlyph = useStar; }
    // What the lamp says: round = instrument (coloured dot), hollow = style (neutral outline), square = library scope.
    enum class Lamp { square, round, hollow };
    void setLamp(Lamp shape) noexcept { lampShape = shape; repaint(); }
    void setUiScale(float pixelsPerUnit) noexcept { scale = pixelsPerUnit; repaint(); }
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    juce::Colour accentColour;
    int count = 0;
    bool starGlyph = false;
    Lamp lampShape = Lamp::square;
    float scale = 1.0f;
};

// A label that draws with its own font; the shared look and feel sizes labels
// from their height, which does not suit the paragraph text in the inspector.
class InfoLabel final : public juce::Label
{
public:
    void paint(juce::Graphics&) override;
};

// A recessed text well in the panel style. Optional magnifier icon, focus lamp,
// and a hook so the arrow keys can steer the result list while typing.
class GlassField final : public juce::TextEditor
{
public:
    enum class Icon { none, search };
    explicit GlassField(Icon icon = Icon::none);

    void setUiScale(float pixelsPerUnit) noexcept { scale = pixelsPerUnit; repaint(); }
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(juce::Component::FocusChangeType) override;
    void focusLost(juce::Component::FocusChangeType) override;

    // Return true when the key was used (list navigation, load, ...).
    std::function<bool(const juce::KeyPress&)> onSpecialKey;

private:
    Icon icon;
    float scale = 1.0f;
};

// The preset library: filter column, result list, inspector. Double-click or
// LOAD replaces the sound with the chosen patch (the key reads RELOAD when that
// patch is already sounding); REVERT returns to the sound that was loaded when
// the library opened.
class PresetBrowser final : public juce::Component,
                            private juce::ListBoxModel,
                            private juce::ScrollBar::Listener,
                            private juce::Timer
{
public:
    explicit PresetBrowser(Cs80AudioProcessor&);
    ~PresetBrowser() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void refresh();
    void focusSaveField();
    void selectCurrentPreset();

    std::function<void()> onClose;
    std::function<void()> onPresetLoaded;
    // Optional: the editor's own "sounding patch has unsaved edits" state. When set, the
    // LOADED / EDITED badge follows it, so it can never disagree with the header's EDITED
    // lamp. Without it the library judges the edit state itself (see patchEdited()).
    std::function<bool()> isPatchEdited;

private:
    friend struct cs80::tests::PresetSaveAccess;

    struct Entry
    {
        juce::String name;
        juce::String category;
        juce::String description;
        int factoryIndex = -1;
        juce::File userFile;
        bool isUser = false;

        // Derived for the library (see classify() in PresetBrowser.cpp).
        int type = 0;                 // instrument type, index into the type table
        std::uint32_t styles = 0;     // style tag bit set
        juce::String key;             // stable identity for favourites
        juce::String slot;            // "017" or "U03"
        juce::int64 modified = 0;     // user files: milliseconds since epoch
        juce::String haystack;        // lower-case text the search box matches against
        bool ownDescription = false;  // false when many patches share the same sentence
        std::shared_ptr<const PresetSignature> signature;
    };

    class Surface;
    class Legends;
    class RailBar;
    class StarCell;
    class ResultList;
    class HoverTracker;
    class Watcher;

    // Panel geometry in design units (1600 x 936 reference); `s` converts to pixels.
    struct Layout
    {
        float s = 1.0f;
        float width = 1600.0f, height = 936.0f;
        juce::Rectangle<float> face, header, leftPlate, results, strip, well, detail, sortStrip, viewStrip, searchRect, filterRow;
        float scopeCaptionY = 0.0f, typeCaptionY = 0.0f, styleCaptionY = 0.0f, chipsBottom = 0.0f;
    };

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height,
                          bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component* existing) override;
    juce::String getNameForRow(int rowNumber) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int lastRow) override;
    void deleteKeyPressed(int lastRow) override;
    juce::String getTooltipForRow(int row) override;

    // ScrollBar::Listener (the viewport's own bar drives the custom rail)
    void scrollBarMoved(juce::ScrollBar*, double newRangeStart) override;
    // Timer: eased scrolling, only running while an animation is in flight.
    void timerCallback() override;

    void buildFilterChips();
    void layoutAll();
    void layoutDescription();
    void rebuildFilter();
    void updateSelectionWidgets();
    void clearFilters();
    bool anyFilterActive() const;
    int activeFilterCount() const;
    bool isLoaded(const Entry& e) const { return loadedKey.isNotEmpty() && e.key == loadedKey; }
    bool isLoadedAndEdited(const Entry& e) const { return isLoaded(e) && loadedEdited; }
    void loadSelected();
    void revertToOpening();
    void captureOpening();
    bool canRevert() const;
    bool patchEdited();
    void syncLoaded();
    void updateLoadLabel();
    void updateActionTips();
    int userCount() const;
    int sortArrow(int mode) const;
    void pollLoaded();
    void setCompactRows(bool compact, bool persist);
    void deleteSelected();
    void disarmDelete(bool announce);
    void saveCurrentPreset(const juce::String& name, bool overwrite = false);
    void stepSelection(int delta);
    void toggleFavourite(int entryIndex);
    bool isFavourite(const Entry&) const;
    void smoothScrollTo(double y);
    void updateRail();
    void setStatus(const juce::String& text, int tone);
    const Entry* selectedEntry() const;
    int entryIndexForRow(int row) const;

    Cs80AudioProcessor& processor;
    std::vector<Entry> entries;
    std::vector<int> filtered;
    int selectedFilteredRow = -1;
    int hoveredRow = -1;
    float uiScale = 1.0f;
    Layout layout;
    std::uint32_t typePresent = 0;       // instrument types that occur in the library
    std::uint32_t stylePresent = 0;      // style tags that occur in the library

    // Filter state.
    int scopeFilter = 0;                 // all, favourites, factory, hardware, user
    std::uint32_t typeFilter = 0;
    std::uint32_t styleFilter = 0;
    int sortMode = 0;                    // bank, A-Z, Z-A, type, newest
    bool sortReverse = false;            // clicking the lit sort segment again flips its direction
    bool restoringSelection = false;
    bool compactRows = false;            // 44 px rows instead of full cards (remembered with the favourites)

    // The sound that is loaded now (marked in the list) and the one that was
    // loaded when the library opened (what REVERT returns to).
    juce::String loadedKey;
    bool loadedEdited = false;           // the sounding patch differs from what the display names
    juce::String editBaselineKey;        // patches that are not a plain library entry: the state first seen under this name
    cs80::ParamValues editBaseline;
    juce::String editFileKey;            // saved patches: the file the reference values were read from
    cs80::ParamValues editFileValues;
    std::vector<char> editFileUsed;
    bool hasOpening = false;
    juce::String openingName;
    int openingIndex = -1;
    cs80::ParamValues openingValues;

    // Scroll animation.
    double scrollTarget = 0.0;
    double scrollVirtual = 0.0;          // unsnapped wheel position; targets snap to whole rows
    int lastScrollSet = -1;

    std::unique_ptr<Surface> surface;
    std::unique_ptr<Legends> legends;

    GlassField search { GlassField::Icon::search };
    PlateButton clearSearch { "CLEAR SEARCH", PlateButton::Kind::clear };
    std::vector<std::unique_ptr<PlateButton>> sortButtons;
    std::vector<std::unique_ptr<PlateButton>> viewButtons;
    std::vector<std::unique_ptr<ChipButton>> scopeChips;
    std::vector<std::unique_ptr<ChipButton>> typeChips;
    std::vector<std::unique_ptr<ChipButton>> styleChips;
    PlateButton resetButton { "CLEAR ALL" };
    PlateButton emptyResetButton { "CLEAR ALL FILTERS" };

    std::unique_ptr<ResultList> list;
    std::unique_ptr<RailBar> rail;
    std::unique_ptr<HoverTracker> hoverTracker;
    std::unique_ptr<Watcher> watcher;

    PlateButton favouriteButton { "Favourite", PlateButton::Kind::star };
    PlateButton previousButton { "PREV", PlateButton::Kind::previous };
    PlateButton nextButton { "NEXT", PlateButton::Kind::next };
    GlassField saveName;
    PlateButton loadButton { "LOAD", PlateButton::Kind::primary };
    PlateButton revertButton { "REVERT" };
    PlateButton deleteButton { "DELETE", PlateButton::Kind::danger };
    PlateButton saveButton { "SAVE AS" };
    PlateButton closeButton { "CLOSE", PlateButton::Kind::close };
    InfoLabel description;
    InfoLabel saveStatus;
    bool overwriteConfirmationPending = false;
    juce::String deleteArmedKey;         // the saved patch the next DELETE press will remove (arm, then confirm)
    juce::uint32 deleteArmedAt = 0;
    // Cancel a visible dialog on editor close.
    juce::ScopedMessageBox overwriteDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace cs80::ui

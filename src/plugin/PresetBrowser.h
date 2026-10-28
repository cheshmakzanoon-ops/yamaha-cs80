#pragma once

#include "UiComponents.h"

#include <functional>
#include <vector>

class Cs80AudioProcessor;

namespace cs80::ui
{

// A searchable factory/user library. Double-click or LOAD auditions in place,
// which is the workflow a player expects when choosing a CS-80 patch.
class PresetBrowser final : public juce::Component,
                            private juce::ListBoxModel
{
public:
    explicit PresetBrowser(Cs80AudioProcessor&);
    ~PresetBrowser() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void refresh();
    void focusSaveField();
    void selectCurrentPreset();

    std::function<void()> onClose;
    std::function<void()> onPresetLoaded;

private:
    struct Entry
    {
        juce::String name;
        juce::String category;
        juce::String description;
        int factoryIndex = -1;
        juce::File userFile;
        bool isUser = false;
    };

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics&, int width, int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void returnKeyPressed(int lastRow) override;

    void rebuildFilter();
    void loadSelected();
    const Entry* selectedEntry() const;

    Cs80AudioProcessor& processor;
    std::vector<Entry> entries;
    std::vector<int> filtered;
    int selectedFilteredRow = -1;
    float uiScale = 1.0f;

    juce::TextEditor search;
    juce::ComboBox category;
    juce::ListBox list;
    juce::TextEditor saveName;
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton saveButton { "SAVE USER" };
    juce::TextButton closeButton { "CLOSE" };
    juce::Label description;
    juce::String currentCategory { "All Categories" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};

} // namespace cs80::ui

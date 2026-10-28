#include "PresetBrowser.h"

#include "PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <algorithm>

namespace cs80::ui
{

namespace
{
juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        height, style));
}
} // namespace

PresetBrowser::PresetBrowser(Cs80AudioProcessor& p)
    : processor(p), list("presets", this)
{
    list.setRowHeight(26);
    list.setMultipleSelectionEnabled(false);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff070d12));
    list.setOutlineThickness(1);
    list.setWantsKeyboardFocus(false);
    addAndMakeVisible(list);

    search.setTextToShowWhenEmpty("Search patches, categories, character...", ivoryDim.withAlpha(0.55f));
    search.setFont(juce::Font(juce::FontOptions(13.0f)));
    search.onTextChange = [this] { rebuildFilter(); };
    addAndMakeVisible(search);

    category.setTextWhenNothingSelected("All Categories");
    category.onChange = [this]
    {
        currentCategory = category.getText();
        if (currentCategory.isEmpty())
            currentCategory = "All Categories";
        rebuildFilter();
    };
    addAndMakeVisible(category);

    saveName.setTextToShowWhenEmpty("New user patch name", ivoryDim.withAlpha(0.55f));
    saveName.setFont(juce::Font(juce::FontOptions(13.0f)));
    saveName.setText("User Patch", false);
    addAndMakeVisible(saveName);

    loadButton.setWantsKeyboardFocus(false);
    loadButton.onClick = [this] { loadSelected(); };
    addAndMakeVisible(loadButton);

    saveButton.setWantsKeyboardFocus(false);
    saveButton.onClick = [this]
    {
        auto name = saveName.getText().trim();
        if (name.isEmpty())
            name = processor.getPresetName() + " Edit";
        processor.saveUserPreset(name);
        refresh();
        if (onPresetLoaded)
            onPresetLoaded();
    };
    addAndMakeVisible(saveButton);

    closeButton.setWantsKeyboardFocus(false);
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton);

    description.setJustificationType(juce::Justification::topLeft);
    description.setColour(juce::Label::textColourId, ivoryDim);
    description.setMinimumHorizontalScale(0.8f);
    addAndMakeVisible(description);

    refresh();
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::refresh()
{
    entries.clear();
    const auto& bank = processor.getFactoryPresets();
    entries.reserve(bank.size() + 32);
    for (int i = 0; i < (int) bank.size(); ++i)
    {
        const auto& preset = bank[(size_t) i];
        entries.push_back({ juce::String(preset.name), juce::String(preset.category),
                            juce::String(preset.description), i, {}, false });
    }

    auto files = processor.getUserPresetFiles();
    files.sort();
    for (const auto& file : files)
        entries.push_back({ file.getFileNameWithoutExtension(), "User",
                            "User patch stored in the CS-80 Reference preset folder.",
                            -1, file, true });

    const auto previous = currentCategory;
    category.clear(juce::dontSendNotification);
    category.addItem("All Categories", 1);
    juce::StringArray categories;
    for (const auto& entry : entries)
        if (! categories.contains(entry.category, true))
            categories.add(entry.category);
    categories.sort(true);
    int id = 2;
    for (const auto& name : categories)
        category.addItem(name, id++);
    int restored = 1;
    for (int i = 0; i < category.getNumItems(); ++i)
        if (category.getItemText(i).equalsIgnoreCase(previous))
        {
            restored = category.getItemId(i);
            break;
        }
    category.setSelectedId(restored, juce::dontSendNotification);
    currentCategory = category.getText().isNotEmpty() ? category.getText() : "All Categories";

    rebuildFilter();
    selectCurrentPreset();
}

void PresetBrowser::rebuildFilter()
{
    const auto query = search.getText().trim().toLowerCase();
    const bool all = currentCategory.isEmpty() || currentCategory == "All Categories";

    filtered.clear();
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& entry = entries[(size_t) i];
        if (! all && ! entry.category.equalsIgnoreCase(currentCategory))
            continue;
        if (query.isNotEmpty())
        {
            const auto haystack = (entry.name + " " + entry.category + " " + entry.description).toLowerCase();
            if (! haystack.contains(query))
                continue;
        }
        filtered.push_back(i);
    }

    selectedFilteredRow = -1;
    list.updateContent();
    list.deselectAllRows();
    description.setText("Select a patch to inspect it. Double-click or press LOAD to audition.",
                        juce::dontSendNotification);
    repaint();
}

int PresetBrowser::getNumRows()
{
    return (int) filtered.size();
}

void PresetBrowser::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                     bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) filtered.size())
        return;

    const auto& entry = entries[(size_t) filtered[(size_t) rowNumber]];
    auto bounds = juce::Rectangle<int>(0, 0, width, height);

    if (rowIsSelected)
        g.fillAll(amber.withAlpha(0.18f));
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colour(0xff0b1218).withAlpha(0.55f));

    g.setColour(lineColour.withAlpha(0.35f));
    g.drawHorizontalLine(height - 1, 0.0f, (float) width);

    auto numberArea = bounds.removeFromLeft(46);
    auto typeArea = bounds.removeFromRight(70);
    auto categoryArea = bounds.removeFromRight(150);

    g.setColour(ivoryDim.withAlpha(0.65f));
    g.setFont(displayFont(
                         juce::jlimit(8.0f, 12.0f, (float) height * 0.40f), juce::Font::plain));
    g.drawText(juce::String(rowNumber + 1).paddedLeft('0', 3), numberArea.reduced(10, 0),
               juce::Justification::centredRight);

    g.setColour(rowIsSelected ? amberBright : ivory);
    g.setFont(displayFont(
                         juce::jlimit(9.0f, 14.0f, (float) height * 0.46f), juce::Font::bold));
    g.drawText(entry.name, bounds.reduced(10, 0), juce::Justification::centredLeft, true);

    g.setColour(amber.withAlpha(0.9f));
    g.setFont(displayFont(
                         juce::jlimit(8.0f, 12.0f, (float) height * 0.40f), juce::Font::plain));
    g.drawText(entry.category.toUpperCase(), categoryArea.reduced(8, 0),
               juce::Justification::centredLeft, true);

    g.setColour(entry.isUser ? teal.brighter(0.35f) : ivoryDim.withAlpha(0.45f));
    g.drawText(entry.isUser ? "USER" : "FACTORY", typeArea.reduced(6, 0),
               juce::Justification::centredRight);
}

void PresetBrowser::selectedRowsChanged(int lastRowSelected)
{
    selectedFilteredRow = lastRowSelected;
    if (const auto* entry = selectedEntry())
    {
        description.setText(entry->name + "\n" + entry->description, juce::dontSendNotification);
        saveName.setText(entry->name, false);
    }
    loadButton.setEnabled(selectedEntry() != nullptr);
}

void PresetBrowser::listBoxItemDoubleClicked(int, const juce::MouseEvent&)
{
    loadSelected();
}

void PresetBrowser::returnKeyPressed(int)
{
    loadSelected();
}

const PresetBrowser::Entry* PresetBrowser::selectedEntry() const
{
    if (selectedFilteredRow < 0 || selectedFilteredRow >= (int) filtered.size())
        return nullptr;
    return &entries[(size_t) filtered[(size_t) selectedFilteredRow]];
}

void PresetBrowser::loadSelected()
{
    const auto* entry = selectedEntry();
    if (entry == nullptr)
        return;

    if (entry->isUser)
        processor.loadUserPreset(entry->userFile);
    else if (entry->factoryIndex >= 0)
        processor.loadFactoryPreset(entry->factoryIndex);

    if (onPresetLoaded)
        onPresetLoaded();
}

void PresetBrowser::selectCurrentPreset()
{
    const int current = processor.getCurrentPresetIndex();
    const auto currentName = processor.getPresetName();
    for (int row = 0; row < (int) filtered.size(); ++row)
    {
        const auto& entry = entries[(size_t) filtered[(size_t) row]];
        if ((! entry.isUser && entry.factoryIndex == current)
            || entry.name.equalsIgnoreCase(currentName))
        {
            selectedFilteredRow = row;
            list.selectRow(row, false, true);
            list.scrollToEnsureRowIsOnscreen(row);
            if (const auto* selected = selectedEntry())
            {
                description.setText(selected->name + "\n" + selected->description,
                                    juce::dontSendNotification);
                saveName.setText(selected->name, false);
            }
            loadButton.setEnabled(true);
            break;
        }
    }
}

void PresetBrowser::focusSaveField()
{
    saveName.grabKeyboardFocus();
    saveName.selectAll();
}

void PresetBrowser::paint(juce::Graphics& g)
{
    const float sx = (float) getWidth() / 1200.0f;
    const float sy = (float) getHeight() / 760.0f;
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(sx, sy));

    juce::ColourGradient sheet(juce::Colour(0xff101a22), 0.0f, 0.0f,
                                juce::Colour(0xff060c11), 0.0f, 760.0f, false);
    g.setGradientFill(sheet);
    g.fillRoundedRectangle(0.0f, 0.0f, 1200.0f, 760.0f, 7.0f);
    g.setColour(amber.withAlpha(0.65f));
    g.drawRoundedRectangle(0.5f, 0.5f, 1199.0f, 759.0f, 7.0f, 1.2f);

    g.setColour(juce::Colour(0xff0b141b));
    g.fillRect(0.0f, 0.0f, 1200.0f, 56.0f);
    g.setColour(lineColour.withAlpha(0.8f));
    g.drawHorizontalLine(56, 0.0f, 1200.0f);

    auto titleFont = displayFont( 20.0f, juce::Font::bold);
    titleFont.setExtraKerningFactor(0.13f);
    g.setFont(titleFont);
    g.setColour(ivory);
    g.drawText("PRESET LIBRARY", 24, 12, 320, 30, juce::Justification::centredLeft);
    g.setColour(amber);
    g.setFont(displayFont( 9.5f, juce::Font::bold));
    g.drawText(juce::String((int) entries.size()) + " PATCHES  /  22 HARDWARE TONES  /  USER LIBRARY",
               25, 35, 520, 14, juce::Justification::centredLeft);

    g.setColour(ivoryDim);
    g.setFont(displayFont( 9.0f, juce::Font::bold));
    g.drawText("INDEX", 20, 108, 60, 14, juce::Justification::centredLeft);
    g.drawText("PATCH", 70, 108, 600, 14, juce::Justification::centredLeft);
    g.drawText("CATEGORY", 878, 108, 150, 14, juce::Justification::centredLeft);
    g.drawText("SOURCE", 1120, 108, 60, 14, juce::Justification::centredRight);

    g.setColour(lineColour.withAlpha(0.8f));
    g.drawHorizontalLine(126, 20.0f, 1180.0f);

    g.setColour(ivoryDim.withAlpha(0.7f));
    g.setFont(displayFont( 9.0f, juce::Font::bold));
    g.drawText("SAVE CURRENT STATE AS", 740, 63, 240, 16, juce::Justification::centredLeft);

    g.restoreState();
}

void PresetBrowser::resized()
{
    uiScale = juce::jmin((float) getWidth() / 1200.0f, (float) getHeight() / 760.0f);
    auto r = [this](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt(x * uiScale), juce::roundToInt(y * uiScale),
                                    juce::roundToInt(w * uiScale), juce::roundToInt(h * uiScale));
    };

    closeButton.setBounds(r(1092.0f, 13.0f, 84.0f, 30.0f));
    search.setBounds(r(24.0f, 72.0f, 380.0f, 30.0f));
    category.setBounds(r(416.0f, 72.0f, 240.0f, 30.0f));
    saveName.setBounds(r(740.0f, 80.0f, 220.0f, 28.0f));
    saveButton.setBounds(r(970.0f, 80.0f, 200.0f, 28.0f));
    list.setBounds(r(20.0f, 128.0f, 1160.0f, 474.0f));
    list.setRowHeight(juce::jmax(22, juce::roundToInt(26.0f * uiScale)));
    description.setBounds(r(24.0f, 614.0f, 1152.0f, 70.0f));
    description.setFont(displayFont(
                                   juce::jmax(9.0f, 11.5f * uiScale), juce::Font::plain));
    loadButton.setBounds(r(874.0f, 700.0f, 150.0f, 38.0f));
    closeButton.setBounds(r(1092.0f, 14.0f, 84.0f, 30.0f));
}

} // namespace cs80::ui

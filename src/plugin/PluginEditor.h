#pragma once

#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>

class Cs80AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit Cs80AudioProcessorEditor(Cs80AudioProcessor&);
    ~Cs80AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void showAdvancedPanel(bool shouldShow);
    void showAdvancedPage(int pageIndex);
    void showPresetBrowser(bool shouldShow);
    bool isAdvancedVisible() const;

    // Display size presets (75, 100, 125, 150 and 200 percent of 1600 x 1000). Choosing one resizes the editor
    // and remembers the choice for new editor windows (a user preference, not a host parameter). Other values
    // are ignored. getZoomPercent() is 0 while the window has a size of its own.
    void setZoomPercent(int percent);
    int getZoomPercent() const;

private:
    void timerCallback() override;

    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cs80AudioProcessorEditor)
};

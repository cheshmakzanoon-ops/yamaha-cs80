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

private:
    void timerCallback() override;

    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Cs80AudioProcessorEditor)
};

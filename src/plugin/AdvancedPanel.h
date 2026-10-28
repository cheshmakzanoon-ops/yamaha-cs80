#pragma once

#include "UiComponents.h"

#include <array>
#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui
{

class AdvancedPage;

// The modern control layer. It deliberately sits behind its own panel so the
// primary surface remains a CS-80, not a generic plugin with 200 knobs.
class AdvancedPanel final : public juce::Component
{
public:
    explicit AdvancedPanel(Cs80AudioProcessor&);
    ~AdvancedPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void showPage(int index);

    std::function<void()> onClose;

private:
    Cs80AudioProcessor& processor;
    std::array<std::unique_ptr<AdvancedPage>, 4> pages;
    std::array<std::unique_ptr<juce::TextButton>, 4> tabs;
    juce::TextButton closeButton { "CLOSE" };
    int activePage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPanel)
};

} // namespace cs80::ui

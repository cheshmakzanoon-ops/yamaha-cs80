#pragma once

#include "UiComponents.h"

#include <array>
#include <functional>
#include <memory>

class Cs80AudioProcessor;

namespace cs80::ui
{

class AdvancedPage;
class AdvChassis;
class AdvPageTab;
class AdvCloseButton;
class AdvTitleBlock;
class AdvFooterStatus;
class AdvInfoLayer;

// The modern control layer, drawn as a second instrument panel. It deliberately
// sits behind its own overlay so the primary surface remains a CS-80, not a
// generic plugin with 200 knobs. The panel occupies design rect
// (0, 64, 1600, 936); everything inside is laid out in that design space and
// scaled to the real bounds. Static art is cached per page, the live widgets
// (scopes, curves, diagrams) repaint only when their parameters change.
class AdvancedPanel final : public juce::Component, private juce::Timer
{
public:
    explicit AdvancedPanel(Cs80AudioProcessor&);
    ~AdvancedPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;
    void showPage(int index);
    int getActivePage() const noexcept { return activePage; }

    std::function<void()> onClose;

private:
    void timerCallback() override;
    void stepPage(int direction);

    Cs80AudioProcessor& processor;
    std::unique_ptr<AdvChassis> chassis;
    std::array<std::unique_ptr<AdvPageTab>, 4> tabs;
    std::array<std::unique_ptr<AdvancedPage>, 4> pages;
    std::unique_ptr<AdvTitleBlock> titleBlock;
    std::unique_ptr<AdvFooterStatus> footer;
    std::unique_ptr<AdvCloseButton> closeButton;
    std::unique_ptr<AdvInfoLayer> info;
    int activePage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPanel)
};

} // namespace cs80::ui

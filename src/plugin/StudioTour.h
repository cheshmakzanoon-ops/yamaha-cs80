#pragma once

#include "UiComponents.h"

#include <functional>
#include <memory>
#include <vector>

namespace cs80::ui
{

// The guided tour: an overlay over the whole editor that dims everything but one area, with a
// card that explains it in plain words. BACK / NEXT step through it; DONE, the close key or Esc
// dismiss it. Keyboard: Right or Return next, Left back, Esc close.
//
// Remembered: a dismissed tour is recorded in the plugin state (per session) and, when the editor
// is a real window, in the user's settings file (TourMemory), so a new instance does not show it
// again. It opens by itself only once, only in a real window; the toolbar menu opens it any time.
class TourOverlay final : public juce::Component
{
public:
    struct Step
    {
        juce::String title;
        juce::String body;
        std::function<juce::Rectangle<int>()> target;   // in this overlay's coordinates; empty = no highlight
    };

    TourOverlay();
    ~TourOverlay() override;

    void setSteps(std::vector<Step>);
    void start(int step = 0);
    void dismiss();
    bool isRunning() const noexcept { return isVisible(); }
    int getStep() const noexcept { return current; }
    int getNumSteps() const noexcept { return (int) steps.size(); }
    void next();
    void back();
    std::function<void()> onDismissed;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    void mouseDown(const juce::MouseEvent&) override {}   // modal: clicks outside the card do nothing

private:
    juce::Rectangle<int> cardBounds() const;
    void layoutCard();

    std::vector<Step> steps;
    int current = 0;
    std::unique_ptr<juce::Button> backKey, nextKey, closeKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TourOverlay)
};

// Whether the player has dismissed the tour on this computer: a small settings file in the user's
// application-data folder (CS80Reference/ui-settings.xml). Never written by headless editors.
struct TourMemory
{
    static juce::File file();
    static bool seen();
    static void markSeen();
};

} // namespace cs80::ui

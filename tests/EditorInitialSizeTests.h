#pragma once

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <cmath>
#include <cstdio>

namespace cs80::tests
{
// Run on the existing message thread. Synthetic desktop-size cases are also
// compile-time checked beside the production helper; this checks actual JUCE
// display selection, constructor wiring and the host/screenshot resize contract.
template <typename Check>
void testEditorInitialSize(Check check)
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const auto* display = displays.getDisplayForPoint(juce::Desktop::getMousePosition());
    if (display == nullptr)
        display = displays.getPrimaryDisplay();
    const auto area = display != nullptr ? display->userArea : juce::Rectangle<int>();

    juce::TemporaryFile fixture("-cs80-editor-size");
    Cs80AudioProcessor processor(fixture.getFile());
    Cs80AudioProcessorEditor editor(processor);
    const int width = editor.getWidth(), height = editor.getHeight();
    std::printf("Editor initial size: %dx%d; logical usable display: %dx%d\n",
                width, height, area.getWidth(), area.getHeight());
    check(width >= 1120 && height >= 700 && width <= 1600 && height <= 1000
              && width * 5 == height * 8,
          "initial editor size stays within supported bounds at exact 8:5 aspect");
    if (area.isEmpty())
        check(width == 1600 && height == 1000,
              "missing display preserves established initial editor size");
    else if (area.getWidth() >= 1152 && area.getHeight() >= 764)
        check(width <= area.getWidth() - 32 && height <= area.getHeight() - 64,
              "initial editor reserves host/window chrome within logical usable display");
    else if (area.getWidth() >= 1120 && area.getHeight() >= 700)
        check(width <= area.getWidth() && height <= area.getHeight(),
              "tight usable display retains minimum layout without inventing extra chrome space");
    else
        check(width == 1120 && height == 700,
              "undersized display retains the documented minimum instead of silently shrinking layout");

    const auto* constrainer = editor.getConstrainer();
    check(editor.isResizable() && constrainer != nullptr
              && constrainer->getMinimumWidth() == 1120 && constrainer->getMinimumHeight() == 700
              // Intentional change (ws7, Phase 5.5): the maximum was 2400 x 1500; it is raised to 3200 x 2000 so
              // the 200 % display-size preset fits. Minimum and aspect ratio are unchanged.
              && constrainer->getMaximumWidth() == 3200 && constrainer->getMaximumHeight() == 2000
              && std::abs(constrainer->getFixedAspectRatio() - 1.6) < 1.0e-6,
          "initial display fitting preserves the existing manual resize constraints");
    for (const auto size : { juce::Point<int>(1120, 700), juce::Point<int>(1200, 760),
                             juce::Point<int>(1600, 1000), juce::Point<int>(2400, 1500) })
    {
        editor.setSize(size.x, size.y);
        check(editor.getWidth() == size.x && editor.getHeight() == size.y,
              "explicit host and screenshot dimensions remain authoritative after initial fitting");
    }
}
} // namespace cs80::tests

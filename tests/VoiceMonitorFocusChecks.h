#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
namespace cs80::test {
inline bool voiceMonitorFocusMatches(juce::Component& scope, bool live)
{
    juce::KeyboardFocusTraverser traverser;
    const auto order = traverser.getAllComponents(&scope);
    const std::array<const char*, 6> names {
        "Monitor channel I", "Monitor channel II", "Close the voice monitor",
        "Switch between settings illustrations and live callback voice inspection",
        "Inspect previous physical voice slot", "Inspect next physical voice slot" };
    if (order.size() != (live ? 6u : 4u)) return false;
    for (size_t i = 0; i < order.size(); ++i)
        if (order[i]->getTitle() != names[i]) return false;
    return true;
}
}

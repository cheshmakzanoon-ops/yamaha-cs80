#pragma once

// Original-panel editor checks (ws7-panel), run by the UI self-test on its JUCE message thread.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <chrono>

namespace cs80::tests
{
namespace panel_editor_detail
{
inline juce::Component* findByTitle(juce::Component& parent, const juce::String& title)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getTitle() == title)
            return child;
        if (auto* found = findByTitle(*child, title))
            return found;
    }
    return nullptr;
}

template <typename Flush, typename Ready>
bool waitFor(Flush flush, Ready ready, int milliseconds = 2000)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (! ready() && std::chrono::steady_clock::now() < deadline)
        flush(20);
    return ready();
}
} // namespace panel_editor_detail

// The program display marks unsaved edits from parameter-change notifications rather than by comparing
// every parameter on each timer tick. Host automation of one control must still light it, and undoing
// that edit must clear it again.
template <typename Check, typename Flush>
void testUnsavedEditIndicator(Check check, Flush flush)
{
    using namespace panel_editor_detail;
    juce::TemporaryFile presetRoot("-cs80-panel-editor");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    int initIndex = 0;
    for (int i = 0; i < (int) host.getFactoryPresets().size(); ++i)
        if (juce::String(host.getFactoryPresets()[(size_t) i].name).equalsIgnoreCase("Init Patch")) { initIndex = i; break; }
    host.loadFactoryPreset(initIndex);
    Cs80AudioProcessorEditor editor(host);
    editor.setSize(1600, 1000);
    auto* display = dynamic_cast<juce::Button*>(findByTitle(editor, "Program display"));
    check(display != nullptr, "program display is discoverable by title");
    if (display == nullptr) return;
    const auto edited = [display] { return display->getTooltip().contains("unsaved edits"); };
    check(waitFor(flush, [&] { return ! edited(); }), "a freshly recalled factory program shows no unsaved edits");

    auto* cutoff = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::chParam(0, cs80::CP_lpfCutoff)));
    check(cutoff != nullptr, "channel I LPF cutoff host parameter exists");
    if (cutoff == nullptr) return;
    const float original = cutoff->getValue();
    cutoff->setValueNotifyingHost(original > 0.5f ? 0.1f : 0.9f);
    check(waitFor(flush, edited), "host automation of one control marks the program as edited");
    cutoff->setValueNotifyingHost(original);
    check(waitFor(flush, [&] { return ! edited(); }), "restoring the control clears the edited mark");
}

template <typename Check, typename Flush>
void runPanelEditorTests(Check check, Flush flush)
{
    testUnsavedEditIndicator(check, flush);
}
} // namespace cs80::tests

#pragma once

// Original-panel editor checks (ws7-panel), run by the UI self-test on its JUCE message thread.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/PanelLayout.h"
#include "plugin/UiComponents.h"

#include <chrono>
#include <cstdio>
#include <vector>

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

inline void collectControls(juce::Component& root, juce::Component& parent, std::vector<juce::Component*>& out)
{
    for (auto* child : parent.getChildren())
    {
        if (! child->isVisible())
            continue;
        if (dynamic_cast<juce::Button*>(child) != nullptr || dynamic_cast<juce::Slider*>(child) != nullptr
            || dynamic_cast<cs80::ui::PerformanceWheel*>(child) != nullptr || dynamic_cast<cs80::ui::RibbonStrip*>(child) != nullptr)
        {
            out.push_back(child);
            continue;
        }
        collectControls(root, *child, out);
    }
}

inline juce::Rectangle<int> boundsIn(juce::Component& root, juce::Component& c)
{
    return root.getLocalArea(&c, c.getLocalBounds());
}

inline juce::Component* findParameterSlider(juce::Component& parent, int parameterIndex)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(child))
            if (knob->getParameterIndex() == parameterIndex)
                return knob;
        if (auto* found = findParameterSlider(*child, parameterIndex))
            return found;
    }
    return nullptr;
}

inline juce::Button* findButtonText(juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* button = dynamic_cast<juce::Button*>(child))
            if (button->getButtonText() == text)
                return button;
        if (auto* found = findButtonText(*child, text))
            return found;
    }
    return nullptr;
}

struct BalancedGesture final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0, depth = 0, unbracketed = 0;
    void parameterValueChanged(int, float) override { ++changes; if (depth != 1) ++unbracketed; }
    void parameterGestureChanged(int, bool starting) override { if (starting) { ++begins; ++depth; } else { ++ends; --depth; } }
    bool ok() const { return changes > 0 && begins == 1 && ends == 1 && depth == 0 && unbracketed == 0; }
};
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


// Main view laid out from the conformance-derived table (docs/PANEL_CONFORMANCE.md): at both supported sizes no
// two visible controls overlap, and each moved group sits in the block the audit assigns it.
template <typename Check, typename Flush>
void testMainViewLayout(Check check, Flush flush)
{
    using namespace panel_editor_detail;
    namespace geo = cs80::panel::geo;
    juce::TemporaryFile presetRoot("-cs80-panel-layout");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    Cs80AudioProcessorEditor editor(host);
    for (const auto size : { juce::Point<int>(1600, 1000), juce::Point<int>(1120, 700) })
    {
        editor.setSize(size.x, size.y);
        flush(30);
        const float sx = (float) size.x / 1600.0f, sy = (float) size.y / 1000.0f;
        auto area = [sx, sy](juce::Rectangle<float> r) {
            return juce::Rectangle<float>(r.getX() * sx, r.getY() * sy, r.getWidth() * sx, r.getHeight() * sy).toNearestInt().expanded(1);
        };
        const auto label = juce::String(" at ") + juce::String(size.x) + "x" + juce::String(size.y);
        std::vector<juce::Component*> controls;
        collectControls(editor, editor, controls);
        check(controls.size() >= 120, ("main view exposes its controls" + label).toRawUTF8());
        int overlaps = 0;
        for (size_t i = 0; i < controls.size(); ++i)
            for (size_t j = i + 1; j < controls.size(); ++j)
                if (boundsIn(editor, *controls[i]).intersects(boundsIn(editor, *controls[j])))
                {
                    if (++overlaps <= 3)
                        std::printf("overlap%s: '%s' and '%s'\n", label.toRawUTF8(), controls[i]->getTitle().toRawUTF8(),
                                    controls[j]->getTitle().toRawUTF8());
                }
        check(overlaps == 0, ("no two visible main-view controls overlap" + label).toRawUTF8());
        for (auto* control : controls)
            if (! editor.getLocalBounds().contains(boundsIn(editor, *control)))
            {
                check(false, ("every control lies inside the editor" + label).toRawUTF8());
                break;
            }

        auto inside = [&](juce::Component* c, juce::Rectangle<float> block) {
            return c != nullptr && area(block).contains(boundsIn(editor, *c));
        };
        for (int index = cs80::P_kbdBrillianceLow; index <= cs80::P_kbdLevelHigh; ++index)
            check(inside(findParameterSlider(editor, index), geo::frontStrip),
                  ("keyboard-control levers sit on the front strip (Panel 2)" + label).toRawUTF8());
        for (const char* title : { "SUSTAIN ASSIGN", "PORTA / GLISS ASSIGN", "PEDAL EXP / WAH", "Foot controller position (expression)",
                                   "Tremolo/chorus ON/OFF", "Tremolo/chorus: TREMOLO", "Tremolo/chorus: CHORUS" })
            check(inside(findByTitle(editor, title), geo::deckLeft),
                  (juce::String("left-hand panel holds ") + title + label).toRawUTF8());
        for (const char* title : { "FOOTSWITCH PLUGGED", "FOOTSWITCH PRESSED", "OUTPUT HIGH / LOW" })
            check(inside(findByTitle(editor, title), geo::deckRight),
                  (juce::String("rear jack panel holds ") + title + label).toRawUTF8());
        check(inside(findByTitle(editor, "Open memory programmer"), geo::lidFace),
              ("the memory lid key sits on the closed lid at the upper left" + label).toRawUTF8());
        check(inside(findParameterSlider(editor, cs80::chParam(0, cs80::CP_fA)), geo::rowFrame(0))
                  && inside(findParameterSlider(editor, cs80::chParam(1, cs80::CP_fA)), geo::rowFrame(1)),
              ("channel rows sit right of the lid" + label).toRawUTF8());
    }
    check(findButtonText(editor, "ELECTRIC PIANO") != nullptr && findButtonText(editor, "HARPSICHORD 2") != nullptr
              && findButtonText(editor, "E. PIANO") == nullptr,
          "tone tabs carry the full hardware names");
}

// Separate ON/OFF, TREMOLO and CHORUS tabs over the unchanged chorus_on / chorus_mode parameters.
template <typename Check, typename Flush>
void testTremoloChorusTabs(Check check, Flush flush)
{
    using namespace panel_editor_detail;
    juce::TemporaryFile presetRoot("-cs80-panel-chorus");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    Cs80AudioProcessorEditor editor(host);
    editor.setSize(1600, 1000);
    auto* onOff = dynamic_cast<juce::Button*>(findByTitle(editor, "Tremolo/chorus ON/OFF"));
    auto* tremolo = dynamic_cast<juce::Button*>(findByTitle(editor, "Tremolo/chorus: TREMOLO"));
    auto* chorus = dynamic_cast<juce::Button*>(findByTitle(editor, "Tremolo/chorus: CHORUS"));
    check(onOff != nullptr && tremolo != nullptr && chorus != nullptr, "ON/OFF, TREMOLO and CHORUS tabs exist");
    check(findByTitle(editor, "Chorus section: OFF") == nullptr, "the three-way OFF/CHORUS/TREMOLO selector is gone");
    if (onOff == nullptr || tremolo == nullptr || chorus == nullptr) return;
    auto* on = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_chorusOn));
    auto* mode = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_chorusMode));
    check(on != nullptr && mode != nullptr, "chorus_on and chorus_mode keep their IDs");
    if (on == nullptr || mode == nullptr) return;
    host.setParamValue(cs80::P_chorusOn, 0.0f);
    host.setParamValue(cs80::P_chorusMode, 0.0f);
    flush(200);
    {
        BalancedGesture probe;
        on->addListener(&probe);
        onOff->triggerClick();
        flush(20);
        on->removeListener(&probe);
        check(host.getParamValue(cs80::P_chorusOn) == 1.0f && probe.ok() && onOff->getToggleState(),
              "ON/OFF turns the section on with one balanced gesture");
    }
    {
        BalancedGesture probe;
        mode->addListener(&probe);
        tremolo->triggerClick();
        flush(20);
        mode->removeListener(&probe);
        check(host.getParamValue(cs80::P_chorusMode) == 1.0f && host.getParamValue(cs80::P_chorusOn) == 1.0f
                  && probe.ok() && tremolo->getToggleState() && ! chorus->getToggleState(),
              "TREMOLO selects tremolo (chorus_mode 1) with one balanced gesture and leaves the section on");
    }
    chorus->triggerClick();
    flush(20);
    check(host.getParamValue(cs80::P_chorusMode) == 0.0f && chorus->getToggleState() && ! tremolo->getToggleState(),
          "CHORUS selects the chorus delay (chorus_mode 0)");
    onOff->triggerClick();
    flush(20);
    check(host.getParamValue(cs80::P_chorusOn) == 0.0f && host.getParamValue(cs80::P_chorusMode) == 0.0f,
          "ON/OFF turns the section off and keeps the tremolo/chorus choice");
    mode->setValueNotifyingHost(1.0f);
    check(waitFor(flush, [&] { return tremolo->getToggleState() && ! chorus->getToggleState() && ! onOff->getToggleState(); }),
          "host automation of chorus_mode moves the lit tab");
}

template <typename Check, typename Flush>
void runPanelEditorTests(Check check, Flush flush)
{
    testUnsavedEditIndicator(check, flush);
    testMainViewLayout(check, flush);
    testTremoloChorusTabs(check, flush);
}
} // namespace cs80::tests

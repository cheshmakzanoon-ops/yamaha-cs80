#pragma once

// Original-panel editor checks (ws7-panel), run by the UI self-test on its JUCE message thread.

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/PanelLayout.h"
#include "plugin/UiComponents.h"
#include "plugin/ArtCommon.h"
#include "plugin/EditorPreferences.h"

#include <chrono>
#include <cmath>
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

// The 10.5 px legend floor at the 1120 x 700 minimum, drawn in the embedded panel typeface.
template <typename Check>
void testLegendFloorAndTypeface(Check check)
{
    check(cs80::art::panelTypeface(false) != nullptr && cs80::art::panelTypeface(true) != nullptr,
          "both embedded panel typeface weights load");
    check(cs80::art::displayFont(12.0f, juce::Font::bold).getTypefaceName().contains("Barlow"),
          "panel legends use the embedded typeface");
    const float minimumScale = 1120.0f / 1600.0f;
    cs80::art::FloorScope floor(minimumScale);
    check(cs80::art::displayFont(6.0f).getHeight() * minimumScale >= cs80::art::kLegendFloorPx - 1.0e-3f,
          "design-space legends never print below 10.5 screen px at the minimum size");
}

// Display-size presets: a user preference in a scratch store, never a host parameter or session state.
template <typename Check, typename Flush>
void testZoomPresets(Check check, Flush flush)
{
    using cs80::panel::EditorPreferences;
    const auto previousStore = EditorPreferences::getStorageFileForTesting();
    const juce::TemporaryFile store(".settings");
    EditorPreferences::setStorageFileForTesting(store.getFile());
    juce::TemporaryFile presetRoot("-cs80-panel-zoom");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    const auto stateBefore = host.getAPVTS().copyState();
    {
        Cs80AudioProcessorEditor editor(host);
        editor.setZoomPercent(75);
        check(editor.getWidth() == 1200 && editor.getHeight() == 750 && editor.getZoomPercent() == 75,
              "the 75 % preset resizes the editor to 1200 x 750");
        check(EditorPreferences::loadZoomPercent() == 75, "the chosen display size is remembered");
        editor.setZoomPercent(110);
        check(editor.getWidth() == 1200 && EditorPreferences::loadZoomPercent() == 75, "a size that is not a preset is ignored");
        editor.setZoomPercent(200);
        check(editor.getWidth() == 3200 && editor.getHeight() == 2000 && editor.getZoomPercent() == 200,
              "the 200 % preset fits the resize limits");
        editor.setSize(1300, 813);
        check(editor.getZoomPercent() == 0, "a window dragged to its own size reports no preset");
        editor.setZoomPercent(75);
        flush(20);
    }
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    {
        Cs80AudioProcessorEditor reopened(host);
        if (display != nullptr && display->userArea.getWidth() >= 1232 && display->userArea.getHeight() >= 814)
            check(reopened.getWidth() == 1200 && reopened.getHeight() == 750, "a new editor opens at the remembered preset");
    }
    EditorPreferences::saveZoomPercent(200);
    {
        Cs80AudioProcessorEditor tooLarge(host);
        if (display != nullptr && (display->userArea.getWidth() < 3232 || display->userArea.getHeight() < 2064))
            check(tooLarge.getWidth() < 3200 && tooLarge.getWidth() <= juce::jmax(1120, display->userArea.getWidth()),
                  "a remembered preset larger than the screen falls back to the automatic size");
    }
    check(stateBefore.isEquivalentTo(host.getAPVTS().copyState()), "display size never changes host parameters or session state");
    EditorPreferences::setStorageFileForTesting(previousStore);
}

// HiDPI: the crinkle finish is tiled at one texel per physical pixel, so a 2x render carries as much fine detail
// per pixel as a 1x render instead of magnifying the 1x texture.
template <typename Check, typename Flush>
void testHiDpiTexture(Check check, Flush flush)
{
    juce::TemporaryFile presetRoot("-cs80-panel-hidpi");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    Cs80AudioProcessorEditor editor(host);
    editor.setSize(1120, 700);
    flush(30);
    // Plain deck finish left of the POLYPHONIC legend, in design units.
    const juce::Rectangle<float> patch(565.0f, 660.0f, 100.0f, 24.0f);
    auto detail = [&](float scale)
    {
        const auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
        const float s = scale * 1120.0f / 1600.0f;
        const auto box = juce::Rectangle<float>(patch.getX() * s, patch.getY() * s, patch.getWidth() * s, patch.getHeight() * s).toNearestInt();
        double sum = 0.0;
        int count = 0;
        for (int y = box.getY(); y < box.getBottom(); ++y)
            for (int x = box.getX(); x + 1 < box.getRight(); ++x)
            {
                sum += std::abs(image.getPixelAt(x + 1, y).getPerceivedBrightness() - image.getPixelAt(x, y).getPerceivedBrightness());
                ++count;
            }
        return count > 0 ? sum / count : 0.0;
    };
    const double oneX = detail(1.0f), twoX = detail(2.0f);
    std::printf("crinkle neighbour difference: 1x %.5f, 2x %.5f\n", oneX, twoX);
    check(oneX > 0.0 && twoX > 0.8 * oneX, "a 2x render keeps the crinkle at one texel per physical pixel");
}

template <typename Check, typename Flush>
void runPanelEditorTests(Check check, Flush flush)
{
    testUnsavedEditIndicator(check, flush);
    testMainViewLayout(check, flush);
    testTremoloChorusTabs(check, flush);
    testLegendFloorAndTypeface(check);
    testZoomPresets(check, flush);
    testHiDpiTexture(check, flush);
}
} // namespace cs80::tests

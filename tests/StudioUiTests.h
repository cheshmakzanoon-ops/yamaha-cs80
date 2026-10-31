#pragma once

// UI tests of the modern layer (workstream ws8-studio), run inside the headless snapshot tool
// (src/tools/ui_snapshot.cpp) on real editors: the Studio toolbar, every Studio tab and sub-page,
// the voice monitor and controllers strip, the interaction polish, MIDI learn, undo/redo, the
// learning tools, keyboard tab order and minimum hit targets. Every capture is written as a PNG
// next to the other snapshots for visual review.

#include "plugin/AdvancedPanel.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/StudioToolbar.h"
#include "plugin/UiComponents.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace cs80::tests::studio
{
using Check = std::function<void(bool, const char*)>;
using Flush = std::function<void(int)>;

inline juce::Component* findById(juce::Component& parent, const juce::String& id)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getComponentID() == id)
            return child;
        if (auto* found = findById(*child, id))
            return found;
    }
    return nullptr;
}

template <class T>
inline T* findFirst(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* hit = dynamic_cast<T*>(child))
            return hit;
        if (auto* found = findFirst<T>(*child))
            return found;
    }
    return nullptr;
}

inline bool isShowingIn(juce::Component& c) { return c.isShowing() || (c.isVisible() && c.getParentComponent() != nullptr && c.getParentComponent()->isVisible()); }

inline void save(juce::Component& editor, const juce::File& dir, const juce::String& name, const Check& check)
{
    auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
    check(! image.isNull() && image.getWidth() == editor.getWidth(), "studio snapshot image created");
    auto file = dir.getChildFile(name + ".png");
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream(image, stream);
    check(ok, "studio snapshot written");
    if (ok)
        std::printf("wrote %s\n", file.getFullPathName().toRawUTF8());
}

// Finds the Studio view inside an editor.
inline cs80::ui::StudioView* studioOf(juce::Component& editor) { return findFirst<cs80::ui::StudioView>(editor); }

// ---- Construction, painting and resizing of the toolbar, the Studio tabs and the drawers ---------------------
inline void testStudioViewsPaint(const Check& check, const Flush& flush, const juce::File& dir)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    Cs80AudioProcessorEditor editor(processor);
    editor.setVisible(true);
    auto* studio = studioOf(editor);
    check(studio != nullptr, "the editor mounts the Studio view");
    auto* toolbar = findFirst<cs80::ui::StudioToolbar>(editor);
    check(toolbar != nullptr, "the editor mounts the Studio toolbar");
    if (studio == nullptr || toolbar == nullptr)
        return;

    for (const auto size : { juce::Point<int>(1600, 1000), juce::Point<int>(1120, 700) })
    {
        editor.setSize(size.x, size.y);
        flush(80);
        const auto suffix = "-" + juce::String(size.x) + "x" + juce::String(size.y);
        check(toolbar->isVisible() && ! toolbar->getBounds().isEmpty(), "toolbar is visible at both sizes");
        check(editor.getLocalBounds().contains(toolbar->getBoundsInParent()), "toolbar lies inside the editor");
        for (const char* id : { "toolbar_previous", "toolbar_program", "toolbar_next", "toolbar_library", "toolbar_compare_a",
                                "toolbar_compare_b", "toolbar_undo", "toolbar_redo", "toolbar_engine_circuit", "toolbar_engine_legacy",
                                "toolbar_pure_cs80", "toolbar_studio", "toolbar_menu" })
        {
            auto* key = findById(*toolbar, id);
            check(key != nullptr && key->isVisible() && toolbar->getLocalBounds().contains(key->getBounds()),
                  "every toolbar key exists and lies inside the toolbar");
        }
        save(editor, dir, "studio-main" + suffix, check);

        editor.showAdvancedPanel(true);
        check(studio->isVisible(), "toolbar STUDIO path opens the Studio view");
        for (int t = 0; t < (int) cs80::ui::StudioTab::count; ++t)
        {
            const auto tab = (cs80::ui::StudioTab) t;
            for (int s = 0; s < studio->getNumSubPages(tab); ++s)
            {
                studio->showTab(tab, s);
                flush(60);
                check(studio->getActiveTab() == tab && studio->getActiveSubPage() == s, "Studio tab and sub-page selection");
                auto* page = studio->getSubPage(tab, s);
                check(page != nullptr && page->isVisible() && ! page->getBounds().isEmpty(), "Studio sub-page is visible with bounds");
                save(editor, dir, "studio-" + cs80::ui::StudioView::tabName(tab).toLowerCase()
                                      + (s > 0 ? "-" + juce::String(s + 1) : juce::String()) + suffix, check);
            }
        }
        editor.showAdvancedPanel(false);
        flush(30);
    }
    editor.setSize(1600, 1000);
    flush(30);

    // The voice monitor drawer and the controllers strip, opened through the toolbar's own actions.
    const auto& actions = toolbar->getActions();
    auto* scope = findById(editor, "voice_monitor");
    auto* controllers = findById(editor, "controllers_strip");
    check(scope != nullptr && controllers != nullptr, "voice monitor and controllers strip are mounted");
    check(actions.toggleScope && actions.toggleControllers, "toolbar menu reaches the drawers");
    if (scope == nullptr || controllers == nullptr || ! actions.toggleScope || ! actions.toggleControllers)
        return;
    check(! scope->isVisible() && ! controllers->isVisible(), "drawers are closed by default: the panel stays original");
    actions.toggleScope();
    actions.toggleControllers();
    processor.getKeyboardState().noteOn(1, 60, 0.8f);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    processor.processBlock(audio, midi);
    flush(120);
    check(scope->isVisible() && controllers->isVisible(), "menu actions open the voice monitor and the controllers strip");
    for (const auto size : { juce::Point<int>(1600, 1000), juce::Point<int>(1120, 700) })
    {
        editor.setSize(size.x, size.y);
        flush(80);
        check(editor.getLocalBounds().contains(scope->getBoundsInParent())
                  && editor.getLocalBounds().contains(controllers->getBoundsInParent()),
              "drawers lie inside the editor at both sizes");
        save(editor, dir, "studio-drawers-" + juce::String(size.x) + "x" + juce::String(size.y), check);
    }
    processor.getKeyboardState().noteOff(1, 60, 0.0f);
    // PURE CS-80 hides the controllers strip, whatever the player chose; switching it off brings it back.
    processor.setParamValue(cs80::P_parityMode, 1.0f);
    flush(300);
    check(! controllers->isVisible(), "PURE CS-80 hides the controllers strip");
    processor.setParamValue(cs80::P_parityMode, 0.0f);
    flush(300);
    check(controllers->isVisible(), "leaving PURE CS-80 restores the chosen controllers strip");
    actions.toggleScope();
    actions.toggleControllers();
    flush(150);
    check(! scope->isVisible() && ! controllers->isVisible(), "drawers close again");
    editor.setSize(1600, 1000);
}

} // namespace cs80::tests::studio

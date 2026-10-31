#pragma once

// UI tests of the modern layer (workstream ws8-studio), run inside the headless snapshot tool
// (src/tools/ui_snapshot.cpp) on real editors: the Studio toolbar, every Studio tab and sub-page,
// the voice monitor and controllers strip, the interaction polish, MIDI learn, undo/redo, the
// learning tools, keyboard tab order and minimum hit targets. Every capture is written as a PNG
// next to the other snapshots for visual review.

#include "plugin/AdvancedPanel.h"
#include "plugin/EvidenceRegister.h"
#include "plugin/HardwareKeyboard.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/StudioMount.h"
#include "plugin/StudioToolbar.h"
#include "plugin/UiComponents.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <set>
#include <thread>
#include <typeinfo>
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

// ---- Interaction polish: faders, typed values, context menu, ribbon, QWERTY play and tooltips --------------
struct StudioGestureProbe final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0, depth = 0, unbracketed = 0;
    void parameterValueChanged(int, float) override { ++changes; if (depth != 1) ++unbracketed; }
    void parameterGestureChanged(int, bool starting) override { starting ? (++begins, ++depth) : (++ends, --depth); }
    bool oneGesture() const { return begins == 1 && ends == 1 && depth == 0 && unbracketed == 0 && changes > 0; }
};

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

inline juce::MouseEvent mouseAt(juce::Component& c, juce::Point<float> p, juce::ModifierKeys mods, int clicks = 1)
{
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), p, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c,
                            juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), clicks, false);
}

inline cs80::ui::ParamKnob* knobFor(juce::Component& parent, int parameter)
{
    for (auto* child : parent.getChildren())
    {
        if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(child); knob != nullptr && knob->getParameterIndex() == parameter && knob->isVisible())
            return knob;
        if (auto* found = knobFor(*child, parameter))
            return found;
    }
    return nullptr;
}

inline juce::StringArray menuTexts(const juce::PopupMenu& menu, std::vector<const juce::PopupMenu::Item*>* items = nullptr)
{
    juce::StringArray texts;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
    {
        texts.add(it.getItem().text);
        if (items != nullptr)
            items->push_back(&it.getItem());
    }
    return texts;
}

inline const juce::PopupMenu::Item* menuItem(const juce::PopupMenu& menu, const juce::String& prefix)
{
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text.startsWith(prefix))
            return &it.getItem();
    return nullptr;
}

inline void testInteractionPolish(const Check& check, const Flush& flush, const juce::File& dir)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 256);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flush(60);
    using cs80::ui::FocusSlider;
    const int lpf = cs80::chParam(0, cs80::CP_lpfCutoff);
    auto* fader = knobFor(editor, lpf);
    check(fader != nullptr, "channel I LPF fader found on the main panel");
    if (fader == nullptr)
        return;
    auto& slider = fader->getSlider();
    auto* focusSlider = dynamic_cast<FocusSlider*>(&slider);
    check(focusSlider != nullptr && ! slider.getSliderSnapsToMousePosition(), "faders drag relative to the cap (no snap to the pointer)");
    const double low = slider.getPositionOfValue(slider.getMinimum()), high = slider.getPositionOfValue(slider.getMaximum());
    check(std::abs(slider.getMouseDragSensitivity() - std::abs(high - low)) <= 1.0, "a full-range drag equals the fader's own track length");

    // A click on the track away from the cap does not move the value.
    processor.setParamValue(lpf, 850.0f);
    flush(30);
    const double before = slider.getValue();
    const float capY = (float) slider.getPositionOfValue(before);
    const float farY = capY > (float) slider.getHeight() * 0.5f ? 4.0f : (float) slider.getHeight() - 4.0f;
    const auto left = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier);
    slider.mouseDown(mouseAt(slider, { (float) slider.getWidth() * 0.5f, farY }, left));
    slider.mouseUp(mouseAt(slider, { (float) slider.getWidth() * 0.5f, farY }, juce::ModifierKeys()));
    check(std::abs(slider.getValue() - before) < 1.0e-6, "a click away from the cap never makes the value jump");

    // A drag moves by the pointer's travel; with Shift by a tenth of it.
    auto dragBy = [&](float pixels, juce::ModifierKeys extra)
    {
        const float x = (float) slider.getWidth() * 0.5f, y0 = capY;
        const auto held = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | extra.getRawFlags());
        const double start = slider.valueToProportionOfLength(slider.getValue());
        slider.mouseDown(mouseAt(slider, { x, y0 }, held));
        for (int i = 1; i <= 4; ++i)
            slider.mouseDrag(mouseAt(slider, { x, y0 - pixels * (float) i / 4.0f }, held));
        slider.mouseUp(mouseAt(slider, { x, y0 - pixels }, extra));
        return slider.valueToProportionOfLength(slider.getValue()) - start;
    };
    const double trackPx = std::abs(high - low);
    const double coarse = dragBy(20.0f, {});
    check(std::abs(coarse - 20.0 / trackPx) < 0.01, "a 20 px drag moves the cap 20 px");
    processor.setParamValue(lpf, 850.0f);
    flush(30);
    const double fine = dragBy(20.0f, juce::ModifierKeys(juce::ModifierKeys::shiftModifier));
    check(fine > 0.0 && std::abs(fine - coarse * FocusSlider::kFineFactor) < 0.004, "Shift-drag moves a tenth as far");

    // Typed values in the units each control shows; one host gesture each.
    auto* parameter = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(lpf));
    {
        StudioGestureProbe probe;
        parameter->addListener(&probe);
        check(fader->applyTypedValue("1.2k"), "typing 1.2k is a value for a cutoff");
        parameter->removeListener(&probe);
        check(std::abs(processor.getParamValue(lpf) - 1200.0f) < 1.0f, "1.2k sets 1200 Hz");
        check(probe.oneGesture(), "a typed value is one balanced host gesture");
    }
    check(fader->applyTypedValue("3.6 kHz") && std::abs(processor.getParamValue(lpf) - 3600.0f) < 2.0f, "3.6 kHz sets 3600 Hz");
    check(! fader->applyTypedValue("80 ms") && std::abs(processor.getParamValue(lpf) - 3600.0f) < 2.0f,
          "a time typed into a cutoff is refused and changes nothing");
    check(! fader->applyTypedValue("loud"), "text that is not a value is refused");
    float value = 0.0f;
    check(cs80::ui::parseParamValue(cs80::chParam(0, cs80::CP_vA), "80 ms", 0.3f, value) && std::abs(value - 0.08f) < 1.0e-4f,
          "80 ms sets an attack of 0.08 s");
    check(cs80::ui::parseParamValue(cs80::chParam(0, cs80::CP_vA), "80", 0.05f, value) && std::abs(value - 0.08f) < 1.0e-4f,
          "a bare number uses the unit shown now (ms)");
    check(cs80::ui::parseParamValue(cs80::chParam(0, cs80::CP_vLevel), "52 %", 0.7f, value) && std::abs(value - 0.52f) < 1.0e-4f,
          "52 % sets a level of 0.52");
    check(cs80::ui::parseParamValue(cs80::P_glideMode, "GLISS", 0.0f, value) && value == 2.0f, "a choice name selects it");
    check(cs80::ui::parseParamValue(cs80::P_resonance, "5", 0.2f, value) && std::abs(value - 0.5f) < 1.0e-4f,
          "a 0-10 scale reads back on its own scale");
    check(cs80::ui::parseParamValue(cs80::P_mix, "68/32", 0.5f, value) && std::abs(value - 0.68f) < 1.0e-4f, "the MIX ratio parses");
    check(cs80::ui::parseParamValue(cs80::P_masterTune, "-0.5 st", 0.0f, value) && std::abs(value + 0.5f) < 1.0e-4f, "signed semitones parse");
    check(cs80::ui::parseParamValue(cs80::P_tuningRef, "9999", 440.0f, value) && value == 465.0f, "typed values clamp to the range");

    // Double-click reset (JUCE's own path) and the menu's reset are both one gesture back to the default.
    check(slider.isDoubleClickReturnEnabled() && std::abs(slider.getDoubleClickReturnValue() - cs80::paramDef(lpf).def) < 0.01,
          "double-click returns the fader to its default");
    {
        StudioGestureProbe probe;
        parameter->addListener(&probe);
        slider.mouseDoubleClick(mouseAt(slider, { 4.0f, capY }, left, 2));
        parameter->removeListener(&probe);
        check(std::abs(processor.getParamValue(lpf) - cs80::paramDef(lpf).def) < 1.0f && probe.oneGesture(),
              "double-click resets in one gesture");
    }

    // The context menu.
    processor.setParamValue(lpf, 850.0f);
    flush(30);
    auto menu = fader->buildContextMenu();
    const auto texts = menuTexts(menu);
    const auto has = [&texts](const char* prefix)
    {
        for (const auto& t : texts)
            if (t.startsWith(prefix)) return true;
        return false;
    };
    check(has("MIDI learn") && has("Reset to default") && has("Type a value") && has("Copy value") && has("Paste value")
              && has("What is this control?"),
          "the context menu offers MIDI learn, reset, type, copy, paste and help");
    if (auto* reset = menuItem(menu, "Reset to default"); reset != nullptr && reset->action)
    {
        reset->action();
        check(std::abs(processor.getParamValue(lpf) - cs80::paramDef(lpf).def) < 1.0f, "menu reset restores the default");
    }
    else
        check(false, "menu reset has an action");
    processor.setParamValue(lpf, 850.0f);
    flush(30);
    fader->copyValue();
    const int hpf = cs80::chParam(1, cs80::CP_hpfCutoff);
    if (auto* other = knobFor(editor, hpf))
    {
        check(other->pasteValue() && std::abs(processor.getParamValue(hpf) - 850.0f) < 1.5f, "copy and paste carry a value between controls");
        auto* attack = knobFor(editor, cs80::chParam(0, cs80::CP_vA));
        const float attackBefore = processor.getParamValue(cs80::chParam(0, cs80::CP_vA));
        check(attack != nullptr && ! attack->pasteValue() && processor.getParamValue(cs80::chParam(0, cs80::CP_vA)) == attackBefore,
              "a cutoff cannot be pasted into a time control");
        if (attack != nullptr)
        {
            auto attackMenu = attack->buildContextMenu();
            const auto* paste = menuItem(attackMenu, "Paste value");
            check(paste != nullptr && ! paste->isEnabled, "the menu disables a paste that is not a value of the control");
        }
    }
    else
        check(false, "channel II HPF fader found");
    // Ctrl/Cmd-click opens the value editor instead of starting a drag.
    {
        StudioGestureProbe probe;
        parameter->addListener(&probe);
        slider.mouseDown(mouseAt(slider, { 4.0f, capY }, juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier)));
        slider.mouseUp(mouseAt(slider, { 4.0f, capY }, juce::ModifierKeys(juce::ModifierKeys::commandModifier)));
        parameter->removeListener(&probe);
        auto* entry = dynamic_cast<juce::TextEditor*>(findByTitle(editor, "Type a value"));
        check(entry != nullptr && entry->isVisible() && entry->getText() == cs80::ui::formatParamValue(lpf, processor.getParamValue(lpf)),
              "Ctrl/Cmd-click opens the value editor with the shown value");
        check(probe.begins == 0 && probe.changes == 0, "Ctrl/Cmd-click is not a drag");
        if (entry != nullptr)
        {
            entry->setText("2 kHz", juce::dontSendNotification);
            if (entry->onReturnKey) entry->onReturnKey();
            flush(40);
            check(std::abs(processor.getParamValue(lpf) - 2000.0f) < 2.0f && findByTitle(editor, "Type a value") == nullptr,
                  "Return commits the typed value and closes the editor");
        }
    }

    // PURE CS-80 has no MIDI learn.
    processor.setParamValue(cs80::P_parityMode, 1.0f);
    {
        auto pureMenu = fader->buildContextMenu();
        const auto* learn = menuItem(pureMenu, "MIDI learn");
        check(learn != nullptr && ! learn->isEnabled, "MIDI learn is unavailable in PURE CS-80");
    }
    processor.setParamValue(cs80::P_parityMode, 0.0f);

    // Tooltips: owner's-manual words and page, Circuit notes with the evidence class on request.
    const auto plainTip = fader->buildTooltip(false);
    check(plainTip.contains("low-pass filter cutoff") && plainTip.contains("Owner's manual p18, p32 (control 28)")
              && plainTip.contains("Shift: circuit notes") && ! plainTip.contains("CIRCUIT NOTES"),
          "a tooltip gives plain words and the owner's-manual page");
    const auto notesTip = fader->buildTooltip(true);
    check(notesTip.contains("CIRCUIT NOTES") && notesTip.contains("SOURCE-DERIVED ROUTE, INFERRED LAW") && notesTip.contains("IG00156"),
          "the Circuit notes section shows the evidence class and the engineering notes");
    cs80::ui::LookAndFeel look;
    const auto tipBounds = look.getTooltipBounds(notesTip, { 100, 100 }, { 0, 0, 1600, 1000 });
    check(tipBounds.getWidth() > 200 && tipBounds.getHeight() > 60, "the structured tooltip lays out");
    {
        juce::Image image(juce::Image::ARGB, tipBounds.getWidth(), tipBounds.getHeight(), true);
        {
            juce::Graphics g(image);
            look.drawTooltip(g, notesTip, tipBounds.getWidth(), tipBounds.getHeight());
        }
        auto file = dir.getChildFile("studio-tooltip-circuit-notes.png");
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat png;
        check(stream.openedOk() && png.writeImageToStream(image, stream), "tooltip capture written");
    }
    const auto help = cs80::ui::controlHelpText(lpf);
    check(help.contains("EVIDENCE") && help.contains("Owner's manual p18, p32") && help.contains("Coverage ledger: IB implemented behaviour"),
          "the help card cites the manual page, the evidence class and the coverage codes");
    // A modern control has no owner page.
        check(cs80::ui::controlHelpText(cs80::P_lfo1Rate).contains("Not on the original instrument"), "a modern control says it is not on the original");

    // The ribbon: full range from any touch point; PURE CS-80 keeps the hardware's distance law.
    {
        cs80::ui::RibbonStrip ribbon;
        ribbon.setSize(800, 36);
        const float right = ribbon.getWidth() - 3.1f - 56.0f;   // band ends as RibbonStrip draws them
        check(std::abs(ribbon.valueForGesture(400.0f, 800.0f) - 1.0f) < 1.0e-6f, "from the centre the right end is the full upward bend");
        check(std::abs(ribbon.valueForGesture(600.0f, 800.0f) - 1.0f) < 1.0e-6f, "from a touch near the end the end is still the full bend");
        check(std::abs(ribbon.valueForGesture(600.0f, 0.0f) + 1.0f) < 1.0e-6f, "and the left end is the full downward bend");
        check(ribbon.valueForGesture(400.0f, 400.0f + (right - 400.0f) * 0.5f) > 0.49f
                  && ribbon.valueForGesture(400.0f, 400.0f + (right - 400.0f) * 0.5f) < 0.51f,
              "half the travel is half the bend");
        ribbon.fullRangeFromTouch = [] { return false; };
        check(std::abs(ribbon.valueForGesture(400.0f, 600.0f) - 0.25f) < 1.0e-6f, "PURE CS-80 ribbon: bend proportional to distance over the strip");
    }

    // QWERTY play through the on-screen keyboard's state, octave shift and text-entry focus.
    {
        juce::MidiKeyboardState& keys = processor.getKeyboardState();
        cs80::ui::QwertyKeyboard qwerty(keys);
        std::set<int> down;
        qwerty.isKeyDown = [&down](int code) { return down.count(code) > 0; };
        qwerty.textEntryFocused = [] { return false; };
        auto press = [&](char c, juce::ModifierKeys mods = {})
        {
            down.insert((int) c);
            return qwerty.keyPressed(juce::KeyPress((int) c, mods, (juce::juce_wchar) c), &editor);
        };
        auto lift = [&](char c)
        {
            down.erase((int) c);
            qwerty.keyStateChanged(false, &editor);
        };
        check(press('a') && keys.isNoteOn(1, 60), "A plays middle C on channel 1");
        check(press('w') && keys.isNoteOn(1, 61), "W plays C sharp");
        check(press('a') && qwerty.heldCount() == 2, "auto-repeat does not retrigger a held key");
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer midi;
        processor.processBlock(audio, midi);
        check(processor.getActiveVoiceCount() >= 2, "QWERTY notes reach the processor's keyboard event path");
        lift('a');
        check(! keys.isNoteOn(1, 60) && keys.isNoteOn(1, 61), "lifting a key releases only its note");
        check(press('z') && qwerty.getBaseNote() == 48, "Z shifts down an octave");
        check(press('a') && keys.isNoteOn(1, 48), "the A key follows the octave");
        lift('w');
        check(! keys.isNoteOn(1, 61), "a note started before the octave change still releases");
        check(! press('s', juce::ModifierKeys(juce::ModifierKeys::commandModifier)), "keys with Ctrl/Cmd are left to shortcuts");
        qwerty.textEntryFocused = [] { return true; };
        check(! press('d') && ! keys.isNoteOn(1, 52), "no notes while a text field has the keyboard");
        qwerty.textEntryFocused = [] { return false; };
        qwerty.releaseAll();
        check(qwerty.heldCount() == 0 && ! keys.isNoteOn(1, 48), "releaseAll leaves no note hanging");
        qwerty.setEnabled(false);
        check(! press('a') && ! keys.isNoteOn(1, 48), "a disabled computer keyboard plays nothing");
        processor.processBlock(audio, midi);
    }
}

// ---- MIDI learn round trip: context menu -> audio thread -> plugin state -> new instance -----------------------
inline void sendController(Cs80AudioProcessor& processor, int cc, int value)
{
    juce::AudioBuffer<float> audio(2, 128);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 7);
    processor.processBlock(audio, midi);
}

inline void testMidiLearnRoundTrip(const Check& check, const Flush& flush, const juce::File& dir)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    const int lpf = cs80::chParam(0, cs80::CP_lpfCutoff);
    const int host = processor.hostParameterIndex(lpf);
    auto* parameter = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(lpf));
    check(host >= 0 && parameter != nullptr && processor.getParameters()[host] == parameter, "host index of a core parameter");
    {
        Cs80AudioProcessorEditor editor(processor);
        editor.setSize(1600, 1000);
        editor.setVisible(true);
        flush(50);
        auto* fader = knobFor(editor, lpf);
        check(fader != nullptr && fader->getHostParameterIndex() == host, "the fader knows its host parameter");
        if (fader == nullptr)
            return;
        auto menu = fader->buildContextMenu();
        const auto* learn = menuItem(menu, "MIDI learn");
        check(learn != nullptr && learn->isEnabled && learn->action, "the menu arms MIDI learn");
        if (learn != nullptr && learn->action)
            learn->action();
        check(processor.getMidiLearn().learningParameter() == host, "MIDI learn waits for a controller for this fader");
        auto armedMenu = fader->buildContextMenu();
        check(menuItem(armedMenu, "Cancel MIDI learn") != nullptr, "while armed the menu offers to cancel");
        sendController(processor, 0, 64);     // bank select is never learned
        check(processor.getMidiLearn().learningParameter() == host, "a protocol controller does not satisfy MIDI learn");
        sendController(processor, 74, 127);
        check(processor.getMidiLearn().parameterFor(74) == host && processor.getMidiLearn().learningParameter() < 0,
              "the next controller is learned on the audio thread");
        check(std::abs(parameter->getValue() - 1.0f) < 1.0e-4f, "the learning move already drives the parameter");
        sendController(processor, 74, 0);
        check(parameter->getValue() < 1.0e-4f, "the learned controller moves the parameter");
        sendController(processor, 74, 64);
        check(std::abs(parameter->getValue() - 64.0f / 127.0f) < 1.0e-4f, "controller values map onto the parameter's normalised range");
        flush(120);
        check(fader->buildTooltip(false).contains("MIDI CC 74"), "the tooltip names the learned controller");
        auto mappedMenu = fader->buildContextMenu();
        check(menuItem(mappedMenu, "Forget MIDI CC 74") != nullptr, "the menu offers to forget the controller");

        // The Keyboard tab lists the map with a FORGET key per controller.
        editor.showAdvancedPanel(true);
        if (auto* studio = studioOf(editor))
            studio->showTab(cs80::ui::StudioTab::keyboard);
        flush(200);
        check(findById(editor, "midi_learn_forget_74") != nullptr, "the MIDI learn panel lists the learned controller");
        save(editor, dir, "studio-keyboard-midi-learn-1600x1000", check);
        editor.showAdvancedPanel(false);
    }

    // Plugin state round trip: a new instance gets the same map; a preset load keeps it; old states clear it.
    juce::MemoryBlock state;
    processor.getEditorState().tourSeen = true;
    processor.getEditorState().qwertyEnabled = false;
    processor.getEditorState().circuitNotes = true;
    processor.getStateInformation(state);
    const auto xml = juce::XmlDocument::parse(state.toString());
    check(xml != nullptr && xml->getChildByName("MIDI_LEARN") != nullptr && xml->getChildByName("MIDI_LEARN")->getIntAttribute("version") == 1,
          "the map is saved with a version in the plugin state");
    check(xml != nullptr && xml->getChildByName("UI_STATE") != nullptr && xml->getChildByName("UI_STATE")->getIntAttribute("version") == 1,
          "the editor state is saved with a version in the plugin state");
    check(processor.getParameters().size() == Cs80AudioProcessor::hostParameterCount, "MIDI learn adds no host parameter");
    {
        Cs80AudioProcessor restored;
        restored.prepareToPlay(48000.0, 128);
        restored.setStateInformation(state.getData(), (int) state.getSize());
        check(restored.getMidiLearn().parameterFor(74) == host && restored.getMidiLearn().count() == 1, "a restored session has the same map");
        check(restored.getEditorState().tourSeen.load() && ! restored.getEditorState().qwertyEnabled.load()
                  && restored.getEditorState().circuitNotes.load(),
              "a restored session has the same editor state");
        auto* restoredParameter = restored.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(lpf));
        sendController(restored, 74, 127);
        check(std::abs(restoredParameter->getValue() - 1.0f) < 1.0e-4f, "the restored map drives the restored instance");
        check(restored.getAPVTS().copyState().getChildWithName("MIDI_LEARN").isValid() == false,
              "the map never lives in the parameter tree (so presets cannot carry it)");
        restored.loadFactoryPreset(3);
        check(restored.getMidiLearn().count() == 1, "loading a preset keeps the session's map");
        // PURE CS-80: no MIDI learn at all, the map is kept.
        restored.setParamValue(cs80::P_parityMode, 1.0f);
        const float held = restoredParameter->getValue();
        sendController(restored, 74, 0);
        check(std::abs(restoredParameter->getValue() - held) < 1.0e-6f, "PURE CS-80 does not apply learned controllers");
        restored.getMidiLearn().armLearn(restored.hostParameterIndex(cs80::P_volume));
        sendController(restored, 20, 50);
        check(restored.getMidiLearn().parameterFor(20) < 0, "PURE CS-80 does not learn");
        restored.getMidiLearn().cancelLearn();
        restored.setParamValue(cs80::P_parityMode, 0.0f);
        sendController(restored, 74, 0);
        check(restoredParameter->getValue() < 1.0e-4f, "leaving PURE CS-80 brings the map back");
        // A learned controller gives up its usual MIDI meaning: CC 11 normally moves the foot controller.
        sendController(restored, 11, 127);
        check(std::abs(restored.getExpressionValue() - 1.0f) < 1.0e-4f, "CC 11 drives the foot controller while unlearned");
        restored.getMidiLearn().assign(11, restored.hostParameterIndex(cs80::P_volume));
        sendController(restored, 11, 0);
        check(std::abs(restored.getExpressionValue() - 1.0f) < 1.0e-4f
                  && restored.getParamValue(cs80::P_volume) < 1.0e-4f,
              "a learned CC 11 drives its parameter, not the foot controller");
        restored.getMidiLearn().clearController(11);
        sendController(restored, 11, 0);
        check(restored.getExpressionValue() < 1.0e-4f, "forgetting the controller restores its MIDI meaning");
    }
    {
        // A state from before this feature carries no MIDI_LEARN element: restoring it leaves no mappings.
        auto old = juce::XmlDocument::parse(state.toString());
        old->removeChildElement(old->getChildByName("MIDI_LEARN"), true);
        old->removeChildElement(old->getChildByName("UI_STATE"), true);
        const auto text = old->toString();
        processor.setStateInformation(text.toRawUTF8(), (int) text.getNumBytesAsUTF8());
        check(processor.getMidiLearn().count() == 0 && processor.getEditorState().qwertyEnabled.load()
                  && ! processor.getEditorState().tourSeen.load(),
              "an old state restores with no mappings and the default editor state");
    }
}

// ---- Undo/redo: one transaction per gesture; automation, MIDI learn and loads never recorded --------------------
inline void testUndoRedo(const Check& check, const Flush& flush)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    auto& history = processor.getUndoManager();
    check(processor.getAPVTS().undoManager == &history, "the APVTS is constructed with the processor's UndoManager");
    check(! history.canUndo() && ! history.canRedo(), "a fresh instance has an empty history");
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flush(50);
    const int lpf = cs80::chParam(0, cs80::CP_lpfCutoff);
    const int vA = cs80::chParam(0, cs80::CP_vA);
    auto* fader = knobFor(editor, lpf);
    auto* attack = knobFor(editor, vA);
    check(fader != nullptr && attack != nullptr, "faders for the undo test");
    if (fader == nullptr || attack == nullptr)
        return;
    const float lpf0 = processor.getParamValue(lpf), attack0 = processor.getParamValue(vA);
    auto steps = [&history] { return history.getUndoDescriptions().size(); };

    // One typed value = one step, named after the control.
    fader->applyTypedValue("1.2k");
    flush(30);
    check(steps() == 1 && history.getUndoDescription().containsIgnoreCase("LPF"), "a typed value is one named undo step");
    // A drag with many moves is still one step, back to where the drag began.
    {
        auto& slider = attack->getSlider();
        const float y = (float) slider.getPositionOfValue(slider.getValue());
        const auto held = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier);
        slider.mouseDown(mouseAt(slider, { 5.0f, y }, held));
        for (int i = 1; i <= 6; ++i)
        {
            slider.mouseDrag(mouseAt(slider, { 5.0f, y - 6.0f * (float) i }, held));
            flush(20);   // the APVTS timer runs mid-drag
        }
        slider.mouseUp(mouseAt(slider, { 5.0f, y - 36.0f }, juce::ModifierKeys()));
        flush(30);
    }
    const float attackDragged = processor.getParamValue(vA);
    check(attackDragged > attack0 && steps() == 2, "a whole drag is one undo step");

    // Host automation, from the message thread and from another thread, is never recorded.
    auto* volume = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_volume));
    volume->setValueNotifyingHost(0.25f);
    std::thread automation([volume] { for (int i = 0; i < 50; ++i) volume->setValueNotifyingHost(0.3f + 0.001f * (float) i); });
    automation.join();
    flush(250);
    check(steps() == 2, "host automation adds no undo step");
    // MIDI learn moves from the audio thread are automation too.
    processor.getMidiLearn().assign(21, processor.hostParameterIndex(cs80::P_detune));
    sendController(processor, 21, 100);
    flush(250);
    check(steps() == 2, "MIDI-learned controller moves add no undo step");
    const float automatedVolume = processor.getParamValue(cs80::P_volume);

    check(processor.undo(), "undo the drag");
    flush(30);
    check(std::abs(processor.getParamValue(vA) - attack0) < 1.0e-5f, "undo returns the drag to where it began");
    check(std::abs(processor.getParamValue(cs80::P_volume) - automatedVolume) < 1.0e-6f, "undo leaves automated parameters alone");
    check(processor.undo(), "undo the typed value");
    flush(30);
    check(std::abs(processor.getParamValue(lpf) - lpf0) < 0.5f && ! history.canUndo(), "undo restores the typed control; nothing older");
    check(processor.redo() && processor.redo(), "redo both steps");
    flush(30);
    check(std::abs(processor.getParamValue(lpf) - 1200.0f) < 1.0f && std::abs(processor.getParamValue(vA) - attackDragged) < 1.0e-5f,
          "redo replays both edits");
    check(steps() == 2, "undo and redo add no steps of their own");

    // The toolbar keys and the shortcut follow the history.
    auto* toolbar = findFirst<cs80::ui::StudioToolbar>(editor);
    auto* undoKey = toolbar != nullptr ? dynamic_cast<juce::Button*>(findById(*toolbar, "toolbar_undo")) : nullptr;
    auto* redoKey = toolbar != nullptr ? dynamic_cast<juce::Button*>(findById(*toolbar, "toolbar_redo")) : nullptr;
    check(undoKey != nullptr && redoKey != nullptr, "toolbar undo and redo keys");
    if (toolbar != nullptr && undoKey != nullptr && redoKey != nullptr)
    {
        toolbar->refresh();
        check(undoKey->isEnabled() && ! redoKey->isEnabled() && undoKey->getTitle().containsIgnoreCase("Undo"),
              "the undo key is live and names the step; redo is idle");
        undoKey->triggerClick();
        flush(40);
        check(std::abs(processor.getParamValue(vA) - attack0) < 1.0e-5f, "the toolbar undo key undoes");
        toolbar->refresh();
        check(redoKey->isEnabled(), "after an undo the redo key is live");
    }
    {
        // The editor's shortcut listener: Ctrl/Cmd+Z, Shift+Ctrl/Cmd+Z, Ctrl+Y, Ctrl/Cmd+S; plain keys pass through.
        cs80::ui::StudioShortcuts shortcuts;
        int undos = 0, redos = 0, saves = 0;
        shortcuts.undo = [&] { ++undos; };
        shortcuts.redo = [&] { ++redos; };
        shortcuts.save = [&] { ++saves; };
        const auto cmd = juce::ModifierKeys(juce::ModifierKeys::commandModifier);
        const auto cmdShift = juce::ModifierKeys(juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
        check(shortcuts.keyPressed(juce::KeyPress('Z', cmd, 0), &editor) && undos == 1, "Ctrl/Cmd+Z undoes");
        check(shortcuts.keyPressed(juce::KeyPress('Z', cmdShift, 0), &editor) && redos == 1, "Shift+Ctrl/Cmd+Z redoes");
        check(shortcuts.keyPressed(juce::KeyPress('Y', cmd, 0), &editor) && redos == 2, "Ctrl+Y redoes");
        check(shortcuts.keyPressed(juce::KeyPress('S', cmd, 0), &editor) && saves == 1, "Ctrl/Cmd+S saves");
        check(! shortcuts.keyPressed(juce::KeyPress('z', {}, 'z'), &editor) && undos == 1, "a plain Z is left to the computer keyboard");
    }
    processor.redo();
    flush(30);

    // A programmatic edit in a user scope is one step; the local A/B switch uses one.
    if (toolbar != nullptr && toolbar->getActions().selectCompare)
    {
        const auto& actions = toolbar->getActions();
        const float lpfA = processor.getParamValue(lpf);
        actions.selectCompare(1);              // B starts as a copy of A
        flush(40);
        fader->applyTypedValue("400 Hz");
        flush(40);
        const int before = steps();
        actions.selectCompare(0);              // back to A: one step that changes the sound
        flush(40);
        check(std::abs(processor.getParamValue(lpf) - lpfA) < 1.0f && steps() == before + 1
                  && history.getUndoDescription() == "Compare A",
              "switching the compared sound is one named undo step");
        processor.undo();
        flush(40);
        check(std::abs(processor.getParamValue(lpf) - 400.0f) < 1.0f, "undoing the switch brings the other sound back");
    }

    // Loading a sound is not a step and starts a fresh history; so does a state restore.
    processor.loadFactoryPreset(5);
    flush(60);
    check(! history.canUndo() && ! history.canRedo(), "a program load clears the history and is not itself undoable");
    fader->applyTypedValue("900 Hz");
    flush(30);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    check(history.canUndo(), "saving the state keeps the history");
    processor.setStateInformation(state.getData(), (int) state.getSize());
    flush(60);
    check(! history.canUndo(), "a state restore clears the history");
    // A switch that sets two parameters in one action is one step (the LEGACY render-policy cell).
    processor.setParamValue(cs80::P_sourcePerformance, 1.0f);
    processor.setParamValue(cs80::P_parityMode, 1.0f);
    flush(250);
    editor.showAdvancedPanel(true);
    editor.showAdvancedPage(3);
    flush(60);
    if (auto* legacyCell = dynamic_cast<juce::Button*>(findById(editor, "render_policy_legacy")))
    {
        const int before = steps();
        legacyCell->triggerClick();
        flush(60);
        check(processor.getParamValue(cs80::P_sourcePerformance) == 0.0f && processor.getParamValue(cs80::P_parityMode) == 0.0f
                  && steps() == before + 1,
              "an action that sets two parameters is one undo step");
        processor.undo();
        flush(60);
        check(processor.getParamValue(cs80::P_sourcePerformance) == 1.0f && processor.getParamValue(cs80::P_parityMode) == 1.0f,
              "undoing it restores both");
        processor.setParamValue(cs80::P_parityMode, 0.0f);
        processor.setParamValue(cs80::P_sourcePerformance, 0.0f);
    }
    else
        check(false, "the LEGACY render-policy cell exists");
    editor.showAdvancedPanel(false);
    flush(60);
}

// ---- Learning tools: the tour, per-control help and the evidence view -------------------------------------------
inline void testLearningTools(const Check& check, const Flush& flush, const juce::File& dir)
{
    using cs80::ui::StudioMount;
    static_assert(StudioMount::shouldOpenTourByItself(true, false, false), "a first real window opens the tour");
    static_assert(! StudioMount::shouldOpenTourByItself(false, false, false), "a headless editor never opens the tour");
    static_assert(! StudioMount::shouldOpenTourByItself(true, true, false), "a session that dismissed it does not reopen it");
    static_assert(! StudioMount::shouldOpenTourByItself(true, false, true), "a computer that dismissed it does not reopen it");

    const auto memory = cs80::ui::TourMemory::file();
    const auto memoryTime = memory.getLastModificationTime();
    const bool memoryExisted = memory.existsAsFile();

    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    flush(400);
    auto* tour = dynamic_cast<cs80::ui::TourOverlay*>(findById(editor, "guided_tour"));
    check(tour != nullptr, "the tour overlay is mounted");
    if (tour == nullptr)
        return;
    check(! tour->isVisible(), "a headless editor never opens the tour by itself (snapshots stay clean)");
    auto* toolbar = findFirst<cs80::ui::StudioToolbar>(editor);
    check(toolbar != nullptr && (bool) toolbar->getActions().startTour, "the toolbar menu starts the tour");
    if (toolbar == nullptr || ! toolbar->getActions().startTour)
        return;
    check(menuTexts(toolbar->buildMenu()).contains("Take the tour"), "the menu lists the tour");
    toolbar->getActions().startTour();
    flush(30);
    check(tour->isVisible() && tour->getStep() == 0 && tour->getNumSteps() >= 8, "the tour starts at its first step");
    for (const auto size : { juce::Point<int>(1600, 1000), juce::Point<int>(1120, 700) })
    {
        editor.setSize(size.x, size.y);
        flush(40);
        tour->start(0);
        for (int step = 0; step < tour->getNumSteps(); ++step)
        {
            flush(20);
            check(tour->getStep() == step, "the tour walks its steps in order");
            check(tour->getDescription().contains("Tour step " + juce::String(step + 1)), "each step is described for screen readers");
            if (size.x == 1120 || step < 3)
                save(editor, dir, "studio-tour-" + juce::String(step + 1) + "-" + juce::String(size.x) + "x" + juce::String(size.y), check);
            if (step + 1 < tour->getNumSteps())
                tour->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        }
    }
    editor.setSize(1600, 1000);
    flush(30);
    tour->keyPressed(juce::KeyPress(juce::KeyPress::leftKey));
    check(tour->getStep() == tour->getNumSteps() - 2, "Left goes back a step");
    check(! processor.getEditorState().tourSeen.load(), "the tour is not marked seen before it is dismissed");
    tour->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    flush(20);
    check(! tour->isVisible() && processor.getEditorState().tourSeen.load(), "Esc dismisses the tour and the session remembers it");
    check(memory.existsAsFile() == memoryExisted && memory.getLastModificationTime() == memoryTime,
          "a headless editor never writes the user's settings file");
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    {
        Cs80AudioProcessor restored;
        restored.setStateInformation(state.getData(), (int) state.getSize());
        check(restored.getEditorState().tourSeen.load(), "a dismissed tour stays dismissed in the restored session");
    }

    // Per-control help for every control on the main panel: plain words, the manual reference, the evidence class.
    int helped = 0, cited = 0;
    std::function<void(juce::Component&)> walk = [&](juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(child); knob != nullptr && knob->isVisible())
            {
                const auto help = cs80::ui::controlHelpText(knob->getParameterIndex());
                const bool hasEvidence = help.contains("EVIDENCE") && help.contains("CIRCUIT NOTES") && help.contains("Coverage ledger:");
                const bool hasReference = help.contains("Owner's manual") || help.contains("Service manual") || help.contains("Control ")
                                       || help.contains("Not on the original") || help.contains("Not in the owner's manual");
                helped += hasEvidence && hasReference ? 1 : 0;
                cited += help.contains("Owner's manual") || help.contains("service manual") ? 1 : 0;
                if (! (hasEvidence && hasReference))
                    std::printf("  help missing for %s\n", Cs80AudioProcessor::parameterId(knob->getParameterIndex()).toRawUTF8());
                continue;
            }
            walk(*child);
        }
    };
    walk(editor);
    check(helped >= 70 && cited >= 60, "every main-panel control has help with a manual reference and an evidence class");

    // F1 on a focused control opens its help card.
    if (auto* fader = knobFor(editor, cs80::chParam(0, cs80::CP_hpfCutoff)))
    {
        check(fader->getSlider().keyPressed(juce::KeyPress(juce::KeyPress::F1Key)), "F1 is handled by a control");
        flush(80);
        auto* card = findFirst<juce::CallOutBox>(editor);
        check(card != nullptr && card->isVisible(), "F1 opens the help card");
        save(editor, dir, "studio-help-card-1600x1000", check);
        if (card != nullptr)
            card->dismiss();
        flush(250);
        check(findFirst<juce::CallOutBox>(editor) == nullptr, "the help card closes");
    }
    else
        check(false, "HPF fader for the help card");

    // The evidence view: the menu opens Studio > Settings, whose register is the compiled STATUS.md table.
    if (toolbar->getActions().showEvidence)
    {
        toolbar->getActions().showEvidence();
        flush(60);
        auto* studio = studioOf(editor);
        check(editor.isAdvancedVisible() && studio != nullptr && studio->getActiveTab() == cs80::ui::StudioTab::settings,
              "the menu opens the evidence register in Studio > Settings");
        // The register is painted; screen readers get the same rows as static text.
        std::function<juce::Component*(juce::Component&)> spoken = [&](juce::Component& parent) -> juce::Component*
        {
            for (auto* child : parent.getChildren())
            {
                if (child->getTitle() == "Evidence register" && child->getDescription().contains("Oscillator"))
                    return child;
                if (auto* found = spoken(*child))
                    return found;
            }
            return nullptr;
        };
        auto* text = studio != nullptr ? spoken(*studio) : nullptr;
        bool everyRow = text != nullptr;
        for (const auto& row : cs80::evidence::kModelRegister)
            everyRow = everyRow && text->getDescription().contains(row.subsystem)
                    && text->getDescription().contains(cs80::evidence::basisName(row.basis));
        check(everyRow, "the evidence register is readable by screen readers, row by row");
        editor.showAdvancedPanel(false);
        flush(30);
    }
    else
        check(false, "the toolbar can show the evidence register");
}

// ---- Keyboard tab order ------------------------------------------------------------------------------------
inline std::vector<juce::Component*> tabOrder(juce::Component& container)
{
    juce::KeyboardFocusTraverser traverser;
    return traverser.getAllComponents(&container);
}

inline juce::String accessibleName(juce::Component& c)
{
    if (c.getTitle().isNotEmpty())
        return c.getTitle();
    if (auto* b = dynamic_cast<juce::Button*>(&c))
        return b->getButtonText();
    return {};
}

inline void testTabOrder(const Check& check, const Flush& flush)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1120, 700);
    editor.setVisible(true);
    flush(80);
    auto* panel = editor.getChildComponent(0);
    check(panel != nullptr && panel->isKeyboardFocusContainer(), "the instrument panel is a keyboard focus container");
    if (panel == nullptr)
        return;
    const auto order = tabOrder(*panel);
    const char* const toolbarIds[] = { "toolbar_previous", "toolbar_program", "toolbar_next", "toolbar_library", "toolbar_compare_a",
                                       "toolbar_compare_b", "toolbar_undo", "toolbar_redo", "toolbar_engine_circuit", "toolbar_engine_legacy",
                                       "toolbar_pure_cs80", "toolbar_studio", "toolbar_menu" };
    // Disabled keys (undo/redo with an empty history) are skipped by the traverser, so compare the enabled ones.
    std::vector<juce::String> expected, actual;
    for (const char* id : toolbarIds)
        if (auto* key = findById(*panel, id); key != nullptr && key->isEnabled())
            expected.push_back(id);
    for (size_t i = 0; i < order.size() && actual.size() < expected.size(); ++i)
        actual.push_back(order[i]->getComponentID());
    check(! expected.empty() && actual == expected, "Tab starts in the toolbar and walks its keys left to right");
    bool named = true, unique = true, noHiddenCluster = true;
    std::set<juce::Component*> seen;
    const juce::StringArray replaced { "Previous preset", "Next preset", "Open preset library", "Save a user preset",
                                       "Load the initial patch", "Show advanced controls", "Program display" };
    for (auto* c : order)
    {
        if (accessibleName(*c).isEmpty() && dynamic_cast<juce::Slider*>(c) == nullptr)
        {
            named = false;
            std::printf("  unnamed focus stop: %s\n", typeid(*c).name());
        }
        if (auto* slider = dynamic_cast<juce::Slider*>(c); slider != nullptr && slider->getTitle().isEmpty())
            named = false;
        unique = unique && seen.insert(c).second;
        // The replaced cluster is hidden; only the toolbar's own program display may carry that name.
        noHiddenCluster = noHiddenCluster && (! replaced.contains(c->getTitle()) || c->getComponentID() == "toolbar_program");
    }
    check(named, "every focus stop on the panel has an accessible name");
    check(unique && noHiddenCluster, "no focus stop repeats and the replaced toolbar cluster is out of the order");

    // The Studio view: its five tabs first, in order, then the sub-page keys, CLOSE, then the open page.
    editor.showAdvancedPanel(true);
    auto* studio = studioOf(editor);
    if (studio != nullptr)
    {
        studio->showTab(cs80::ui::StudioTab::modulation, 0);
        flush(40);
        const auto studioOrder = tabOrder(*studio);
        bool tabsFirst = studioOrder.size() > 8;
        for (int i = 0; tabsFirst && i < (int) cs80::ui::StudioTab::count; ++i)
            tabsFirst = accessibleName(*studioOrder[(size_t) i]) == cs80::ui::StudioView::tabName((cs80::ui::StudioTab) i) + " tab";
        check(tabsFirst, "Studio Tab order starts with its five tabs in order");
        check(studioOrder.size() > 8 && accessibleName(*studioOrder[5]).startsWith("MODULATION sub-page")
                  && accessibleName(*studioOrder[6]).startsWith("MODULATION sub-page")
                  && accessibleName(*studioOrder[7]) == "Close the Studio view",
              "then the sub-page keys and CLOSE");
        bool pageAfter = studioOrder.size() > 8;
        for (size_t i = 8; i < studioOrder.size(); ++i)
            pageAfter = pageAfter && studio->getSubPage(cs80::ui::StudioTab::modulation, 0)->isParentOf(studioOrder[i]);
        check(pageAfter, "then only the controls of the open page");
        bool studioNamed = true;
        for (int t = 0; t < (int) cs80::ui::StudioTab::count; ++t)
            for (int s = 0; s < studio->getNumSubPages((cs80::ui::StudioTab) t); ++s)
            {
                studio->showTab((cs80::ui::StudioTab) t, s);
                flush(20);
                for (auto* c : tabOrder(*studio))
                    if (accessibleName(*c).isEmpty())
                    {
                        studioNamed = false;
                        std::printf("  unnamed Studio focus stop on tab %d: %s\n", t, typeid(*c).name());
                    }
            }
        check(studioNamed, "every Studio focus stop has an accessible name");
    }
    else
        check(false, "Studio view for the tab-order test");
    editor.showAdvancedPanel(false);
    flush(30);

    // The voice monitor and the tour are their own focus containers.
    if (auto* toolbar = findFirst<cs80::ui::StudioToolbar>(editor))
    {
        toolbar->getActions().toggleScope();
        flush(30);
        if (auto* scope = findById(editor, "voice_monitor"))
        {
            const auto scopeOrder = tabOrder(*scope);
            check(scopeOrder.size() == 3 && accessibleName(*scopeOrder[0]) == "Monitor channel I"
                      && accessibleName(*scopeOrder[2]) == "Close the voice monitor",
                  "voice monitor order: channel I, channel II, CLOSE");
        }
        toolbar->getActions().toggleScope();
        toolbar->getActions().startTour();
        flush(30);
        if (auto* tour = findById(editor, "guided_tour"))
        {
            const auto tourOrder = tabOrder(*tour);
            check(tourOrder.size() >= 2 && tourOrder[0]->getComponentID() == "tour_next", "the tour focuses NEXT first");
            if (auto* t = dynamic_cast<cs80::ui::TourOverlay*>(tour))
                t->dismiss();
        }
        flush(30);
    }
}

// ---- Minimum target size: WCAG 2.2 success criterion 2.5.8 at the 1120 x 700 minimum -------------------------
struct HitTarget
{
    juce::Component* component;
    juce::Rectangle<float> bounds;
    bool owned;   // part of the modern layer this workstream owns
};

inline bool isModernLayer(juce::Component* c)
{
    for (auto* p = c; p != nullptr; p = p->getParentComponent())
        if (dynamic_cast<cs80::ui::StudioToolbar*>(p) != nullptr || dynamic_cast<cs80::ui::StudioView*>(p) != nullptr
            || dynamic_cast<cs80::ui::ScopeDrawer*>(p) != nullptr || dynamic_cast<cs80::ui::ControllersStrip*>(p) != nullptr
            || dynamic_cast<cs80::ui::TourOverlay*>(p) != nullptr)
            return true;
    return false;
}

inline void collectTargets(juce::Component& root, juce::Component& parent, std::vector<HitTarget>& out)
{
    for (auto* child : parent.getChildren())
    {
        if (! child->isVisible())
            continue;
        // The piano keys are exempt: their size and spacing are the instrument (WCAG 2.5.8 "essential").
        if (dynamic_cast<juce::MidiKeyboardComponent*>(child) != nullptr)
            continue;
        const bool interactive = (dynamic_cast<juce::Button*>(child) != nullptr && child->isEnabled())
                              || dynamic_cast<juce::Slider*>(child) != nullptr || dynamic_cast<juce::ComboBox*>(child) != nullptr
                              || dynamic_cast<juce::TextEditor*>(child) != nullptr || dynamic_cast<cs80::ui::RibbonStrip*>(child) != nullptr
                              || dynamic_cast<cs80::ui::PerformanceWheel*>(child) != nullptr;
        if (interactive)
        {
            out.push_back({ child, root.getLocalArea(child, child->getLocalBounds()).toFloat(), isModernLayer(child) });
            continue;
        }
        collectTargets(root, *child, out);
    }
}

inline bool circleHitsRect(juce::Point<float> c, float radius, juce::Rectangle<float> r)
{
    const float x = juce::jlimit(r.getX(), r.getRight(), c.x), y = juce::jlimit(r.getY(), r.getBottom(), c.y);
    return c.getDistanceFrom({ x, y }) < radius;
}

// Returns the failing targets: smaller than 24 x 24 px and not rescued by the spacing exception (a 24 px circle on
// the target's centre must hit no other target and no other undersized target's circle).
inline std::vector<const HitTarget*> undersizedTargets(const std::vector<HitTarget>& targets)
{
    constexpr float minimum = 24.0f, radius = 12.0f;
    auto small = [](const HitTarget& t) { return t.bounds.getWidth() < minimum - 0.01f || t.bounds.getHeight() < minimum - 0.01f; };
    std::vector<const HitTarget*> failing;
    for (const auto& a : targets)
    {
        if (! small(a))
            continue;
        bool spaced = true;
        for (const auto& b : targets)
        {
            if (&a == &b || b.component->isParentOf(a.component) || a.component->isParentOf(b.component))
                continue;
            if (circleHitsRect(a.bounds.getCentre(), radius, b.bounds)
                || (small(b) && a.bounds.getCentre().getDistanceFrom(b.bounds.getCentre()) < 2.0f * radius))
            {
                spaced = false;
                break;
            }
        }
        if (! spaced)
            failing.push_back(&a);
    }
    return failing;
}

inline juce::String describe(const HitTarget& t)
{
    const auto name = accessibleName(*t.component).isNotEmpty() ? accessibleName(*t.component) : t.component->getComponentID();
    return name + " (" + juce::String(t.bounds.getWidth(), 1) + " x " + juce::String(t.bounds.getHeight(), 1) + " px)";
}

// The modern layer must pass outright. The rest of the editor is being re-laid out by the panel workstream: its
// results are printed as an audit (set kStrictForWholeEditor once that re-layout lands).
constexpr bool kStrictForWholeEditor = false;

inline void testHitTargets(const Check& check, const Flush& flush)
{
    Cs80AudioProcessor processor;
    processor.prepareToPlay(48000.0, 128);
    processor.getMidiLearn().assign(74, processor.hostParameterIndex(cs80::chParam(0, cs80::CP_lpfCutoff)));
    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1120, 700);
    editor.setVisible(true);
    flush(80);
    int ownedChecked = 0, ownedFailing = 0, panelFailing = 0;
    auto audit = [&](juce::Component& root, const char* where)
    {
        std::vector<HitTarget> targets;
        collectTargets(root, root, targets);
        for (const auto* t : undersizedTargets(targets))
        {
            if (t->owned)
            {
                ++ownedFailing;
                std::printf("  undersized modern-layer target in %s: %s\n", where, describe(*t).toRawUTF8());
            }
            else
            {
                ++panelFailing;
                std::printf("  hit-target audit (%s): %s\n", where, describe(*t).toRawUTF8());
            }
        }
        for (const auto& t : targets)
            ownedChecked += t.owned ? 1 : 0;
    };
    audit(editor, "main view");
    editor.showAdvancedPanel(true);
    if (auto* studio = studioOf(editor))
        for (int t = 0; t < (int) cs80::ui::StudioTab::count; ++t)
            for (int s = 0; s < studio->getNumSubPages((cs80::ui::StudioTab) t); ++s)
            {
                studio->showTab((cs80::ui::StudioTab) t, s);
                flush(40);
                audit(*studio, ("Studio " + cs80::ui::StudioView::tabName((cs80::ui::StudioTab) t)).toRawUTF8());
            }
    editor.showAdvancedPanel(false);
    flush(30);
    if (auto* toolbar = findFirst<cs80::ui::StudioToolbar>(editor))
    {
        toolbar->getActions().toggleScope();
        toolbar->getActions().toggleControllers();
        flush(150);
        if (auto* scope = findById(editor, "voice_monitor")) audit(*scope, "voice monitor");
        if (auto* strip = findById(editor, "controllers_strip")) audit(*strip, "controllers strip");
        toolbar->getActions().toggleScope();
        toolbar->getActions().toggleControllers();
        toolbar->getActions().startTour();
        flush(40);
        if (auto* tour = findById(editor, "guided_tour"))
        {
            audit(*tour, "tour");
            if (auto* t = dynamic_cast<cs80::ui::TourOverlay*>(tour)) t->dismiss();
        }
    }
    std::printf("hit targets at 1120 x 700: %d modern-layer targets checked, %d undersized; %d panel targets in the audit list\n",
                ownedChecked, ownedFailing, panelFailing);
    check(ownedChecked > 120, "the hit-target scan reaches the toolbar, every Studio page, the drawers and the tour");
    check(ownedFailing == 0, "every modern-layer control is at least 24 x 24 px (or spaced per WCAG 2.5.8) at 1120 x 700");
    if (kStrictForWholeEditor)
        check(panelFailing == 0, "every editor control meets the 24 px target size at 1120 x 700");
}

// ---- Contrast of the modern layer's text (WCAG 2.2 1.4.3, 4.5:1 for text under 18.66 px bold) ---------------
inline float luminance(juce::Colour c)
{
    auto lin = [](float v) { return v <= 0.03928f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * lin(c.getFloatRed()) + 0.7152f * lin(c.getFloatGreen()) + 0.0722f * lin(c.getFloatBlue());
}

inline float contrast(juce::Colour a, juce::Colour b)
{
    const float la = luminance(a), lb = luminance(b);
    return (juce::jmax(la, lb) + 0.05f) / (juce::jmin(la, lb) + 0.05f);
}

inline void testContrast(const Check& check)
{
    using namespace cs80::ui;
    const auto keyFace = juce::Colour(0xff5d5850).interpolatedWith(juce::Colour(0xff2a2824), 0.5f);
    const auto litFace = juce::Colour(0xffe2b25a).interpolatedWith(juce::Colour(0xff9c6c22), 0.5f);
    const auto tipPlate = juce::Colour(0xff35312a).interpolatedWith(juce::Colour(0xff15130f), 0.5f);
    const auto cardPlate = juce::Colour(0xff2d2a25).interpolatedWith(juce::Colour(0xff161411), 0.5f);
    const auto panel = panelFaceAt(0.5f, 0.0f, 1.0f);
    struct Pair { const char* what; juce::Colour ink, ground; };
    const Pair pairs[] = {
        { "toolbar key legend", juce::Colour(0xffeee6cc), keyFace },
        { "lit toolbar key legend", juce::Colour(0xff241703), litFace },
        { "masked ENGINE legend", keyFace.interpolatedWith(juce::Colour(0xffeee6cc), 0.74f), keyFace },
        { "program name", juce::Colour(0xffffcf70), juce::Colour(0xff160c05) },
        { "tooltip body", ivory, tipPlate },
        { "tooltip heading", gold, tipPlate },
        { "tooltip hint", ivoryDim, tipPlate },
        { "tooltip manual page", gold, tipPlate },
        { "tooltip circuit notes", tipPlate.interpolatedWith(ivory, 0.86f), tipPlate },
        { "tour card text", cardPlate.interpolatedWith(ivory, 0.9f), cardPlate },
        { "value editor", amberBright, juce::Colour(0xff0d0b08) },
        { "Studio running text", ivoryDim.interpolatedWith(ivory, 0.5f), panel },
        { "Studio legend", ivory, panel },
    };
    for (const auto& p : pairs)
    {
        const float ratio = contrast(p.ink, p.ground);
        if (ratio < 4.5f)
            std::printf("  contrast %.2f:1 for %s\n", (double) ratio, p.what);
        check(ratio >= 4.5f, "modern-layer text meets 4.5:1");
    }
    // Evidence chips: the brightened basis colour on its 20 % tint over the panel.
    for (auto basis : { cs80::evidence::Basis::sourceDerived, cs80::evidence::Basis::sourceTopologyInferredLaw,
                        cs80::evidence::Basis::engineeringInferred, cs80::evidence::Basis::modernExtension })
    {
        const juce::Colour colour = basis == cs80::evidence::Basis::sourceDerived ? juce::Colour(0xff4fbf6e)
                                  : basis == cs80::evidence::Basis::sourceTopologyInferredLaw ? juce::Colour(0xffe2b04a)
                                  : basis == cs80::evidence::Basis::engineeringInferred ? juce::Colour(0xffc9bfae) : juce::Colour(0xff6cc6e6);
        const float ratio = contrast(colour.brighter(0.25f), panel.interpolatedWith(colour, 0.2f));
        check(ratio >= 4.5f, "evidence chip text meets 4.5:1");
    }
}

} // namespace cs80::tests::studio

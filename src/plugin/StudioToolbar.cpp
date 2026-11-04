#include "StudioToolbar.h"

#include "ArtCommon.h"
#include "PluginProcessor.h"

#include <cmath>

namespace cs80::ui
{

namespace
{
using Rf = juce::Rectangle<float>;

juce::Font toolFont(float height)
{
    return art::displayFont(juce::jmax(8.0f, height), juce::Font::bold).withExtraKerningFactor(0.08f);
}

juce::String shortcut()
{
#if JUCE_MAC
    return "Cmd";
#else
    return "Ctrl";
#endif
}

juce::ColourGradient vgrad(juce::Colour top, juce::Colour bottom, Rf r)
{
    return juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
}
} // namespace

// ---------------------------------------------------------------------------
// One key of the toolbar: the toolbar keycap (warm grey plastic, lit amber when on), as a plain key,
// one segment of a segmented switch, or an arrow / undo / redo / menu glyph.
// ---------------------------------------------------------------------------
class StudioToolbar::Key final : public ContextButton
{
public:
    enum class Glyph { none, previous, next, undo, redo, menu };
    enum class Segment { whole, left, right };

    Key(const juce::String& text, const juce::String& title, Glyph g = Glyph::none, Segment s = Segment::whole)
        : ContextButton(text), glyph(g), segment(s)
    {
        setTitle(title);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }

    void setArmed(bool value) { if (value != armed) { armed = value; repaint(); } }
    // A masked key (ENGINE while PURE CS-80 is on) keeps working but reads as overridden.
    void setMasked(bool value) { if (value != masked) { masked = value; repaint(); } }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        const float rad = juce::jmin(4.0f, r.getHeight() * 0.2f);
        const bool on = getToggleState();
        const bool enabled = isEnabled();
        juce::Colour top(0xff5d5850), bottom(0xff2a2824);
        if (on) { top = juce::Colour(0xffe2b25a); bottom = juce::Colour(0xff9c6c22); }
        if (armed) { top = juce::Colour(0xffb0452f); bottom = juce::Colour(0xff5e1e12); }
        if (hovered && enabled) { top = top.brighter(0.14f); bottom = bottom.brighter(0.10f); }
        if (down) { std::swap(top, bottom); r = r.translated(0.0f, 1.0f); }
        else softShadow(g, r, rad, 0.0f, 1.6f, 2.2f, 0.65f);

        juce::Path body;
        const bool leftRound = segment != Segment::right, rightRound = segment != Segment::left;
        body.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), rad, rad, leftRound, rightRound, leftRound, rightRound);
        g.setGradientFill(vgrad(top, bottom, r));
        g.fillPath(body);
        g.setColour(juce::Colours::white.withAlpha(down ? 0.05f : 0.20f));
        g.drawLine(r.getX() + (leftRound ? rad : 0.0f), r.getY() + 1.0f, r.getRight() - (rightRound ? rad : 0.0f), r.getY() + 1.0f, 1.0f);
        g.setColour(on ? juce::Colour(0xffe6b04a).withAlpha(0.9f) : juce::Colours::black.withAlpha(0.85f));
        g.strokePath(body, juce::PathStrokeType(on ? 1.2f : 1.0f));

        const auto ink = ! enabled ? juce::Colour(0xff8a8476)
                       : armed ? juce::Colour(0xffffe6d6)
                       : on ? juce::Colour(0xff241703)
                       : juce::Colour(0xffeee6cc);
        const float alpha = masked ? 0.74f : 1.0f;
        auto area = r.reduced(4.0f, 1.0f);
        if (glyph == Glyph::none || armed)
        {
            const auto shown = armed ? juce::String("?") : getButtonText();
            auto font = fitFontToWidth(toolFont(juce::jlimit(8.0f, 14.0f, r.getHeight() * 0.38f)), shown, area.getWidth(), 0.85f, 8.0f);
            g.setFont(font);
            g.setColour((on ? juce::Colours::white.withAlpha(0.35f) : juce::Colours::black.withAlpha(0.6f)).withMultipliedAlpha(alpha));
            g.drawFittedText(shown, area.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centred, 1, 0.85f);
            g.setColour(ink.withMultipliedAlpha(alpha));
            g.drawFittedText(shown, area.toNearestInt(), juce::Justification::centred, 1, 0.85f);
        }
        else
            paintGlyph(g, area, ink.withMultipliedAlpha(alpha));
        if (masked)
        {
            g.setColour(juce::Colour(0xffcdb878).withAlpha(0.55f));
            g.drawLine(r.getX() + 4.0f, r.getBottom() - 4.0f, r.getRight() - 4.0f, r.getBottom() - 4.0f, 1.0f);
        }
        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat());
    }

private:
    void paintGlyph(juce::Graphics& g, Rf area, juce::Colour ink)
    {
        const auto c = area.getCentre();
        const float s = juce::jmin(area.getWidth(), area.getHeight()) * 0.30f;
        juce::Path p;
        switch (glyph)
        {
            case Glyph::previous:
            case Glyph::next:
            {
                const float dir = glyph == Glyph::previous ? -1.0f : 1.0f;
                p.addTriangle(c.x + dir * s, c.y, c.x - dir * s * 0.8f, c.y - s * 1.05f, c.x - dir * s * 0.8f, c.y + s * 1.05f);
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.fillPath(p, juce::AffineTransform::translation(0.0f, 1.0f));
                g.setColour(ink);
                g.fillPath(p);
                return;
            }
            case Glyph::undo:
            case Glyph::redo:
            {
                // A hooked arrow: an arc over the top, the head pointing back (undo) or forward (redo).
                const float dir = glyph == Glyph::undo ? -1.0f : 1.0f;
                p.addCentredArc(c.x, c.y + s * 0.35f, s * 1.05f, s * 0.95f, 0.0f,
                                dir < 0.0f ? -1.9f : 1.9f, dir < 0.0f ? 0.9f : -0.9f, true);
                const auto tip = juce::Point<float>(c.x + dir * s * 1.0f, c.y - s * 0.25f);
                juce::Path head;
                head.addTriangle(tip.x + dir * s * 0.62f, tip.y + s * 0.05f, tip.x - dir * s * 0.12f, tip.y - s * 0.55f,
                                 tip.x - dir * s * 0.12f, tip.y + s * 0.62f);
                g.setColour(ink);
                g.strokePath(p, juce::PathStrokeType(juce::jmax(1.4f, s * 0.24f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.fillPath(head);
                return;
            }
            case Glyph::menu:
            {
                g.setColour(ink);
                for (int i = -1; i <= 1; ++i)
                    g.fillRoundedRectangle(c.x - s, c.y + (float) i * s * 0.62f - s * 0.12f, s * 2.0f, s * 0.24f, s * 0.1f);
                return;
            }
            case Glyph::none: break;
        }
    }

    Glyph glyph;
    Segment segment;
    bool armed = false, masked = false;
};

// ---------------------------------------------------------------------------
// The program display: a backlit glass window with the program number, its category and the name. It is
// a button: clicking it opens the library.
// ---------------------------------------------------------------------------
class StudioToolbar::Display final : public juce::Button
{
public:
    Display() : juce::Button("PROGRAM")
    {
        setTitle("Program display");
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setContent(const PresetDisplay& d)
    {
        if (d == content)
            return;
        content = d;
        setDescription("Program " + d.number + ": " + d.name + (d.edited ? ", edited" : "") + ". Press to open the library.");
        setTooltip(d.name + (d.edited ? "  (unsaved edits)" : "") + "\nClick to open the library.");
        repaint();
    }
    const PresetDisplay& getContent() const noexcept { return content; }

    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto w = getLocalBounds().toFloat().reduced(1.0f);
        drawGlassPanel(g, w, 4.0f, juce::Colour(0xff160c05), 0.7f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe39a38).withAlpha(hovered || down ? 0.32f : 0.22f),
                                               w.getX() + w.getWidth() * 0.18f, w.getCentreY() + 3.0f,
                                               juce::Colours::transparentBlack, w.getX(), w.getCentreY(), true));
        g.fillRoundedRectangle(w.reduced(1.5f), 3.0f);
        const float h = w.getHeight();
        const float numberW = h * 1.45f;
        auto numberArea = w.withWidth(numberW).reduced(2.0f, 0.0f);
        auto rest = w.withTrimmedLeft(numberW + 6.0f).reduced(4.0f, 2.0f);
        g.setColour(juce::Colour(0xffd09a3c).withAlpha(0.35f));
        g.fillRect(w.getX() + numberW + 2.0f, w.getY() + h * 0.18f, 1.0f, h * 0.64f);

        g.setFont(toolFont(h * 0.52f));
        g.setColour(content.number == "USR" ? juce::Colour(0xff9a6a2c) : juce::Colour(0xffffb347));
        g.drawFittedText(content.number, numberArea.toNearestInt(), juce::Justification::centred, 1, 0.8f);

        auto tagArea = rest.removeFromTop(rest.getHeight() * 0.36f);
        g.setFont(toolFont(juce::jmax(8.0f, h * 0.24f)));
        g.setColour(juce::Colour(0xffd09a3c));
        auto tagText = content.tag;
        if (content.edited)
        {
            const auto font = toolFont(juce::jmax(8.0f, h * 0.24f));
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt(font, "EDITED") + 8.0f;
            const Rf tag(tagArea.getRight() - tw, tagArea.getY() + 0.5f, tw, tagArea.getHeight());
            g.setColour(juce::Colour(0xffffa530).withAlpha(0.16f));
            g.fillRoundedRectangle(tag, 2.0f);
            g.setColour(juce::Colour(0xffffb347).withAlpha(0.85f));
            g.drawRoundedRectangle(tag.reduced(0.5f), 2.0f, 0.9f);
            g.setColour(juce::Colour(0xffffcf70));
            g.drawFittedText("EDITED", tag.toNearestInt(), juce::Justification::centred, 1, 0.8f);
            tagArea.removeFromRight(tw + 6.0f);
            g.setColour(juce::Colour(0xffd09a3c));
        }
        g.drawFittedText(tagText, tagArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
        g.setFont(toolFont(juce::jmax(9.0f, h * 0.42f)).withExtraKerningFactor(0.03f));
        g.setColour(juce::Colour(0xffffcf70));
        g.drawFittedText(content.name, rest.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, getLocalBounds().toFloat());
    }

private:
    PresetDisplay content;
};

// ---------------------------------------------------------------------------
// The toolbar
// ---------------------------------------------------------------------------
StudioToolbar::StudioToolbar(Cs80AudioProcessor& p) : processor(p)
{
    setTitle("Studio toolbar");
    setOpaque(false);
    setFocusContainerType(juce::Component::FocusContainerType::none);

    prev = std::make_unique<Key>("<", "Previous program", Key::Glyph::previous);
    next = std::make_unique<Key>(">", "Next program", Key::Glyph::next);
    library = std::make_unique<Key>("LIBRARY", "Open the preset library");
    compareA = std::make_unique<Key>("A", "Compare: sound A", Key::Glyph::none, Key::Segment::left);
    compareB = std::make_unique<Key>("B", "Compare: sound B", Key::Glyph::none, Key::Segment::right);
    undoKey = std::make_unique<Key>("UNDO", "Undo", Key::Glyph::undo);
    redoKey = std::make_unique<Key>("REDO", "Redo", Key::Glyph::redo);
    circuitKey = std::make_unique<Key>("CIRCUIT", "Engine: circuit", Key::Glyph::none, Key::Segment::left);
    legacyKey = std::make_unique<Key>("LEGACY", "Engine: legacy compatibility", Key::Glyph::none, Key::Segment::right);
    pureKey = std::make_unique<Key>("PURE CS-80", "PURE CS-80");
    studioKey = std::make_unique<Key>("STUDIO", "Show the Studio view");
    menuKey = std::make_unique<Key>("MENU", "Toolbar menu", Key::Glyph::menu);
    display = std::make_unique<Display>();

    prev->setComponentID("toolbar_previous");
    next->setComponentID("toolbar_next");
    library->setComponentID("toolbar_library");
    compareA->setComponentID("toolbar_compare_a");
    compareB->setComponentID("toolbar_compare_b");
    undoKey->setComponentID("toolbar_undo");
    redoKey->setComponentID("toolbar_redo");
    circuitKey->setComponentID("toolbar_engine_circuit");
    legacyKey->setComponentID("toolbar_engine_legacy");
    pureKey->setComponentID("toolbar_pure_cs80");
    studioKey->setComponentID("toolbar_studio");
    menuKey->setComponentID("toolbar_menu");
    display->setComponentID("toolbar_program");

    prev->setTooltip("Previous program. With unsaved edits, press twice to discard them.");
    next->setTooltip("Next program. With unsaved edits, press twice to discard them.");
    library->setTooltip("Open the preset library: browse, search and load programs.");
    compareA->setTooltip("Compare A: switch to sound A. The first switch to B copies A, so you can edit B and compare.");
    compareB->setTooltip("Compare B: switch to sound B. The first switch to B copies A, so you can edit B and compare.");
    undoKey->setTooltip("Undo the last edit (" + shortcut() + "+Z). Loading a program clears the history; host automation is never recorded.");
    redoKey->setTooltip("Redo (" + shortcut() + "+Shift+Z).");
    circuitKey->setTooltip("ENGINE: CIRCUIT runs the circuit-informed models (the source_performance parameter). Not calibrated: no hardware "
                           "match established. An engine change stops sounding notes.");
    legacyKey->setTooltip("ENGINE: LEGACY keeps the older voicing so old sessions sound unchanged. An engine change stops sounding notes.");
    pureKey->setTooltip("PURE CS-80 strips every software addition (LFOs, effects, macros, arpeggiator, MPE, MIDI CCs, MIDI learn) and runs the "
                        "circuit models only (the parity_mode parameter). Not calibrated. Switching stops sounding notes.");
    studioKey->setTooltip("Studio view: modulation, keyboard and tuning, effects, arpeggiator and settings.");
    menuKey->setTooltip("Menu: save, init, voice monitor, controllers strip, computer keyboard, help and tour.");

    int order = 0;
    for (juce::Component* c : { (juce::Component*) prev.get(), (juce::Component*) display.get(), (juce::Component*) next.get(),
                                (juce::Component*) library.get(), (juce::Component*) compareA.get(), (juce::Component*) compareB.get(),
                                (juce::Component*) undoKey.get(), (juce::Component*) redoKey.get(), (juce::Component*) circuitKey.get(),
                                (juce::Component*) legacyKey.get(), (juce::Component*) pureKey.get(), (juce::Component*) studioKey.get(),
                                (juce::Component*) menuKey.get() })
    {
        c->setExplicitFocusOrder(++order);
        addAndMakeVisible(*c);
    }

    prev->onClick = [this] { if (actions.stepPreset) actions.stepPreset(-1); refresh(); };
    next->onClick = [this] { if (actions.stepPreset) actions.stepPreset(1); refresh(); };
    display->onClick = [this] { if (actions.openLibrary) actions.openLibrary(); refresh(); };
    library->onClick = [this] { if (actions.openLibrary) actions.openLibrary(); refresh(); };
    compareA->onClick = [this] { if (actions.selectCompare) actions.selectCompare(0); refresh(); };
    compareB->onClick = [this] { if (actions.selectCompare) actions.selectCompare(1); refresh(); };
    undoKey->onClick = [this] { if (actions.undo) actions.undo(); refresh(); };
    redoKey->onClick = [this] { if (actions.redo) actions.redo(); refresh(); };
    studioKey->onClick = [this] { if (actions.toggleStudio) actions.toggleStudio(); refresh(); };
    menuKey->onClick = [this] { showMenu(); };
    circuitKey->onClick = [this] { setEngine(true); };
    legacyKey->onClick = [this] { setEngine(false); };
    pureKey->onClick = [this] { setPure(pureValue < 0.5f); };
    circuitKey->bindParameterContext(processor, Cs80AudioProcessor::parameterId(cs80::P_sourcePerformance), cs80::P_sourcePerformance);
    legacyKey->bindParameterContext(processor, Cs80AudioProcessor::parameterId(cs80::P_sourcePerformance), cs80::P_sourcePerformance);
    pureKey->bindParameterContext(processor, Cs80AudioProcessor::parameterId(cs80::P_parityMode), cs80::P_parityMode);

    auto& apvts = processor.getAPVTS();
    if (auto* engine = apvts.getParameter(Cs80AudioProcessor::parameterId(cs80::P_sourcePerformance)))
    {
        engineAttachment = std::make_unique<juce::ParameterAttachment>(*engine, [this](float v) { engineValue = v; refresh(); });
        engineAttachment->sendInitialUpdate();
    }
    if (auto* pure = apvts.getParameter(Cs80AudioProcessor::parameterId(cs80::P_parityMode)))
    {
        pureAttachment = std::make_unique<juce::ParameterAttachment>(*pure, [this](float v) { pureValue = v; refresh(); });
        pureAttachment->sendInitialUpdate();
    }
    refresh();
    startTimerHz(8);
}

StudioToolbar::~StudioToolbar()
{
    stopTimer();
}

void StudioToolbar::setActions(Actions newActions)
{
    actions = std::move(newActions);
    refresh();
}

// [integration] ws2's processor API over the unchanged host IDs (one host gesture each).
void StudioToolbar::setEngine(bool circuit)
{
    processor.setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit : Cs80AudioProcessor::EngineMode::legacy);
    refresh();
}

void StudioToolbar::setPure(bool pure)
{
    processor.setPure(pure);
    refresh();
}

void StudioToolbar::refresh()
{
    if (actions.presetDisplay)
        display->setContent(actions.presetDisplay());
    else
    {
        const auto metadata = processor.getPresetMetadata();
        display->setContent({ juce::String(metadata.factoryIndex + 1).paddedLeft('0', 3), metadata.name, {}, false });
    }
    prev->setEnabled((bool) actions.stepPreset);
    next->setEnabled((bool) actions.stepPreset);
    prev->setArmed(actions.stepArmed && actions.stepArmed(-1));
    next->setArmed(actions.stepArmed && actions.stepArmed(1));
    library->setEnabled((bool) actions.openLibrary);
    library->setToggleState(actions.libraryOpen && actions.libraryOpen(), juce::dontSendNotification);
    const bool compare = (bool) actions.selectCompare;
    const int slot = actions.activeCompare ? actions.activeCompare() : 0;
    compareA->setEnabled(compare);
    compareB->setEnabled(compare);
    compareA->setToggleState(compare && slot == 0, juce::dontSendNotification);
    compareB->setToggleState(compare && slot == 1, juce::dontSendNotification);
    const bool canUndo = actions.undo && actions.canUndo && actions.canUndo();
    const bool canRedo = actions.redo && actions.canRedo && actions.canRedo();
    undoKey->setEnabled(canUndo);
    redoKey->setEnabled(canRedo);
    const auto undoName = canUndo && actions.undoName ? actions.undoName() : juce::String();
    const auto redoName = canRedo && actions.redoName ? actions.redoName() : juce::String();
    undoKey->setTitle(undoName.isNotEmpty() ? "Undo " + undoName : juce::String("Undo"));
    redoKey->setTitle(redoName.isNotEmpty() ? "Redo " + redoName : juce::String("Redo"));

    const bool circuit = engineValue >= 0.5f, pure = pureValue >= 0.5f;
    circuitKey->setToggleState(circuit, juce::dontSendNotification);
    legacyKey->setToggleState(! circuit, juce::dontSendNotification);
    circuitKey->setMasked(pure);
    legacyKey->setMasked(pure);
    pureKey->setToggleState(pure, juce::dontSendNotification);
    const auto masked = pure ? juce::String(" Masked while PURE CS-80 is on: it always runs the circuit models; the stored choice applies again "
                                            "when PURE CS-80 is switched off.") : juce::String();
    circuitKey->setDescription(circuitKey->getTooltip() + masked);
    legacyKey->setDescription(legacyKey->getTooltip() + masked);
    pureKey->setDescription(pureKey->getTooltip());

    studioKey->setEnabled((bool) actions.toggleStudio);
    studioKey->setToggleState(actions.studioOpen && actions.studioOpen(), juce::dontSendNotification);
}

void StudioToolbar::timerCallback()
{
    if (isShowing())
        refresh();
}

juce::PopupMenu StudioToolbar::buildMenu()
{
    juce::PopupMenu menu;
    auto item = [&menu](int id, const juce::String& text, bool enabled, bool ticked, const std::function<void()>& action)
    {
        if (! action)
            return;
        juce::PopupMenu::Item entry(text);
        entry.itemID = id;
        entry.isEnabled = enabled;
        entry.isTicked = ticked;
        entry.action = action;
        menu.addItem(entry);
    };
    const bool pure = pureValue >= 0.5f;
    menu.addSectionHeader("PROGRAM");
    item(1, "Save preset...  (" + shortcut() + "+S)", true, false, actions.savePreset);
    item(2, "Init patch", true, false, actions.initPatch);
    item(14, "Randomise within original ranges", true, false, actions.randomise);
    if (actions.copyCompare && actions.selectCompare)
    {
        const int slot = actions.activeCompare ? actions.activeCompare() : 0;
        item(3, juce::String("Copy ") + (slot == 0 ? "A to B" : "B to A"), true, false, actions.copyCompare);
    }
    if (actions.undo || actions.redo)
    {
        menu.addSeparator();
        const bool canUndo = actions.canUndo && actions.canUndo();
        const bool canRedo = actions.canRedo && actions.canRedo();
        const auto undoName = canUndo && actions.undoName ? actions.undoName() : juce::String();
        const auto redoName = canRedo && actions.redoName ? actions.redoName() : juce::String();
        item(4, "Undo" + (undoName.isNotEmpty() ? " " + undoName : juce::String()), canUndo, false, actions.undo);
        item(5, "Redo" + (redoName.isNotEmpty() ? " " + redoName : juce::String()), canRedo, false, actions.redo);
    }
    menu.addSeparator();
    menu.addSectionHeader("VIEW");
    if (actions.setZoom && ! actions.zoomPresets.empty())
    {
        juce::PopupMenu sizes;
        const int current = actions.zoom ? actions.zoom() : 0;
        for (int percent : actions.zoomPresets)
        {
            juce::PopupMenu::Item size(juce::String(percent) + " %   (" + juce::String(16 * percent) + " x " + juce::String(10 * percent) + ")");
            size.itemID = 100 + percent;
            size.isTicked = percent == current;
            size.action = [setZoom = actions.setZoom, percent] { setZoom(percent); };
            sizes.addItem(size);
        }
        menu.addSubMenu("Display size", sizes);
    }
    item(10, "Voice monitor (scope drawer)", true, actions.scopeOpen && actions.scopeOpen(), actions.toggleScope);
    item(11, pure ? "Controllers strip (hidden in PURE CS-80)" : "Controllers strip: pitch and mod wheels", ! pure,
         actions.controllersShown && actions.controllersShown(), actions.toggleControllers);
    item(12, "Computer keyboard plays notes", true, actions.qwertyEnabled && actions.qwertyEnabled(), actions.toggleQwerty);
    item(13, "Circuit notes in tooltips", true, actions.circuitNotesShown && actions.circuitNotesShown(), actions.toggleCircuitNotes);
    if (actions.clearMidiLearn)
    {
        const int count = actions.midiLearnCount ? actions.midiLearnCount() : 0;
        menu.addSeparator();
        item(20, "Clear MIDI learn (" + juce::String(count) + (count == 1 ? " mapping)" : " mappings)"), count > 0, false, actions.clearMidiLearn);
    }
    menu.addSeparator();
    menu.addSectionHeader("HELP");
    item(30, "Take the tour", true, false, actions.startTour);
    item(31, "Evidence register (Studio > Settings)", true, false, actions.showEvidence);
    return menu;
}

void StudioToolbar::showMenu()
{
    auto menu = buildMenu();
    juce::Component::SafePointer<StudioToolbar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(menuKey.get()).withMinimumWidth(240),
                       [safe](int) { if (safe != nullptr) safe->refresh(); });
}

void StudioToolbar::paint(juce::Graphics& g)
{
    // A recessed console rail the keys sit in: dark lacquer with a lit lower lip.
    const auto r = getLocalBounds().toFloat();
    const float rad = juce::jmin(6.0f, r.getHeight() * 0.16f);
    g.setGradientFill(vgrad(juce::Colour(0xff0d0c0a), juce::Colour(0xff1d1b18), r));
    g.fillRoundedRectangle(r, rad);
    g.setGradientFill(vgrad(juce::Colours::black.withAlpha(0.55f), juce::Colours::transparentBlack, r.withHeight(r.getHeight() * 0.3f)));
    g.fillRoundedRectangle(r.withHeight(r.getHeight() * 0.3f), rad);
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawRoundedRectangle(r.reduced(0.5f), rad, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(r.getX() + rad, r.getBottom() - 0.6f, r.getRight() - rad, r.getBottom() - 0.6f, 1.0f);
    // Etched dividers between the groups.
    const float sx = r.getWidth() / kDesignW;
    g.setColour(juce::Colour(0xffcdb878).withAlpha(0.28f));
    for (float x : { 395.0f, 553.0f, 679.0f })
        g.fillRect(x * sx, r.getHeight() * 0.22f, 1.0f, r.getHeight() * 0.56f);
}

void StudioToolbar::resized()
{
    const float sx = (float) getWidth() / kDesignW, sy = (float) getHeight() / kDesignH;
    auto place = [sx, sy](juce::Component& c, float x, float w)
    {
        c.setBounds(juce::roundToInt(x * sx), juce::roundToInt(4.0f * sy), juce::roundToInt(w * sx), juce::roundToInt(36.0f * sy));
    };
    // Design layout, 864 x 44: program group | compare and history | engine | studio and menu. No key is narrower
    // than 35 design px, so every key is at least 24 x 24 px at the 1120 x 700 minimum.
    place(*prev, 6.0f, 35.0f);
    place(*display, 45.0f, 232.0f);
    place(*next, 281.0f, 35.0f);
    place(*library, 320.0f, 68.0f);
    place(*compareA, 402.0f, 35.0f);
    place(*compareB, 437.0f, 35.0f);
    place(*undoKey, 476.0f, 35.0f);
    place(*redoKey, 513.0f, 35.0f);
    place(*circuitKey, 558.0f, 60.0f);
    place(*legacyKey, 618.0f, 56.0f);
    place(*pureKey, 684.0f, 64.0f);
    place(*studioKey, 752.0f, 68.0f);
    place(*menuKey, 824.0f, 35.0f);
}

} // namespace cs80::ui

#include "PerformanceDeck.h"

namespace cs80::panel
{
PerformanceDeck::PerformanceDeck(Cs80AudioProcessor& p) : PanelSection(p)
{
    buildLeftPanel();
    buildRearPanel();

    ribbon.setComponentID("ribbon_controller");
    ribbon.setTitle("Ribbon controller");
    ribbon.setDescription("Ribbon controller: touch it where you like to set the zero point, then slide left or right to bend the pitch. Lift off to return to centre.");
    ribbon.onValueChange = [this](float value) { processor.setRibbonFromUI(value); };
    addAndMakeVisible(ribbon);
    place(ribbon, geo::ribbonRect.getX(), geo::ribbonRect.getY(), geo::ribbonRect.getWidth(), geo::ribbonRect.getHeight());
}

juce::Rectangle<float> PerformanceDeck::designArea() const
{
    return juce::Rectangle<float>(geo::bodyLeft, geo::faceBottom, geo::bodyRight - geo::bodyLeft,
                                  geo::deckBottom - geo::faceBottom).withTrimmedTop(-2.0f);
}

// The original left-hand panel (Panel 3, see docs/PANEL_CONFORMANCE.md): sustain, portamento/glissando, their
// footswitch assigners, tremolo/chorus, and the foot-controller selector with the on-screen pedal.
void PerformanceDeck::buildLeftPanel()
{
    const float top = geo::deckControlTop, h = 88.0f, rule = geo::deckRuleY;
    shortCaption(addFader(cs80::P_masterSustain, 52, top, 34, h, "SUSTAIN", yellowCap()), "SUSTAIN", "SUS");
    susToggle = &addLed(cs80::P_sustainMode, "SUS I", 89, 672, 46, 62);
    susToggle->hardwareTab = true; susToggle->accent = whiteCap();
    susToggle->setTooltip("Sustain mode: I (lamp dark) or II (lamp lit).");
    addBracket("SUSTAIN", 52, rule, 82);

    addFader(cs80::P_portamento, 140, top, 34, h, "PORTA", whiteCap());
    addFader(cs80::P_glideMode, 176, top, 34, h, "GLIDE", greyCap());
    addBracket("PORTAMENTO", 140, rule, 70, 4.0f, "PORTA");

    // Rocker switches 48 x 26: a comfortable target, lit legend and LED.
    auto rocker = [this](int parameter, const char* label, const char* tip, float x, float y) -> ui::LedButton&
    {
        auto& led = addLed(parameter, label, x, y, 48, 26);
        led.rockerStyle = true;
        led.setTooltip(tip);
        return led;
    };
    rocker(cs80::P_sustainAssign, "SUSTAIN",
           "Assign the footswitch to SUSTAIN. Plugged and open switch removes the sustain release.", 216, 662);
    rocker(cs80::P_glideAssign, "PORTA",
           "Assign the footswitch to portamento/glissando. Plugged and open switch suppresses gliding.", 216, 704);
    addBracket("FOOT ASSIGN", 216, rule, 48, 4.0f, "ASSIGN");

    // Knobs are sized so their engraved legends match the faders'.
    chorusSpeedKnob = &addKnob(cs80::P_chorusSpeed, 270, 668, 46, 66, "SPEED");
    chorusDepthKnob = &addKnob(cs80::P_chorusDepth, 316, 668, 46, 66, "DEPTH");
    // Three separate tabs as named by the adjustment procedure: ON/OFF is chorus_on; TREMOLO and CHORUS are the
    // two settings of chorus_mode (the owner's "tremolo versus chorus"), so pressing the lit one keeps it.
    struct Tab { KeyButton* key; const char* title; const char* tip; };
    const Tab tabs[] = {
        { &chorusOnKey, "Tremolo/chorus ON/OFF", "Switches the tremolo/chorus section on or off." },
        { &tremoloKey, "Tremolo/chorus: TREMOLO", "Tremolo: amplitude modulation instead of the chorus delay." },
        { &chorusKey, "Tremolo/chorus: CHORUS", "Chorus: the ensemble delay." },
    };
    for (int i = 0; i < 3; ++i)
    {
        auto& key = *tabs[i].key;
        key.setTitle(tabs[i].title);
        key.setTooltip(tabs[i].tip);
        key.setClickingTogglesState(false);
        key.setWantsKeyboardFocus(true);
        addAndMakeVisible(key);
        place(key, 366.0f, 657.0f + (float) i * 27.0f, 70.0f, 25.0f);
    }
    chorusOnKey.onClick = [this]
    {
        setParameter(cs80::P_chorusOn, processor.getParamValue(cs80::P_chorusOn) >= 0.5f ? 0.0f : 1.0f);
        syncChorusState();
    };
    tremoloKey.onClick = [this] { setParameter(cs80::P_chorusMode, 1.0f); syncChorusState(); };
    chorusKey.onClick = [this] { setParameter(cs80::P_chorusMode, 0.0f); syncChorusState(); };
    addBracket("TREMOLO / CHORUS", 270, rule, 166, 4.0f, "CHORUS");
    syncChorusState();

    wahToggle = &rocker(cs80::P_footMode, "EXP",
                        "Foot controller selector: EXP = volume only, WAH = EXP-WAH sweep.", 444, 662);
    expressionPedal.setTitle("Foot controller position (expression)");
    expressionPedal.setComponentID("expression_pedal");
    expressionPedal.setDescription("On-screen foot controller: volume and the EXP-WAH sweep. MIDI CC11 follows this pedal with parity isolation OFF, in both legacy and source-performance modes. In isolation, CC11 is ignored; this on-screen pedal remains active.");
    expressionPedal.setValue(processor.getExpressionValue());
    expressionPedal.onValueChange = [this](float value) { processor.setExpressionFromUI(value); };
    addAndMakeVisible(expressionPedal);
    place(expressionPedal, 499, 661, 32, 79);
    addBracket("FOOT CONTROLLER", 444, rule, 88, 4.0f, "FOOT");
}

// The rear jack panel (Panel 5): the footswitch jack and its contact, and the output level switch.
void PerformanceDeck::buildRearPanel()
{
    auto rocker = [this](int parameter, const char* label, const char* tip, float x, float y) -> ui::LedButton&
    {
        auto& led = addLed(parameter, label, x, y, 48, 26);
        led.rockerStyle = true;
        led.setTooltip(tip);
        return led;
    };
    rocker(cs80::P_footswitchPlugged, "PLUG",
           "Footswitch plugged into the jack. With no plug the switched jack acts as a pressed switch.", 1434, 662);
    holdToggle = &rocker(cs80::P_footswitchPressed, "HOLD",
                         "Footswitch contact (PRESS). Latches; toggle it to play the switch.", 1434, 704);
    outputToggle = &rocker(cs80::P_outputLevel, "LOW",
                           "Output level switch: HIGH = unity, LOW = reduced.", 1494, 662);
    // One bracket names the block as the rear panel; the two jacks' switches are captioned under them.
    addBracket("REAR PANEL", 1434, geo::deckRuleY, 108, 4.0f, "REAR");
    syncDeckLegends();
}

void PerformanceDeck::applyCaptions(bool small)
{
    PanelSection::applyCaptions(small);
    if (holdToggle != nullptr)
    {
        const juce::String text = small ? "HLD" : "HOLD";
        if (holdToggle->getButtonText() != text) holdToggle->setButtonText(text);
    }
}

void PerformanceDeck::refresh()
{
    syncChorusState();
    syncDeckLegends();
    // Follow the pedal when MIDI CC11 or a restart moved it; never fight a drag.
    if (! expressionPedal.isMouseButtonDown()
        && std::abs(expressionPedal.getValue() - processor.getExpressionValue()) > 1.0e-4f)
        expressionPedal.setValue(processor.getExpressionValue());
}

void PerformanceDeck::setParameter(int index, float physicalValue)
{
    if (auto* parameter = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(physicalValue));
        parameter->endChangeGesture();
    }
}

void PerformanceDeck::syncChorusState()
{
    const bool on = processor.getParamValue(cs80::P_chorusOn) >= 0.5f;
    const bool tremolo = processor.getParamValue(cs80::P_chorusMode) >= 0.5f;
    chorusOnKey.setToggleState(on, juce::dontSendNotification);
    tremoloKey.setToggleState(tremolo, juce::dontSendNotification);
    chorusKey.setToggleState(! tremolo, juce::dontSendNotification);
    // With the section off its two knobs do nothing: dim them (they stay usable so a patch can be prepared).
    for (auto* knob : { chorusSpeedKnob, chorusDepthKnob })
        if (knob != nullptr && knob->getAlpha() != (on ? 1.0f : 0.42f))
            knob->setAlpha(on ? 1.0f : 0.42f);
}

// The two-way deck switches print the position they are in.
void PerformanceDeck::syncDeckLegends()
{
    if (wahToggle != nullptr)
    {
        const juce::String text = wahToggle->getToggleState() ? "WAH" : "EXP";
        if (wahToggle->getButtonText() != text) wahToggle->setButtonText(text);
    }
    if (susToggle != nullptr)
    {
        const juce::String text = susToggle->getToggleState() ? "SUS II" : "SUS I";
        if (susToggle->getButtonText() != text) susToggle->setButtonText(text);
    }
    if (outputToggle != nullptr)
    {
        const juce::String text = outputToggle->getToggleState() ? "HIGH" : "LOW";
        if (outputToggle->getButtonText() != text) outputToggle->setButtonText(text);
    }
}

// The deck is a lower step of the same panel. Its blocks are outlined in etched gold; the
// centre carries the nameplate and the ribbon.
void PerformanceDeck::paintArt(juce::Graphics& g) const
{
    const juce::Rectangle<float> deck(geo::bodyLeft, geo::faceBottom, geo::bodyRight - geo::bodyLeft, geo::deckBottom - geo::faceBottom);
    g.setGradientFill(art::vgrad(art::black(0.55f), juce::Colours::transparentBlack, { deck.getX(), deck.getY(), deck.getWidth(), 18.0f }));
    g.fillRect(deck.getX(), deck.getY(), deck.getWidth(), 18.0f);
    for (const auto& block : { geo::deckLeft, geo::deckRight })
    {
        art::ambient(g, block, 5.0f, 8.0f, 0.42f);       // each block sits slightly proud of the deck
        art::etchRect(g, block, 5.0f, 0.5f);
    }
    PanelSection::paintArt(g);
    // Captions of the rear-panel switches: the footswitch jack column and the output-level switch.
    art::legend(g, "FOOT SW", { 1428.0f, 731.0f, 60.0f, 12.0f }, spacedFont(8.5f, 0.06f), juce::Justification::centred,
                juce::Colour(0xffcfc4a2), 0.5f);
    art::legend(g, "OUTPUT", { 1488.0f, 689.0f, 60.0f, 12.0f }, spacedFont(8.5f, 0.06f), juce::Justification::centred,
                juce::Colour(0xffcfc4a2), 0.5f);

    // centre: nameplate and the ribbon slot
    const auto np = geo::nameplate;
    ui::softShadow(g, np, 3.0f, 0.0f, 5.0f, 9.0f, 0.95f);
    art::brushedMetal(g, np, 3.0f, -0.35f, 9);
    // warm the metal toward brass-grey and pull its highlights down so it does not out-shout the controls
    g.setColour(juce::Colour(0xff7a5a30).withAlpha(0.30f));
    g.fillRoundedRectangle(np, 3.0f);
    g.setColour(art::black(0.16f));
    g.fillRoundedRectangle(np, 3.0f);
    g.setGradientFill(art::hgrad(art::black(0.30f), juce::Colours::transparentBlack, { np.getX(), np.getY(), 28.0f, 1.0f }));
    g.fillRoundedRectangle(np.withWidth(28.0f), 3.0f);
    g.setGradientFill(art::hgrad(juce::Colours::transparentBlack, art::black(0.30f), { np.getRight() - 28.0f, np.getY(), 28.0f, 1.0f }));
    g.fillRoundedRectangle(np.withX(np.getRight() - 28.0f).withWidth(28.0f), 3.0f);
    g.setColour(art::white(0.34f));
    g.drawRoundedRectangle(np.reduced(1.3f), 2.5f, 0.9f);
    g.setColour(art::black(0.8f));
    g.drawRoundedRectangle(np, 3.0f, 1.0f);
    for (float sx : { np.getX() + 13.0f, np.getRight() - 13.0f })
        art::slotScrew(g, { sx, np.getCentreY() }, 4.4f, sx * 0.1f);
    art::engraved(g, "YAMAHA", np.reduced(30.0f, 3.0f), spacedFont(25.0f, 0.34f, 0.95f));
    art::legend(g, "POLYPHONIC", { np.getX() - 150.0f, np.getY() + 12.0f, 136.0f, 20.0f }, spacedFont(10.5f, 0.2f),
                juce::Justification::centredRight, juce::Colour(0xffe6dcbc));
    art::legend(g, "SYNTHESIZER", { np.getRight() + 14.0f, np.getY() + 12.0f, 136.0f, 20.0f }, spacedFont(10.5f, 0.2f),
                juce::Justification::centredLeft, juce::Colour(0xffe6dcbc));

    // The ribbon's own caption: one bracket over the whole slot. (The scale along the ribbon is printed by the
    // ribbon component itself.)
    art::bracket(g, "RIBBON CONTROLLER", geo::ribbonRect.getX() + 2.0f, geo::ribbonRect.getY() - 11.0f,
                 geo::ribbonRect.getWidth() - 4.0f, 4.0f, 9.5f);
    const auto slot = geo::ribbonRect.expanded(5.0f, 3.0f);
    art::ambient(g, slot, 5.0f, 9.0f, 0.55f);
    art::recessTray(g, slot, 5.0f, juce::Colour(0xff101010), juce::Colour(0xff070707), 8.0f, 0.8f);
}
} // namespace cs80::panel

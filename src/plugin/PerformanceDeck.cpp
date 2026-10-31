#include "PerformanceDeck.h"

namespace cs80::panel
{
PerformanceDeck::PerformanceDeck(Cs80AudioProcessor& p) : PanelSection(p)
{
    buildPerformance();
    buildKeyboardControls();

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

// Performance block: the left side of the deck. Sustain, portamento, glide and the sustain-mode tab
// form one group; chorus speed and depth sit beside the three-state chorus selector.
void PerformanceDeck::buildPerformance()
{
    const float y = 679.0f, h = 89.0f;       // 4 px below the header rule so type stays clear of it even at the 9 px floor
    // Everything sits between x50 and x386, inside the 342 px block (x48..390) that leaves the ribbon its width.
    shortCaption(addFader(cs80::P_masterSustain, 50, y, 36, h, "SUSTAIN", yellowCap()), "SUSTAIN", "SUS");
    addFader(cs80::P_portamento, 86, y, 36, h, "PORTA", whiteCap());
    addFader(cs80::P_glideMode, 122, y, 36, h, "GLIDE", greyCap());
    susToggle = &addLed(cs80::P_sustainMode, "SUS I", 160, 698, 52, 62);
    susToggle->hardwareTab = true; susToggle->accent = whiteCap();
    susToggle->setTooltip("Sustain mode: I (lamp dark) or II (lamp lit).");
    // Knobs are sized so their engraved legends match the faders' (about 10 px).
    chorusSpeedKnob = &addKnob(cs80::P_chorusSpeed, 214, 690, 48, 66, "SPEED");
    chorusDepthKnob = &addKnob(cs80::P_chorusDepth, 262, 690, 48, 66, "DEPTH");

    // Chorus: one three-position selector (OFF / CHORUS / TREMOLO) over the two switches it stands for.
    const char* names[] = { "OFF", "CHORUS", "TREMOLO" };
    const char* tips[] = { "Chorus section off.", "Chorus: the ensemble delay.", "Tremolo: amplitude modulation instead of the chorus delay." };
    for (int i = 0; i < 3; ++i)
    {
        auto& key = chorusKeys[(size_t) i];
        key.setButtonText(names[i]);
        key.setTitle(juce::String("Chorus section: ") + names[i]);
        key.setTooltip(tips[i]);
        key.setClickingTogglesState(false);
        key.setWantsKeyboardFocus(true);
        key.onClick = [this, i] { setChorusState(i); };
        addAndMakeVisible(key);
        place(key, 314.0f, 687.0f + (float) i * 27.0f, 72.0f, 25.0f);
    }
    syncChorusState();
}

// Owner controls 9/10 (keyboard contour) plus the foot-controller group that
// the original keeps on its left-hand panel: footswitch jack/contact, the two
// assigners, EXP/WAH and the output switch. The right side of the deck.
void PerformanceDeck::buildKeyboardControls()
{
    const float y = 679.0f, h = 89.0f;
    addFader(cs80::P_kbdBrillianceLow, 1212, y, 32, h, "LOW", greenCap());
    addFader(cs80::P_kbdBrillianceHigh, 1246, y, 32, h, "HIGH", greenCap());
    addFader(cs80::P_kbdLevelLow, 1284, y, 32, h, "LOW", greyCap());
    addFader(cs80::P_kbdLevelHigh, 1318, y, 32, h, "HIGH", greyCap());

    expressionPedal.setTitle("Foot controller position (expression)");
    expressionPedal.setComponentID("expression_pedal");
    expressionPedal.setDescription("On-screen foot controller: volume and the EXP-WAH sweep. MIDI CC11 follows this pedal with parity isolation OFF, in both legacy and source-performance modes. In isolation, CC11 is ignored; this on-screen pedal remains active.");
    expressionPedal.setValue(processor.getExpressionValue());
    expressionPedal.onValueChange = [this](float value) { processor.setExpressionFromUI(value); };
    addAndMakeVisible(expressionPedal);
    place(expressionPedal, 1358, 685, 32, 79);

    // Three columns of 54 px keep the block inside the key edge (x1538).
    // Rocker switches 54 x 26: a comfortable target, lit legend and LED.
    auto toggle = [this](int parameter, const char* label, const char* tip, float x, float ty) -> ui::LedButton&
    {
        auto& led = addLed(parameter, label, x, ty, 48, 26);
        led.rockerStyle = true;
        led.setTooltip(tip);
        return led;
    };
    toggle(cs80::P_footswitchPlugged, "PLUG",
           "Footswitch plugged into the jack. With no plug the switched jack acts as a pressed switch.", 1400, 684);
    holdToggle = &toggle(cs80::P_footswitchPressed, "HOLD",
           "Footswitch contact (PRESS). Latches; toggle it to play the switch.", 1450, 684);
    wahToggle = &toggle(cs80::P_footMode, "EXP",
           "Foot controller selector: EXP = volume only, WAH = EXP-WAH sweep.", 1500, 684);
    toggle(cs80::P_sustainAssign, "SUSTAIN",
           "Assign the footswitch to SUSTAIN. Plugged and open switch removes the sustain release.", 1400, 731);
    toggle(cs80::P_glideAssign, "PORTA",
           "Assign the footswitch to portamento/glissando. Plugged and open switch suppresses gliding.", 1450, 731);
    outputToggle = &toggle(cs80::P_outputLevel, "LOW",
           "Output level switch: HIGH = unity, LOW = reduced.", 1500, 731);
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

// 0 = off, 1 = chorus, 2 = tremolo, as the two stored switches say it.
int PerformanceDeck::readChorusState() const
{
    auto& apvts = processor.getAPVTS();
    const auto* on = apvts.getRawParameterValue(Cs80AudioProcessor::parameterId(cs80::P_chorusOn));
    const auto* mode = apvts.getRawParameterValue(Cs80AudioProcessor::parameterId(cs80::P_chorusMode));
    if (on == nullptr || mode == nullptr || on->load() < 0.5f) return 0;
    return mode->load() >= 0.5f ? 2 : 1;
}

void PerformanceDeck::setChorusState(int state)
{
    auto set = [this](int index, float value)
    {
        if (auto* parameter = processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(index)))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            parameter->endChangeGesture();
        }
    };
    if (state == 0) set(cs80::P_chorusOn, 0.0f);
    else { set(cs80::P_chorusMode, state == 2 ? 1.0f : 0.0f); set(cs80::P_chorusOn, 1.0f); }
    syncChorusState();
}

void PerformanceDeck::syncChorusState()
{
    const int state = readChorusState();
    for (int i = 0; i < 3; ++i)
        chorusKeys[(size_t) i].setToggleState(i == state, juce::dontSendNotification);
    // With the section off its two knobs do nothing: dim them (they stay usable so a patch can be prepared).
    for (auto* knob : { chorusSpeedKnob, chorusDepthKnob })
        if (knob != nullptr && knob->getAlpha() != (state == 0 ? 0.42f : 1.0f))
            knob->setAlpha(state == 0 ? 0.42f : 1.0f);
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
void PerformanceDeck::paintArt(juce::Graphics& g)
{
    const juce::Rectangle<float> deck(geo::bodyLeft, geo::faceBottom, geo::bodyRight - geo::bodyLeft, geo::deckBottom - geo::faceBottom);
    g.setGradientFill(art::vgrad(art::black(0.55f), juce::Colours::transparentBlack, { deck.getX(), deck.getY(), deck.getWidth(), 18.0f }));
    g.fillRect(deck.getX(), deck.getY(), deck.getWidth(), 18.0f);
    for (const auto& block : { geo::deckLeft, geo::deckRight })
    {
        art::ambient(g, block, 5.0f, 8.0f, 0.42f);       // each block sits slightly proud of the deck
        art::etchRect(g, block, 5.0f, 0.5f);
    }
    // One bracket style for every deck header.
    const float ruleY = 672.0f;
    art::bracket(g, "PERFORMANCE", 50, ruleY, 162, 4.0f);
    art::bracket(g, "CHORUS / TREMOLO", 214, ruleY, 174, 4.0f, 9.5f, "CHORUS");
    // Small editors print these at the 9 px floor, which is wider in design units: the wide headers are pulled
    // in a little so neighbouring headers (LEVEL, FOOT, SWITCH, MODE) keep a gap between their rules, while
    // the narrow ones (FOOT, MODE) keep every pixel their text needs.
    const float pull = art::isSmallEditor() ? 2.0f : 0.0f;
    auto header = [&](const char* title, float x, float y, float w, float inset, const char* shortTitle = "")
    {
        art::bracket(g, title, x + inset, y, w - 2.0f * inset, 4.0f, 9.5f, shortTitle);
    };
    // 8 px or more between neighbouring header rules (BRILL / LEVEL / FOOT / SWITCH / MODE)
    header("BRILLIANCE", 1212, ruleY, 64, pull, "BRILL");
    header("LEVEL", 1288, ruleY, 60, pull);
    header("FOOT", 1356, ruleY, 40, 0.0f);
    header("FOOTSWITCH", 1404, ruleY, 88, pull, "SWITCH");
    header("ASSIGN TO", 1404, 719.0f, 88, pull);
    header("OUTPUT", 1500, 719.0f, 48, 0.0f, "OUT");
    header("MODE", 1500, ruleY, 48, 0.0f);

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
    art::legend(g, "POLYPHONIC", { 490, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredRight, juce::Colour(0xffe6dcbc));
    art::legend(g, "SYNTHESIZER", { 974, 679, 136, 20 }, spacedFont(10.5f, 0.2f), juce::Justification::centredLeft, juce::Colour(0xffe6dcbc));

    // The ribbon's own caption: one bracket over the whole slot. (The scale along the ribbon is printed by the
    // ribbon component itself.)
    art::bracket(g, "RIBBON CONTROLLER", geo::ribbonRect.getX() + 2.0f, 713.0f, geo::ribbonRect.getWidth() - 4.0f, 4.0f, 9.5f);
    const auto slot = geo::ribbonRect.expanded(5.0f, 3.0f);
    art::ambient(g, slot, 5.0f, 9.0f, 0.55f);
    art::recessTray(g, slot, 5.0f, juce::Colour(0xff101010), juce::Colour(0xff070707), 8.0f, 0.8f);
}
} // namespace cs80::panel

#include "CentreRow.h"

#include "core/FactoryPresets.h"

namespace cs80::panel
{
CentreRow::CentreRow(Cs80AudioProcessor& p) : PanelSection(p)
{
    build();
}

juce::Rectangle<float> CentreRow::designArea() const
{
    return geo::frontStrip.expanded(2.0f);
}

juce::String CentreRow::toneTabName(int channel, int source)
{
    if (source == 13) return "PANEL";
    if (source >= 11) return "MEMORY " + juce::String(channel + (source == 11 ? 1 : 3));
    const auto name = juce::String(cs80::hardwareToneName(source, channel)).toUpperCase();
    // The core abbreviates; the panel prints the hardware wording (S1, owner p37).
    return name == "E. PIANO" ? juce::String("ELECTRIC PIANO") : name;
}

// The front strip, left to right (docs/PANEL_CONFORMANCE.md): the order is the pre-ws7 reading of PN2 with the
// keyboard-control levers, which sit on the same board (VR19-22), placed after RESONANCE.
void CentreRow::build()
{
    const float y = geo::stripLeverY, h = geo::stripLeverH, rule = geo::stripRuleY;
    float x = geo::stripFirstX;
    float groupStart = x;
    auto lever = [&](int parameter, const char* name, juce::Colour colour) -> ui::ParamKnob&
    {
        auto& control = addFader(parameter, x, y, geo::stripLeverW, h, name, colour);
        x += geo::stripLeverPitch;
        return control;
    };
    // Brackets the levers placed since the last group, then leaves the group gap.
    auto group = [&](const char* title, const char* shortTitle)
    {
        const float right = x - geo::stripLeverPitch + geo::stripLeverW;
        if (title != nullptr)
            addBracket(title, groupStart + 1.0f, rule, right - groupStart - 2.0f, 6.0f, shortTitle);
        x += geo::stripGroupGap;
        groupStart = x;
    };

    // The knobs share the faders' top and height; setLegendColumnWidth (applied after layout) sets their caption
    // and value on the faders' name and value lines.
    pitchKnob = &addKnob(cs80::P_masterTune, x, y, geo::stripKnobW, h, "PITCH");
    x += geo::stripKnobW + geo::stripGroupGap;
    groupStart = x;
    shortCaption(lever(cs80::P_detune, "DETUNE", whiteCap()), "DETUNE", "DET");
    group(nullptr, nullptr);
    shortCaption(lever(cs80::P_ringAttack, "ATTACK", blackCap()), "ATTACK", "ATK");
    legendReference = &lever(cs80::P_ringDecay, "DECAY", blackCap());
    lever(cs80::P_ringDepth, "DEPTH", greyCap());
    lever(cs80::P_ringSpeed, "SPEED", whiteCap());
    lever(cs80::P_ringAmount, "MOD", greyCap());
    group("RING MODULATOR", "RING");
    subFunction = &lever(cs80::P_subWave, "FUNCTION", blackCap());
    shortCaption(*subFunction, "FUNCTION", "FUNC");
    subSpeed = &lever(cs80::P_subSpeed, "SPEED", whiteCap());
    lever(cs80::P_subVco, "VCO", whiteCap());
    lever(cs80::P_subVcf, "VCF", greenCap());
    lever(cs80::P_subVca, "VCA", greyCap());
    group("SUB OSCILLATOR", "SUB OSC");
    shortCaption(lever(cs80::chParam(0, cs80::CP_feet), "FEET I", greyCap()), "FEET I", "FT I");
    shortCaption(lever(cs80::chParam(1, cs80::CP_feet), "FEET II", greyCap()), "FEET II", "FT II");
    group("FEET", nullptr);

    buildToneSection(x);
    x += geo::toneBezelW + geo::stripGroupGap;
    groupStart = x;

    lever(cs80::P_mix, "MIX", redCap());
    lever(cs80::P_brilliance, "BRILL", greenCap());
    lever(cs80::P_resonance, "RESO", redCap());
    group("GLOBAL", nullptr);
    lever(cs80::P_kbdBrillianceLow, "LOW", greenCap());
    lever(cs80::P_kbdBrillianceHigh, "HIGH", greenCap());
    group("KBD BRILL", "BRILL");
    lever(cs80::P_kbdLevelLow, "LOW", greyCap());
    lever(cs80::P_kbdLevelHigh, "HIGH", greyCap());
    group("KBD LEVEL", "LEVEL");
    // Initial pitch bend, then the after-touch amounts; the bracket names the group, so the levers print the
    // destination alone (titles and tooltips keep "AT SPD" and so on).
    lever(cs80::P_initBend, "BEND", whiteCap());
    shortCaption(lever(cs80::P_subAtSpeed, "AT SPD", whiteCap()), "SPEED", "SPD");
    shortCaption(lever(cs80::P_subAtVco, "AT VCO", whiteCap()), "VCO", "VCO");
    shortCaption(lever(cs80::P_subAtVcf, "AT VCF", greenCap()), "VCF", "VCF");
    group("TOUCH RESPONSE", "TOUCH");
    volumeKnob = &addKnob(cs80::P_volume, x, y, geo::stripKnobW, h, "VOLUME");
}

void CentreRow::buildToneSection(float x0)
{
    toneBezel = { x0, geo::toneTop - 7.0f, geo::toneBezelW, 2.0f * geo::toneTabH + geo::toneRowGap + 14.0f };
    addBracket("TONE SELECTOR", x0 + 15.0f, geo::stripRuleY + 6.0f, geo::toneBezelW - 30.0f);
    const std::array<juce::Colour, 14> colours = {
        yellowCap(), yellowCap(), redCap(), whiteCap(), yellowCap(),
        yellowCap(), yellowCap(), whiteCap(), greenCap(), greenCap(), greenCap(),
        whiteCap(), whiteCap(), whiteCap()
    };
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int tone = 0; tone < 14; ++tone)
        {
            const auto fullName = toneTabName(channel, tone);
            auto button = std::make_unique<juce::TextButton>(fullName);
            button->setTitle(fullName);
            button->setDescription((channel == 0 ? juce::String("Channel I") : juce::String("Channel II"))
                + ": select this voice source with Space or Return. PANEL preserves the main-panel settings.");
            button->setTooltip(fullName + (channel == 0 ? " / CHANNEL I" : " / CHANNEL II"));
            button->setClickingTogglesState(true);
            button->setRadioGroupId(0x100 + channel);
            button->setWantsKeyboardFocus(true);
            button->getProperties().set("hardwareTab", true);
            button->setColour(juce::TextButton::buttonColourId, colours[(size_t) tone]);
            button->onClick = [this, channel, tone]
            {
                processor.selectHardwareSource(channel, tone);
                syncToneMarks();
                if (onToneSelected) onToneSelected();
            };
            addAndMakeVisible(*button);
            place(*button, x0 + geo::toneMarkerW + (float) tone * geo::tonePitch,
                  geo::toneTop + (float) channel * (geo::toneTabH + geo::toneRowGap), geo::toneW, geo::toneTabH);
            toneButtons[(size_t) channel][(size_t) tone] = std::move(button);
        }
    }
}

void CentreRow::syncToneMarks()
{
    for (int channel = 0; channel < 2; ++channel)
        for (int source = 0; source < 14; ++source)
            toneButtons[(size_t) channel][(size_t) source]->setToggleState(
                processor.getHardwareSource(channel) == source, juce::dontSendNotification);
}

void CentreRow::afterLayout()
{
    // PITCH / VOLUME captions and values use the legend column of one strip lever.
    if (legendReference != nullptr)
        for (auto* knob : { pitchKnob, volumeKnob })
            if (knob != nullptr)
                knob->setLegendColumnWidth((float) legendReference->getWidth());
}

void CentreRow::paintArt(juce::Graphics& g) const
{
    art::etchRect(g, geo::frontStrip, 5.0f, 0.55f);
    PanelSection::paintArt(g);
    paintToneBezel(g);
}

void CentreRow::paintToneBezel(juce::Graphics& g) const
{
    const auto r = toneBezel;
    art::ambient(g, r, 6.0f, 14.0f, 0.55f);
    ui::softShadow(g, r, 5.0f, 0.0f, 3.0f, 8.0f, 0.85f);
    g.setGradientFill(art::vgrad(juce::Colour(0xff6a655d), juce::Colour(0xff211f1b), r));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(art::white(0.35f));
    g.drawLine(r.getX() + 5, r.getY() + 1, r.getRight() - 5, r.getY() + 1, 1.0f);
    const auto well = r.reduced(4.5f);
    art::recessTray(g, well, 3.0f, juce::Colour(0xff0c0b0a), juce::Colour(0xff030303), 16.0f, 1.0f);
    g.setColour(art::black(0.9f));
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    // Row markers: large enough to read at a glance and clear of the first tab.
    for (int row = 0; row < 2; ++row)
    {
        const float cy = geo::toneTop + (float) row * (geo::toneTabH + geo::toneRowGap) + geo::toneTabH * 0.5f;
        art::legend(g, row == 0 ? "I" : "II", { r.getX() + 2.0f, cy - 9.0f, 19.0f, 18.0f }, spacedFont(11.5f, 0.04f),
                    juce::Justification::centred, juce::Colour(0xfff0e6c4), 0.8f);
    }
}
} // namespace cs80::panel

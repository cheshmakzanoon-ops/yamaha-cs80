#include "CentreRow.h"

#include "core/FactoryPresets.h"

namespace cs80::panel
{
CentreRow::CentreRow(Cs80AudioProcessor& p) : PanelSection(p)
{
    buildMasterSection();
    buildToneSection();
}

juce::Rectangle<float> CentreRow::designArea() const
{
    return geo::lowerPlate.expanded(2.0f);
}

void CentreRow::buildMasterSection()
{
    // The lower strip follows Yamaha's PN2 panel: ring, sub oscillator,
    // feet, tone selectors, mix, brilliance and performance response.
    const float y = 510.0f, h = 126.0f;
    // The knobs share the faders' top and height; setLegendColumnWidth (applied after layout) sets their caption
    // and value on the faders' name and value lines.
    pitchKnob = &addKnob(cs80::P_masterTune, 49, y, 62, h, "PITCH");
    shortCaption(addFader(cs80::P_detune, 114, y, 39, h, "DETUNE", whiteCap()), "DETUNE", "DET");
    shortCaption(addFader(cs80::P_ringAttack, 161, y, 38, h, "ATTACK", blackCap()), "ATTACK", "ATK");
    legendReference = &addFader(cs80::P_ringDecay, 201, y, 38, h, "DECAY", blackCap());
    addFader(cs80::P_ringDepth, 241, y, 38, h, "DEPTH", greyCap());
    addFader(cs80::P_ringSpeed, 281, y, 38, h, "SPEED", whiteCap());
    addFader(cs80::P_ringAmount, 321, y, 38, h, "MOD", greyCap());
    // FUNCTION is a little wider than its neighbours, so its engraved legend would sit lower
    // than theirs. Lift the component by the difference to keep one baseline across the row.
    subFunction = &addFader(cs80::P_subWave, 373, y - 1.2f, 42, h - 0.5f, "FUNCTION", blackCap());
    shortCaption(*subFunction, "FUNCTION", "FUNC");
    subSpeed = &addFader(cs80::P_subSpeed, 417, y, 38, h, "SPEED", whiteCap());
    addFader(cs80::P_subVco, 457, y, 38, h, "VCO", whiteCap());
    addFader(cs80::P_subVcf, 497, y, 38, h, "VCF", greenCap());
    addFader(cs80::P_subVca, 537, y, 38, h, "VCA", greyCap());
    shortCaption(addFader(cs80::chParam(0, cs80::CP_feet), 582, y, 36, h, "FEET I", greyCap()), "FEET I", "FT I");
    shortCaption(addFader(cs80::chParam(1, cs80::CP_feet), 620, y, 36, h, "FEET II", greyCap()), "FEET II", "FT II");
    // Pitch follows the width of each engraved name so the legends never touch.
    addFader(cs80::P_mix, 1203, y, 38, h, "MIX", redCap());
    addFader(cs80::P_brilliance, 1241, y, 38, h, "BRILL", greenCap());
    addFader(cs80::P_resonance, 1285, y, 38, h, "RESO", redCap());
    addFader(cs80::P_initBend, 1330, y, 38, h, "BEND", whiteCap());
    addFader(cs80::P_subAtSpeed, 1374, y, 38, h, "AT SPD", whiteCap());
    addFader(cs80::P_subAtVco, 1417, y, 38, h, "AT VCO", whiteCap());
    addFader(cs80::P_subAtVcf, 1456, y, 38, h, "AT VCF", greenCap());
    volumeKnob = &addKnob(cs80::P_volume, 1497, y, 50, h, "VOLUME");
}

void CentreRow::buildToneSection()
{
    const std::array<juce::Colour, 14> colours = {
        yellowCap(), yellowCap(), redCap(), whiteCap(), yellowCap(),
        yellowCap(), yellowCap(), whiteCap(), greenCap(), greenCap(), greenCap(),
        whiteCap(), whiteCap(), whiteCap()
    };
    for (int channel = 0; channel < 2; ++channel)
    {
        for (int tone = 0; tone < 14; ++tone)
        {
            auto button = std::make_unique<juce::TextButton>(
                (tone < 11 ? juce::String(cs80::hardwareToneName(tone, channel)).toUpperCase()
                 : tone == 13 ? juce::String("PANEL")
                 : juce::String("MEMORY ") + juce::String(channel + (tone == 11 ? 1 : 3))));
            const auto fullName = button->getButtonText();
            button->setTitle(fullName);
            button->setDescription((channel == 0 ? juce::String("Channel I") : juce::String("Channel II"))
                + ": select this voice source with Space or Return. PANEL preserves the main-panel settings.");
            button->setTooltip(fullName + (channel == 0 ? " / CHANNEL I" : " / CHANNEL II"));
            button->setButtonText(fullName.replace("HARPSICHORD ", "HARPSI\nCHORD ")
                                          .replace("CLAVICHORD ", "CLAVI\nCHORD ")
                                          .replace("STRING ", "STRING\n")
                                          .replace("BRASS ", "BRASS\n")
                                          .replace("GUITAR ", "GUITAR\n")
                                          .replace("FUNKY ", "FUNKY\n")
                                          .replace("ORGAN ", "ORGAN\n")
                                          .replace("MEMORY ", "MEMORY\n"));
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
            place(*button, geo::toneX0 + (float) tone * geo::tonePitch,
                  527.0f + (float) channel * 45.0f, geo::toneW, 41.0f);
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
    // PITCH / VOLUME captions and values use the legend column of one 38 px fader.
    if (legendReference != nullptr)
        for (auto* knob : { pitchKnob, volumeKnob })
            if (knob != nullptr)
                knob->setLegendColumnWidth((float) legendReference->getWidth());
}

void CentreRow::paintArt(juce::Graphics& g)
{
    const auto lp = geo::lowerPlate;
    art::etchRect(g, lp, 5.0f, 0.55f);
    art::bracket(g, "RING MODULATOR", 158, 499, 200);
    art::bracket(g, "SUB OSCILLATOR", 370, 499, 205, 6.0f, 9.5f, "SUB OSC");
    art::bracket(g, "FEET", 580, 499, 80);
    art::bracket(g, "TONE SELECTOR", 675, 505, 514);
    art::bracket(g, "GLOBAL / TOUCH RESPONSE", 1208, 499, 340, 6.0f, 9.5f, "GLOBAL");
    paintToneBezel(g);
}

void CentreRow::paintToneBezel(juce::Graphics& g)
{
    const auto r = geo::toneBezel;
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
        const float cy = 527.0f + (float) row * 45.0f + 20.5f;
        art::legend(g, row == 0 ? "I" : "II", { 662.0f, cy - 9.0f, 19.0f, 18.0f }, spacedFont(11.5f, 0.04f),
                    juce::Justification::centred, juce::Colour(0xfff0e6c4), 0.8f);
    }
}
} // namespace cs80::panel

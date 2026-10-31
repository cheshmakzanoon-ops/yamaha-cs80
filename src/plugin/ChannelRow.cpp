#include "ChannelRow.h"

namespace cs80::panel
{
ChannelRow::ChannelRow(Cs80AudioProcessor& p, int channelIndex)
    : PanelSection(p), channel(channelIndex), tag(channelIndex == 1)
{
    const float y0 = geo::rowY0[(size_t) channel];
    auto param = [this](int local) { return cs80::chParam(channel, local); };
    float x = 458.0f;
    const float fy = y0 + 24.0f, fh = 148.0f;
    auto fader = [&](int local, const char* name, juce::Colour colour)
    {
        addFader(param(local), x, fy, 38, fh, name, colour);
        x += 40.0f;
    };
    fader(cs80::CP_pwmSpeed, "SPEED", whiteCap());
    fader(cs80::CP_pwmDepth, "PWM", greenCap());
    fader(cs80::CP_pw, "PW", greenCap());
    auto& pulse = addLed(param(cs80::CP_pulse), "PULSE", x, fy + 52.0f, 36, 56);
    pulse.hardwareTab = true; pulse.accent = blackCap(); x += 40;
    auto& saw = addLed(param(cs80::CP_saw), "SAW", x, fy + 52.0f, 36, 56);
    saw.hardwareTab = true; saw.accent = blackCap(); x += 40;
    fader(cs80::CP_noise, "NOISE", greenCap());
    x += 9;
    fader(cs80::CP_hpfCutoff, "HPF", greenCap());
    fader(cs80::CP_hpfRes, "RES H", redCap());
    fader(cs80::CP_lpfCutoff, "LPF", greenCap());
    fader(cs80::CP_lpfRes, "RES L", redCap());
    fader(cs80::CP_fIl, "IL", blackCap());
    fader(cs80::CP_fAl, "AL", blackCap());
    fader(cs80::CP_fA, "A", blackCap());
    fader(cs80::CP_fD, "D", blackCap());
    fader(cs80::CP_fR, "R", yellowCap());
    x += 9;
    fader(cs80::CP_vcfLevel, "VCF LV", greyCap());
    fader(cs80::CP_sine, "SINE", greyCap());
    fader(cs80::CP_vA, "A", blackCap());
    fader(cs80::CP_vD, "D", blackCap());
    fader(cs80::CP_vS, "S", blackCap());
    fader(cs80::CP_vR, "R", yellowCap());
    fader(cs80::CP_vLevel, "LEVEL", greyCap());
    x += 9;
    fader(cs80::CP_initBrill, "BRILL", greenCap());
    fader(cs80::CP_initLevel, "LEVEL", greyCap());
    fader(cs80::CP_afterBrill, "BRILL", greenCap());
    fader(cs80::CP_afterLevel, "LEVEL", greyCap());

    addAndMakeVisible(tag);
    tag.setViewed(channel == 0);
    place(tag, 424.0f, y0 + 66.0f, 24.0f, 40.0f);
}

juce::Rectangle<float> ChannelRow::designArea() const
{
    const float y0 = geo::rowY0[(size_t) channel];
    return juce::Rectangle<float>(418.0f, y0 - 6.0f, 1134.0f, 186.0f).expanded(2.0f);
}

// Each channel is outlined in etched gold, not boxed in a bevelled tray.
void ChannelRow::paintArt(juce::Graphics& g, int channel)
{
    const float y0 = geo::rowY0[(size_t) channel];
    const juce::Rectangle<float> frame(418.0f, y0 - 6.0f, 1134.0f, 186.0f);
    art::etchRect(g, frame, 4.0f, 0.62f);
    // (the channel numeral is an engraved ChannelTag component, lit while its curves are on the plate)
    art::bracket(g, "VCO", 459, y0 + 10, 235);
    art::bracket(g, "VCF", 706, y0 + 10, 354);
    art::bracket(g, "VCA", 1075, y0 + 10, 272);
    art::bracket(g, "INITIAL", 1365, y0 + 10, 76);
    art::bracket(g, "AFTER", 1445, y0 + 10, 76);
    // faint column separators between functional groups
    for (float x : { 702.0f, 1071.0f, 1360.0f })
        art::etchLine(g, x, y0 + 22.0f, x, y0 + 170.0f, 0.22f);
}
} // namespace cs80::panel

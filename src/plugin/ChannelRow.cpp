#include "ChannelRow.h"

namespace cs80::panel
{
ChannelRow::ChannelRow(Cs80AudioProcessor& p, int channelIndex)
    : PanelSection(p), channel(channelIndex), tag(channelIndex == 1)
{
    const float y0 = geo::rowY0[(size_t) channel];
    const float fy = y0 + geo::rowFaderY, fh = geo::rowFaderH, ruleY = y0 + 10.0f;
    auto param = [this](int local) { return cs80::chParam(channel, local); };
    float x = geo::rowFirstX;
    float groupStart = x;
    auto fader = [&](int local, const char* name, juce::Colour colour)
    {
        addFader(param(local), x, fy, geo::rowFaderW, fh, name, colour);
        x += geo::rowPitch;
    };
    auto tab = [&](int local, const char* name)
    {
        auto& led = addLed(param(local), name, x + (geo::rowPitch - 36.0f) * 0.5f - 1.0f, fy + 52.0f, 36, 56);
        led.hardwareTab = true;
        led.accent = blackCap();
        x += geo::rowPitch;
    };
    // Closes the group that started at groupStart: bracket over its columns, then the gap to the next group.
    auto group = [&](const char* title, bool gapAfter)
    {
        addBracket(title, groupStart + 1.0f, ruleY, x - groupStart - geo::rowPitch + geo::rowFaderW - 2.0f, 6.0f);
        if (gapAfter)
        {
            separators.push_back(x - geo::rowPitch + geo::rowFaderW + (geo::rowGroupGap + geo::rowPitch - geo::rowFaderW) * 0.5f);
            x += geo::rowGroupGap;
        }
        groupStart = x;
    };
    fader(cs80::CP_pwmSpeed, "SPEED", whiteCap());
    fader(cs80::CP_pwmDepth, "PWM", greenCap());
    fader(cs80::CP_pw, "PW", greenCap());
    tab(cs80::CP_pulse, "PULSE");
    tab(cs80::CP_saw, "SAW");
    fader(cs80::CP_noise, "NOISE", greenCap());
    group("VCO", true);
    fader(cs80::CP_hpfCutoff, "HPF", greenCap());
    fader(cs80::CP_hpfRes, "RES H", redCap());
    fader(cs80::CP_lpfCutoff, "LPF", greenCap());
    fader(cs80::CP_lpfRes, "RES L", redCap());
    fader(cs80::CP_fIl, "IL", blackCap());
    fader(cs80::CP_fAl, "AL", blackCap());
    fader(cs80::CP_fA, "A", blackCap());
    fader(cs80::CP_fD, "D", blackCap());
    fader(cs80::CP_fR, "R", yellowCap());
    group("VCF", true);
    fader(cs80::CP_vcfLevel, "VCF LV", greyCap());
    fader(cs80::CP_sine, "SINE", greyCap());
    fader(cs80::CP_vA, "A", blackCap());
    fader(cs80::CP_vD, "D", blackCap());
    fader(cs80::CP_vS, "S", blackCap());
    fader(cs80::CP_vR, "R", yellowCap());
    fader(cs80::CP_vLevel, "LEVEL", greyCap());
    group("VCA", true);
    fader(cs80::CP_initBrill, "BRILL", greenCap());
    fader(cs80::CP_initLevel, "LEVEL", greyCap());
    group("INITIAL", false);
    fader(cs80::CP_afterBrill, "BRILL", greenCap());
    fader(cs80::CP_afterLevel, "LEVEL", greyCap());
    group("AFTER", false);

    // The engraved numeral is a label, not a control: its pilot shows whether this row is heard.
    addAndMakeVisible(tag);
    tag.setInterceptsMouseClicks(false, false);
    tag.setWantsKeyboardFocus(false);
    tag.setTitle(channel == 0 ? "Channel I" : "Channel II");
    tag.setTooltip({});
    place(tag, geo::rowTagX, y0 + geo::rowTagY, 24.0f, 40.0f);
    refresh();
}

void ChannelRow::refresh()
{
    const bool heard = processor.getHardwareSource(channel) == 13;
    tag.setViewed(heard);
    tag.setDescription(juce::String(channel == 0 ? "Channel I" : "Channel II")
                       + (heard ? ": this row is heard (tone selector on PANEL)."
                                : ": a tone or memory is selected, so this row is not heard."));
}

juce::Rectangle<float> ChannelRow::designArea() const
{
    return geo::rowFrame(channel).expanded(2.0f);
}

// Each channel is outlined in etched gold, not boxed in a bevelled tray.
void ChannelRow::paintArt(juce::Graphics& g) const
{
    const float y0 = geo::rowY0[(size_t) channel];
    art::etchRect(g, geo::rowFrame(channel), 4.0f, 0.62f);
    PanelSection::paintArt(g);
    // faint column separators between functional groups
    for (float x : separators)
        art::etchLine(g, x, y0 + 22.0f, x, y0 + 170.0f, 0.22f);
}
} // namespace cs80::panel

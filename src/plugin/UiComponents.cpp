#include "UiComponents.h"

#include "PluginProcessor.h"

#include <cmath>

namespace cs80::ui
{

// Palette sampled from photographs of an original CS-80: warm charcoal panel,
// cream/gold silkscreen, and the five slider-cap colours.
const juce::Colour ivory { 0xffe9dfc2 };
const juce::Colour ivoryDim { 0xffa89f86 };
const juce::Colour amber { 0xffd9a640 };
const juce::Colour amberBright { 0xffffd47a };
const juce::Colour teal { 0xff2f9a55 };
const juce::Colour charcoal { 0xff0e0d0c };
const juce::Colour panelDark { 0xff1b1a18 };
const juce::Colour panelMid { 0xff2e2b27 };
const juce::Colour lineColour { 0xff4d4739 };
const juce::Colour gold { 0xffcdb878 };
const juce::Colour capCream { 0xffdcd8c4 };
const juce::Colour capGreen { 0xff2b9a50 };
const juce::Colour capPink { 0xffe0205f };
const juce::Colour capYellow { 0xffe9e03e };
const juce::Colour capBlack { 0xff1a1a1f };
const juce::Colour capGrey { 0xffbdb7ae };

namespace
{
const char* const kFeet[] = { "16'", "8'", "5 1/3'", "4'", "2 2/3'", "2'" };
const char* const kSubWaves[] = { "SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "RANDOM" };
const char* const kOriginalSubWaves[] = { "SINE", "SAW DOWN", "SAW UP", "SQUARE", "NOISE", "EXT" };
const char* const kDestinations[] = { "OFF", "PITCH", "VCF", "VCA", "PWM", "PAN" };
const char* const kFxNames[] = { "OFF", "CHORUS", "PHASER", "TAPE DELAY", "PLATE REVERB", "DRIVE" };
const char* const kArpModes[] = { "UP", "DOWN", "UP / DOWN", "RANDOM", "PLAYED" };
const char* const kGlideModes[] = { "OFF", "PORTAMENTO", "GLISSANDO" };

juce::String number(float value, int decimals)
{
    return juce::String(value, decimals);
}

juce::String seconds(float value)
{
    if (value < 0.01f)
        return number(value * 1000.0f, 1) + " ms";
    if (value < 1.0f)
        return number(value * 1000.0f, value < 0.1f ? 1 : 0) + " ms";
    return number(value, value < 10.0f ? 2 : 1) + " s";
}

juce::String rate(float value)
{
    return number(value, value < 10.0f ? 2 : 1) + " Hz";
}

juce::String percent(float value)
{
    return juce::String(juce::roundToInt(value * 100.0f)) + "%";
}

juce::String enumName(const char* const* names, int count, float value)
{
    const int index = juce::jlimit(0, count - 1, juce::roundToInt(value));
    return names[index];
}

juce::Font displayFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(),
                                        height, style));
}
} // namespace

juce::String formatParamValue(int parameterIndex, float value)
{
    const auto& definition = cs80::paramDef(parameterIndex);
    const int local = parameterIndex >= cs80::P_numGlobals && parameterIndex < cs80::P_legacyNumParams
        ? (parameterIndex - cs80::P_ch1Start) % cs80::CP_count
        : -1;

    if (parameterIndex == cs80::P_subAtVcf || parameterIndex == cs80::P_subExtLevel) return percent(value);
    if (parameterIndex == cs80::P_subOriginalMode) return value >= 0.5f ? "ORIGINAL" : "LEGACY";
    if (parameterIndex == cs80::P_subOriginalFunction) return enumName(kOriginalSubWaves, 6, value);
    if (parameterIndex == cs80::P_subOriginalSpeed) return rate(value);
    if (parameterIndex >= cs80::P_kbdBrillianceLow && parameterIndex <= cs80::P_kbdLevelHigh)
        return (value > 0.004f ? "+" : "") + juce::String(juce::roundToInt(value * 100.0f)) + "%";
    switch (parameterIndex)
    {
        case cs80::P_footswitchPlugged: return value >= 0.5f ? "PLUGGED" : "NO PLUG";
        case cs80::P_footswitchPressed: return value >= 0.5f ? "PRESSED" : "OPEN";
        case cs80::P_sustainAssign:
        case cs80::P_glideAssign: return value >= 0.5f ? "ASSIGNED" : "OFF";
        case cs80::P_footMode: return value >= 0.5f ? "EXP-WAH" : "EXP";
        case cs80::P_outputLevel: return value >= 0.5f ? "HIGH" : "LOW";
        default: break;
    }
    if (parameterIndex < cs80::P_numGlobals)
    {
        switch (parameterIndex)
        {
            case cs80::P_masterTune: return (value >= 0.0f ? "+" : "") + number(value, 2) + " st";
            case cs80::P_detune: return number(value, 1) + " ct";
            case cs80::P_mix: return "I " + juce::String(juce::roundToInt(value * 100.0f));
            case cs80::P_brilliance:
                return (value >= 0.0f ? "+" : "") + number(value, 2) + " oct";
            case cs80::P_resonance:
            case cs80::P_masterSustain: return number(value * 10.0f, 1);
            case cs80::P_volume: return percent(value);
            case cs80::P_initBend: return percent(value);
            case cs80::P_portamento: return value <= 0.001f ? "OFF" : seconds(value);
            case cs80::P_glideMode: return enumName(kGlideModes, 3, value);
            case cs80::P_sustainMode: return value >= 0.5f ? "II" : "I";
            case cs80::P_keyRelease: return value <= 0.001f ? "OFF" : seconds(value);
            case cs80::P_ribbon: return number(value, 2);
            case cs80::P_subWave: return enumName(kSubWaves, 6, value);
            case cs80::P_subSpeed: return rate(value);
            case cs80::P_subVco:
            case cs80::P_subVcf:
            case cs80::P_subVca:
            case cs80::P_subAtSpeed:
            case cs80::P_subAtVco:
            case cs80::P_modWheelSub:
            case cs80::P_modWheelVcf:
            case cs80::P_macro1:
            case cs80::P_macro2:
            case cs80::P_macro3:
            case cs80::P_macro4: return percent(value);
            case cs80::P_ringSpeed: return rate(value);
            case cs80::P_ringDepth:
            case cs80::P_ringAmount: return percent(value);
            case cs80::P_ringAttack:
            case cs80::P_ringDecay: return seconds(value);
            case cs80::P_chorusOn:
            case cs80::P_outputMode:
            case cs80::P_mpeOn:
            case cs80::P_arpOn: return value >= 0.5f ? "ON" : "OFF";
            case cs80::P_chorusMode: return value >= 0.5f ? "TREMOLO" : "CHORUS";
            case cs80::P_chorusSpeed:
            case cs80::P_lfo1Rate:
            case cs80::P_lfo2Rate:
            case cs80::P_lfo3Rate:
            case cs80::P_arpRate: return rate(value);
            case cs80::P_chorusDepth:
            case cs80::P_lfo1Depth:
            case cs80::P_lfo2Depth:
            case cs80::P_lfo3Depth:
            case cs80::P_fx1Mix:
            case cs80::P_fx2Mix:
            case cs80::P_fx3Mix: return percent(value);
            case cs80::P_voiceDispersion:
            case cs80::P_drift:
            case cs80::P_analogNoise: return percent(value);
            case cs80::P_pitchBendRange:
            case cs80::P_mpeBendRange: return number(value, 0) + " st";
            case cs80::P_tuningRef: return number(value, 1) + " Hz";
            case cs80::P_lfo1Dest:
            case cs80::P_lfo2Dest:
            case cs80::P_lfo3Dest: return enumName(kDestinations, 6, value);
            case cs80::P_fx1Type:
            case cs80::P_fx2Type:
            case cs80::P_fx3Type: return enumName(kFxNames, 6, value);
            case cs80::P_arpMode: return enumName(kArpModes, 5, value);
            case cs80::P_arpOctaves: return juce::String(juce::roundToInt(value)) + " oct";
            default: break;
        }
    }

    switch (local)
    {
        case cs80::CP_saw:
        case cs80::CP_pulse:
        case cs80::CP_noise:
        case cs80::CP_hpfOn:
        case cs80::CP_lpfOn: return value >= 0.5f ? "ON" : "OFF";
        case cs80::CP_sine: return percent(value);
        case cs80::CP_pw: return number(value * 100.0f, 1) + "%";
        case cs80::CP_pwmSpeed: return rate(value);
        case cs80::CP_pwmDepth: return percent(value);
        case cs80::CP_feet: return enumName(kFeet, 6, value);
        case cs80::CP_hpfCutoff:
        case cs80::CP_lpfCutoff:
            if (value >= 1000.0f)
                return number(value / 1000.0f, 2) + " kHz";
            return number(value, 0) + " Hz";
        case cs80::CP_hpfRes:
        case cs80::CP_lpfRes: return number(value * 10.0f, 1);
        case cs80::CP_fIl:
        case cs80::CP_fAl:
        case cs80::CP_initBrill:
        case cs80::CP_initLevel:
        case cs80::CP_afterBrill:
        case cs80::CP_afterLevel:
        case cs80::CP_vcfLevel:
        case cs80::CP_vLevel:
        case cs80::CP_vS: return percent(value);
        case cs80::CP_fA:
        case cs80::CP_fD:
        case cs80::CP_fR:
        case cs80::CP_vA:
        case cs80::CP_vD:
        case cs80::CP_vR: return seconds(value);
        case cs80::CP_pan:
            if (std::abs(value) < 0.01f) return "C";
            return value < 0.0f ? "L" + juce::String(juce::roundToInt(-value * 100.0f))
                                : "R" + juce::String(juce::roundToInt(value * 100.0f));
        default: break;
    }

    return number(value, 2) + (definition.unit != nullptr ? juce::String(" ") + definition.unit : juce::String());
}

juce::String shortParamName(int parameterIndex)
{
    return juce::String(cs80::paramDef(parameterIndex).name);
}

void softShadow(juce::Graphics& g, juce::Rectangle<float> r, float radius,
                float dx, float dy, float spread, float alpha)
{
    constexpr int layers = 5;
    for (int i = 0; i < layers; ++i)
    {
        const float t = (float) (i + 1) / (float) layers; // outermost (faintest) first
        const float grow = spread * (1.2f - t);
        g.setColour(juce::Colours::black.withAlpha(alpha * t * t * 0.55f));
        g.fillRoundedRectangle(r.translated(dx, dy).expanded(grow), radius + grow);
    }
}

// A raised cap seen slightly from above: lit top face, dark right/bottom
// sides that give it thickness, and a soft cast shadow on the panel.
void drawChiclet(juce::Graphics& g, juce::Rectangle<float> f, juce::Colour c, float d)
{
    softShadow(g, f, 1.5f, d * 1.1f, d * 1.6f, d * 1.2f, 0.8f);
    juce::Path side;
    side.addQuadrilateral(f.getRight(), f.getY() + 1.0f, f.getRight() + d, f.getY() + d,
                          f.getRight() + d, f.getBottom() + d, f.getRight(), f.getBottom());
    g.setColour(c.darker(0.68f));
    g.fillPath(side);
    juce::Path under;
    under.addQuadrilateral(f.getX() + 1.0f, f.getBottom(), f.getX() + d, f.getBottom() + d,
                           f.getRight() + d, f.getBottom() + d, f.getRight(), f.getBottom());
    g.setColour(c.darker(0.82f));
    g.fillPath(under);
    g.setGradientFill(juce::ColourGradient(c.brighter(0.30f), f.getX(), f.getY(),
                                           c.darker(0.16f), f.getX(), f.getBottom(), false));
    g.fillRoundedRectangle(f, 1.4f);
    g.setColour(c.brighter(0.85f).withAlpha(0.9f));
    g.drawLine(f.getX() + 1.0f, f.getY() + 0.7f, f.getRight() - 1.0f, f.getY() + 0.7f, 1.0f);
    g.setColour(c.brighter(0.6f).withAlpha(0.35f));
    g.drawLine(f.getX() + 0.7f, f.getY() + 1.5f, f.getX() + 0.7f, f.getBottom() - 1.5f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.drawLine(f.getX() + 1.0f, f.getBottom() - 0.6f, f.getRight() - 1.0f, f.getBottom() - 0.6f, 1.0f);
}

void drawWell(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    g.setColour(juce::Colour(0xff060605));
    g.fillRoundedRectangle(r, radius);
    const float depth = juce::jmin(9.0f, r.getHeight() * 0.45f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.75f), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX(), r.getY() + depth, false));
    g.fillRoundedRectangle(r.reduced(0.5f), radius);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX() + depth, r.getY(), false));
    g.fillRoundedRectangle(r.reduced(0.5f), radius);
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.drawLine(r.getX() + radius, r.getBottom() + 0.6f, r.getRight() - radius, r.getBottom() + 0.6f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, radius, 0.8f);
}

void drawRocker(juce::Graphics& g, juce::Rectangle<float> a, bool on, bool hovered)
{
    drawWell(g, a.expanded(1.5f), 2.5f);
    auto body = a.reduced(1.0f, 1.0f);
    const float h = body.getHeight();
    auto upper = body.withHeight(h * 0.52f);
    auto lower = body.withTrimmedTop(h * 0.52f);
    const auto lit = juce::Colour(hovered ? 0xff3b3b42 : 0xff303036);
    const auto dim = juce::Colour(0xff0d0d10);
    g.setGradientFill(juce::ColourGradient(on ? lit : dim.brighter(0.15f), upper.getX(), upper.getY(),
                                           on ? lit.darker(0.45f) : dim, upper.getX(), upper.getBottom(), false));
    g.fillRoundedRectangle(upper, 2.0f);
    g.setGradientFill(juce::ColourGradient(on ? dim : lit, lower.getX(), lower.getY(),
                                           on ? dim.darker(0.4f) : lit.darker(0.55f), lower.getX(), lower.getBottom(), false));
    g.fillRoundedRectangle(lower, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.28f));
    g.drawLine(body.getX() + 1.5f, (on ? body.getY() : upper.getBottom()) + 0.8f,
               body.getRight() - 1.5f, (on ? body.getY() : upper.getBottom()) + 0.8f, 1.0f);
    g.setColour(juce::Colours::black);
    g.drawLine(body.getX() + 1.0f, upper.getBottom(), body.getRight() - 1.0f, upper.getBottom(), 1.2f);
}

void drawKnob(juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    softShadow(g, { c.x - r, c.y - r, r * 2.0f, r * 2.0f }, r, r * 0.16f, r * 0.30f, r * 0.22f, 0.85f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff58585c), c.x - r * 0.75f, c.y - r * 0.85f,
                                           juce::Colour(0xff060607), c.x + r * 0.8f, c.y + r * 0.95f, false));
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    for (int i = 0; i < 30; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 30.0f;
        g.drawLine(c.x + std::sin(a) * r * 0.86f, c.y - std::cos(a) * r * 0.86f,
                   c.x + std::sin(a) * r * 0.99f, c.y - std::cos(a) * r * 0.99f, 0.9f);
    }
    const float cr = r * 0.76f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff414145), c.x - cr * 0.55f, c.y - cr * 0.75f,
                                           juce::Colour(0xff09090a), c.x + cr * 0.5f, c.y + cr * 0.85f, false));
    g.fillEllipse(c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    juce::Path sheen;
    sheen.addCentredArc(c.x, c.y, cr * 0.9f, cr * 0.9f, 0.0f, -2.5f, -0.7f, true);
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.strokePath(sheen, juce::PathStrokeType(juce::jmax(1.0f, r * 0.06f)));
    const float x0 = c.x + std::sin(angle) * cr * 0.18f, y0 = c.y - std::cos(angle) * cr * 0.18f;
    const float x1 = c.x + std::sin(angle) * cr * 0.88f, y1 = c.y - std::cos(angle) * cr * 0.88f;
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawLine(x0 + 0.8f, y0 + 1.0f, x1 + 0.8f, y1 + 1.0f, juce::jmax(1.6f, r * 0.12f));
    g.setColour(juce::Colour(0xfff1ead2));
    g.drawLine(x0, y0, x1, y1, juce::jmax(1.6f, r * 0.12f));
}

void drawScrew(juce::Graphics& g, juce::Point<float> c, float r, float turn)
{
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(c.x - r + 0.6f, c.y - r + 1.4f, r * 2.0f, r * 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd2cfc2), c.x - r * 0.6f, c.y - r * 0.7f,
                                           juce::Colour(0xff4b4840), c.x + r * 0.7f, c.y + r * 0.8f, false));
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0xff1b1a16));
    for (float a : { turn, turn + juce::MathConstants<float>::halfPi })
        g.drawLine(c.x + std::cos(a) * r * 0.75f, c.y + std::sin(a) * r * 0.75f,
                   c.x - std::cos(a) * r * 0.75f, c.y - std::sin(a) * r * 0.75f, juce::jmax(1.0f, r * 0.28f));
}

// Engraved silkscreen line: a dark shadow just below, the gold ink on top.
void etchLine(juce::Graphics& g, float x1, float y1, float x2, float y2, float alpha)
{
    g.setColour(juce::Colours::black.withAlpha(0.55f * alpha));
    g.drawLine(x1, y1 + 1.0f, x2, y2 + 1.0f, 1.0f);
    g.setColour(gold.withAlpha(alpha));
    g.drawLine(x1, y1, x2, y2, 1.0f);
}

void etchRect(juce::Graphics& g, juce::Rectangle<float> r, float radius, float alpha)
{
    g.setColour(juce::Colours::black.withAlpha(0.55f * alpha));
    g.drawRoundedRectangle(r.translated(0.0f, 1.0f), radius, 1.0f);
    g.setColour(gold.withAlpha(alpha));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

juce::Colour panelFaceAt(float y, float top, float bottom)
{
    return juce::Colour(0xff3e3b37).interpolatedWith(juce::Colour(0xff2a2825),
        juce::jlimit(0.0f, 1.0f, (y - top) / juce::jmax(1.0f, bottom - top)));
}

void fillPanelFace(juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill(juce::ColourGradient(panelFaceAt(r.getY(), r.getY(), r.getBottom()), r.getX(), r.getY(),
                                           panelFaceAt(r.getBottom(), r.getY(), r.getBottom()), r.getX(), r.getBottom(), false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.045f), r.getX(), r.getY(),
                                           juce::Colours::black.withAlpha(0.10f), r.getRight(), r.getBottom(), false));
    g.fillRect(r);
    juce::Random rng(80);
    for (int i = 0; i < (int) (r.getWidth() * r.getHeight() / 420.0f); ++i)
    {
        g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black)
                        .withAlpha(0.015f + rng.nextFloat() * 0.035f));
        g.fillRect(r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight(), 1.0f, 1.0f);
    }
}

void paintSheet(juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    softShadow(g, r, radius, 0.0f, 5.0f, 12.0f, 0.7f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34312d), r.getX(), r.getY(),
                                           juce::Colour(0xff161513), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawLine(r.getX() + radius, r.getY() + 1.5f, r.getRight() - radius, r.getY() + 1.5f, 1.0f);
    g.setColour(gold.withAlpha(0.55f));
    g.drawRoundedRectangle(r.reduced(0.5f), radius, 1.2f);
}

LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, charcoal);
    setColour(juce::Label::textColourId, ivory);
    setColour(juce::TextButton::buttonColourId, panelMid);
    setColour(juce::TextButton::buttonOnColourId, amber.withAlpha(0.32f));
    setColour(juce::TextButton::textColourOffId, ivory);
    setColour(juce::TextButton::textColourOnId, amberBright);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff12110f));
    setColour(juce::ComboBox::outlineColourId, lineColour);
    setColour(juce::ComboBox::textColourId, ivory);
    setColour(juce::ComboBox::arrowColourId, amber);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff211f1c));
    setColour(juce::PopupMenu::textColourId, ivory);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, amberBright);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f0e0c));
    setColour(juce::TextEditor::textColourId, ivory);
    setColour(juce::TextEditor::highlightColourId, amber.withAlpha(0.4f));
    setColour(juce::TextEditor::outlineColourId, lineColour);
    setColour(juce::TextEditor::focusedOutlineColourId, amber);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff5b5548));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff12110f));
    setColour(juce::ListBox::outlineColourId, lineColour);
    setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPos, float, float,
                                   juce::Slider::SliderStyle, juce::Slider& slider)
{
    const float cx = (float) x + (float) width * 0.5f;
    const float top = (float) y + 4.0f;
    const float bottom = (float) (y + height) - 4.0f;
    const float scale = juce::jlimit(0.6f, 1.5f, (float) width / 36.0f);

    // Printed scale: engraved cream ticks in groups either side of the slot.
    for (int i = 0; i <= 10; ++i)
    {
        const float yy = top + (bottom - top) * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        const float a = major ? 0.85f : 0.45f;
        for (int side = -1; side <= 1; side += 2)
        {
            const float x0 = cx + (float) side * 5.0f * scale;
            const float x1 = cx + (float) side * (major ? 12.0f : 9.0f) * scale;
            g.setColour(juce::Colours::black.withAlpha(0.5f * a));
            g.drawLine(x0, yy + 1.0f, x1, yy + 1.0f, 0.9f * scale);
            g.setColour(juce::Colour(0xffd8cfb0).withAlpha(a));
            g.drawLine(x0, yy, x1, yy, 0.9f * scale);
        }
    }
    // Black slot, recessed into the panel.
    drawWell(g, { cx - 2.0f * scale, top - 3.0f, 4.0f * scale, bottom - top + 6.0f }, 1.5f * scale);

    const float cy = juce::jlimit(top, bottom, sliderPos);
    const float w = 27.0f * scale, h = 18.0f * scale;
    const juce::Rectangle<float> cap(cx - w * 0.5f - 1.5f * scale, cy - h * 0.5f - 1.5f * scale, w, h);
    drawChiclet(g, cap, slider.findColour(juce::Slider::thumbColourId), 3.8f * scale);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.drawRoundedRectangle(cap.expanded(1.5f), 2.0f, 1.0f);
    }
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = (float) juce::jmin(width, height) * 0.5f - 4.0f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const float angle = rotaryStartAngle
        + juce::jlimit(0.0f, 1.0f, sliderPosProportional) * (rotaryEndAngle - rotaryStartAngle);
    const bool hardware = (bool) slider.getProperties()["hardwareRotary"];
    const float knobRadius = hardware ? radius : radius * 0.8f;

    // Printed dots (hardware) or a lit value arc (software pages) around the knob.
    if (hardware)
    {
        for (int n = 0; n <= 10; ++n)
        {
            const float a = rotaryStartAngle + (float) n / 10.0f * (rotaryEndAngle - rotaryStartAngle);
            const float rr = radius + 2.2f;
            g.setColour(juce::Colour(0xffd8cfb0).withAlpha(n % 5 == 0 ? 0.95f : 0.6f));
            g.fillEllipse(cx + std::sin(a) * rr - 0.8f, cy - std::cos(a) * rr - 0.8f, 1.6f, 1.6f);
        }
    }
    else
    {
        juce::Path track, valueArc;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        valueArc.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.strokePath(track, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(slider.isEnabled() ? amber : lineColour);
        g.strokePath(valueArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawKnob(g, { cx, cy }, knobRadius, angle);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&, bool hovered, bool down)
{
    if ((bool) button.getProperties()["hardwareTab"])
    {
        const auto base = button.findColour(juce::TextButton::buttonColourId);
        const bool pressed = button.getToggleState() || down;
        const float depth = juce::jlimit(1.5f, 4.5f, (float) button.getWidth() * 0.085f);
        auto face = button.getLocalBounds().toFloat().reduced(1.5f)
                        .withTrimmedRight(depth).withTrimmedBottom(depth);
        // A latched tab sits lower, with its shadow mostly hidden.
        const float travel = pressed ? depth * 0.55f : 0.0f;
        drawChiclet(g, face.translated(travel, travel), hovered && !pressed ? base.brighter(0.1f) : base,
                    depth - travel * 0.6f);
        return;
    }
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = button.getToggleState();
    if (! down)
        softShadow(g, r, 3.0f, 0.0f, 2.2f, 2.5f, 0.65f);
    auto top = on ? juce::Colour(0xff4a3b22) : juce::Colour(0xff55514a);
    auto bottom = on ? juce::Colour(0xff271d10) : juce::Colour(0xff2a2723);
    if (down)
        std::swap(top, bottom);
    if (hovered)
    {
        top = top.brighter(0.14f);
        bottom = bottom.brighter(0.1f);
    }
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.05f : 0.2f));
    g.drawLine(r.getX() + 3.0f, r.getY() + 1.2f, r.getRight() - 3.0f, r.getY() + 1.2f, 1.0f);
    g.setColour(on ? amber.withAlpha(0.8f) : juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r, 3.0f, on ? 1.2f : 1.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                 bool, bool)
{
    if (button.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xfff6df91));
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.0f), 2.0f, 1.5f);
    }
    if ((bool) button.getProperties()["hardwareTab"])
    {
        const auto base = button.findColour(juce::TextButton::buttonColourId);
        g.setColour(base.getPerceivedBrightness() < 0.56f ? juce::Colour(0xfff4f0e2) : juce::Colour(0xff1d1c18));
        g.setFont(displayFont(juce::jlimit(7.0f, 9.5f, (float) button.getWidth() * 0.25f), juce::Font::bold));
        auto text = button.getButtonText();
        if (button.getWidth() < 30)
            text = button.getTitle().replace("STRING ", "STR\n")
                                   .replace("CLAVICHORD ", "CLAV\n")
                                   .replace("HARPSICHORD ", "HARP\n")
                                   .replace("MEMORY ", "MEM\n")
                                   .replace("GUITAR ", "GUIT\n")
                                   .replace("ORGAN ", "ORG\n")
                                   .replace("FUNKY ", "FUNK\n")
                                   .replace("BRASS ", "BRS\n")
                                   .replace("E. PIANO", "E.P.")
                                   .replace("FLUTE", "FLT");
        const bool pressed = button.getToggleState() || button.isDown();
        const float depth = juce::jlimit(1.5f, 4.5f, (float) button.getWidth() * 0.085f);
        const auto face = button.getLocalBounds().toFloat().reduced(1.5f)
                              .withTrimmedRight(depth).withTrimmedBottom(depth)
                              .translated(pressed ? depth * 0.55f : 0.0f, pressed ? depth * 0.55f : 0.0f);
        const auto lines = juce::StringArray::fromLines(text);
        auto area = face.toNearestInt().reduced(1, 4);
        area.removeFromBottom(3);
        const int count = juce::jmax(1, lines.size());
        const int lineHeight = area.getHeight() / count;
        for (int i = 0; i < lines.size(); ++i)
            g.drawFittedText(lines[i], area.getX(), area.getY() + i * lineHeight,
                             area.getWidth(), lineHeight, juce::Justification::centred, 1, 0.45f);
        g.setColour(pressed ? juce::Colour(0xfff8f0c4) : juce::Colours::black.withAlpha(0.3f));
        g.fillRect(face.getCentreX() - 5.0f, face.getBottom() - 4.0f, 10.0f, 1.8f);
        return;
    }
    g.setColour(button.getToggleState() ? amberBright : ivory);
    auto font = displayFont(
                           juce::jlimit(8.0f, 14.0f, (float) button.getHeight() * 0.43f),
                           juce::Font::bold);
    font.setExtraKerningFactor(0.08f);
    g.setFont(font);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 1),
                     juce::Justification::centred, 1, 0.8f);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                               int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    g.setColour(juce::Colour(0xff080f15));
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(box.hasKeyboardFocus(false) ? amber : lineColour);
    g.drawRoundedRectangle(r, 2.5f, box.hasKeyboardFocus(false) ? 1.2f : 0.7f);

    const float arrowX = (float) width - 14.0f;
    const float arrowY = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
    arrow.lineTo(arrowX, arrowY + 3.0f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);
    g.setColour(amber);
    g.strokePath(arrow, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(7, 0, box.getWidth() - 24, box.getHeight());
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return displayFont(
                      juce::jlimit(9.0f, 13.0f, (float) box.getHeight() * 0.44f),
                      juce::Font::plain);
}

juce::Font LookAndFeel::getLabelFont(juce::Label& label)
{
    return displayFont(
                      juce::jlimit(9.0f, 14.0f, (float) label.getHeight() * 0.48f),
                      juce::Font::plain);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return displayFont(
                      juce::jlimit(8.0f, 14.0f, (float) buttonHeight * 0.42f),
                      juce::Font::bold);
}

namespace
{
juce::String parameterTooltip(int index, const juce::String& title, float value)
{
    auto text = title + ": " + formatParamValue(index, value);
    if (index == cs80::P_subExtLevel)
        text += ". Optional mono EXT bus is disabled by default; host routing is required. "
                "Maximum LEVEL: a peak 1 passband sine gives nominal inverted modulation peak 1. "
                "Causal reconstruction delay: 63.5 host samples. Software normalization, "
                "not physical voltage calibration.";
    return text;
}
} // namespace

ParamKnob::ParamKnob(Cs80AudioProcessor& p, int index, juce::String displayName)
    : processor(p), parameterIndex(index), name(displayName.isNotEmpty() ? displayName : shortParamName(index))
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity(150);
    slider.setDoubleClickReturnValue(true, cs80::paramDef(index).def);
    slider.setWantsKeyboardFocus(true);
    const auto channelName = index >= cs80::P_numGlobals && index < cs80::P_legacyNumParams
        ? juce::String(index < cs80::P_ch2Start ? "Channel I " : "Channel II ") : juce::String();
    slider.setTitle(channelName + juce::String(cs80::paramDef(index).name));
    slider.setComponentID(Cs80AudioProcessor::parameterId(index));
    slider.setDescription("Use the arrow keys to adjust this control. Double-click to restore its default.");
    slider.onValueChange = [this] {
        slider.setTooltip(parameterTooltip(parameterIndex, slider.getTitle(), (float) slider.getValue()));
        repaint();
    };
    addAndMakeVisible(slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), Cs80AudioProcessor::parameterId(index), slider);
    slider.setTooltip(parameterTooltip(parameterIndex, slider.getTitle(), (float) slider.getValue()));

}

ParamKnob::~ParamKnob() = default;

bool ParamKnob::bindParameter(int index)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (index == parameterIndex) return true;
    if (slider.isGestureInProgress() || slider.isMouseButtonDown()) return false;
    // Destroy the old attachment before changing the slider range/value.
    attachment.reset();
    parameterIndex = index;
    slider.setDoubleClickReturnValue(true, cs80::paramDef(index).def);
    slider.setTitle(juce::String(cs80::paramDef(index).name));
    slider.setComponentID(Cs80AudioProcessor::parameterId(index));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), Cs80AudioProcessor::parameterId(index), slider);
    slider.setTooltip(parameterTooltip(index, slider.getTitle(), (float) slider.getValue()));
    repaint();
    return true;
}

void ParamKnob::setHardwareSlider(juce::Colour capColour)
{
    hardwareSlider = true;
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setColour(juce::Slider::thumbColourId, capColour);
    slider.setTooltip(parameterTooltip(parameterIndex, slider.getTitle(), (float) slider.getValue()));
    resized();
    repaint();
}

void ParamKnob::paint(juce::Graphics& g)
{
    if (hardwareSlider)
    {
        const float scale = juce::jlimit(0.65f, 1.5f, (float) getWidth() / 42.0f);
        g.setFont(displayFont(9.0f * scale, juce::Font::bold));
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawFittedText(name, 0, 1, getWidth(), juce::roundToInt(24.0f * scale),
                         juce::Justification::centred, 2, 0.72f);
        g.setColour(juce::Colour(0xffe6dcbc));
        g.drawFittedText(name, 0, 0, getWidth(), juce::roundToInt(24.0f * scale),
                         juce::Justification::centred, 2, 0.72f);
        g.setFont(displayFont(7.5f * scale));
        g.setColour(juce::Colour(0xffb5aa86));
        g.drawFittedText(formatParamValue(parameterIndex, (float) slider.getValue()),
                         0, getHeight() - juce::roundToInt(16.0f * scale), getWidth(),
                         juce::roundToInt(16.0f * scale), juce::Justification::centred, 1, 0.65f);
        return;
    }

    const float h = (float) getHeight();
    const float nameHeight = juce::jlimit(10.0f, 15.0f, h * 0.20f);
    const float valueHeight = juce::jlimit(9.0f, 14.0f, h * 0.18f);

    auto nameFont = displayFont(
                               juce::jlimit(7.5f, 11.5f, h * 0.155f), juce::Font::bold);
    nameFont.setExtraKerningFactor(0.055f);
    g.setFont(nameFont);
    g.setColour(ivory.withAlpha(0.92f));
    g.drawFittedText(name, juce::Rectangle<int>(0, 0, getWidth(), (int) nameHeight + 2),
                     juce::Justification::centred, 1, 0.72f);

    auto valueFont = displayFont(
                                juce::jlimit(7.0f, 11.0f, h * 0.15f), juce::Font::plain);
    g.setFont(valueFont);
    g.setColour(amber.withAlpha(0.95f));
    g.drawFittedText(formatParamValue(parameterIndex, (float) slider.getValue()),
                     juce::Rectangle<int>(0, getHeight() - (int) valueHeight - 1,
                                          getWidth(), (int) valueHeight + 1),
                     juce::Justification::centred, 1, 0.65f);
}

void ParamKnob::resized()
{
    if (hardwareSlider)
    {
        const float scale = juce::jlimit(0.65f, 1.5f, (float) getWidth() / 42.0f);
        const int top = juce::roundToInt(26.0f * scale);
        const int bottom = juce::roundToInt(18.0f * scale);
        slider.setBounds(1, top, getWidth() - 2, juce::jmax(8, getHeight() - top - bottom));
        return;
    }

    const float h = (float) getHeight();
    const float nameHeight = juce::jlimit(10.0f, 15.0f, h * 0.20f);
    const float valueHeight = juce::jlimit(9.0f, 14.0f, h * 0.18f);
    const int top = (int) nameHeight;
    const int bottom = getHeight() - (int) valueHeight;
    slider.setBounds(2, top, getWidth() - 4, juce::jmax(8, bottom - top));
}

void ParamKnob::mouseDoubleClick(const juce::MouseEvent&)
{
    slider.setValue(cs80::paramDef(parameterIndex).def, juce::sendNotificationSync);
}

LedButton::LedButton(const juce::String& text, bool momentary)
    : juce::Button(text)
{
    setClickingTogglesState(! momentary);
    setMouseClickGrabsKeyboardFocus(true);
    setWantsKeyboardFocus(true);
    setTitle(text);
    setDescription("Press Space or Return to operate this switch.");
    onStateChange = [this] {
        setTooltip(getTitle() + (getToggleState() ? ": ON" : ": OFF"));
    };
    setTooltip(text + ": OFF");
}

void LedButton::paintButton(juce::Graphics& g, bool hovered, bool down)
{
    if (hardwareTab)
    {
        const bool on = getToggleState();
        if (accent.getPerceivedBrightness() < 0.2f)
        {
            // Black rocker with its printed legend, as on the original oscillator row.
            auto r = getLocalBounds().toFloat().reduced(getWidth() * 0.07f, 1.0f);
            drawRocker(g, r, on || down, hovered);
            const auto upper = r.withHeight(r.getHeight() * 0.5f);
            const float cx = upper.getCentreX(), cy = upper.getCentreY();
            const float u = juce::jlimit(2.5f, 5.0f, r.getWidth() * 0.16f);
            juce::Path glyph;
            if (getButtonText() == "SAW")
            {
                glyph.startNewSubPath(cx - u, cy + u * 0.6f); glyph.lineTo(cx, cy - u * 0.6f);
                glyph.lineTo(cx, cy + u * 0.6f); glyph.lineTo(cx + u, cy - u * 0.6f);
            }
            else
            {
                glyph.startNewSubPath(cx - u, cy + u * 0.6f); glyph.lineTo(cx - u, cy - u * 0.6f);
                glyph.lineTo(cx, cy - u * 0.6f); glyph.lineTo(cx, cy + u * 0.6f);
                glyph.lineTo(cx + u, cy + u * 0.6f);
            }
            g.setColour(on ? juce::Colour(0xfff1ead2) : juce::Colour(0xff8b8574));
            g.strokePath(glyph, juce::PathStrokeType(1.3f));
            g.setColour(on ? juce::Colour(0xffc8bf9b) : juce::Colour(0xff6f6a5c));
            g.setFont(displayFont(juce::jlimit(6.0f, 8.5f, r.getWidth() * 0.22f), juce::Font::bold));
            g.drawFittedText(getButtonText(), r.withTrimmedTop(r.getHeight() * 0.55f).toNearestInt(),
                             juce::Justification::centred, 1, 0.6f);
            return;
        }
        const float depth = juce::jlimit(1.5f, 4.5f, (float) getWidth() * 0.085f);
        auto face = getLocalBounds().toFloat().reduced(2.0f).withTrimmedRight(depth).withTrimmedBottom(depth);
        const float travel = on || down ? depth * 0.55f : 0.0f;
        face.translate(travel, travel);
        drawChiclet(g, face, hovered ? accent.brighter(0.1f) : accent, depth - travel * 0.6f);
        g.setColour(juce::Colour(0xff1d1c18));
        g.setFont(displayFont(juce::jlimit(7.0f, 10.5f, face.getWidth() * 0.22f), juce::Font::bold));
        g.drawFittedText(getButtonText(), face.reduced(2.0f, 4.0f).toNearestInt(), juce::Justification::centred, 2, 0.65f);
        g.setColour(on ? juce::Colour(0xfff8f0c4) : juce::Colours::black.withAlpha(0.3f));
        g.fillRect(face.getCentreX() - 5.0f, face.getBottom() - 4.0f, 10.0f, 1.8f);
        return;
    }

    auto r = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = getToggleState();
    if (! down)
        softShadow(g, r, 3.0f, 0.0f, 2.0f, 2.2f, 0.6f);
    juce::Colour top = on ? juce::Colour(0xff4b3d24) : juce::Colour(0xff4f4b45);
    juce::Colour bottom = on ? juce::Colour(0xff2a2011) : juce::Colour(0xff272420);
    if (down) std::swap(top, bottom);
    if (hovered)
    {
        top = top.brighter(0.12f);
        bottom = bottom.brighter(0.08f);
    }
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.04f : 0.17f));
    g.drawLine(r.getX() + 3.0f, r.getY() + 1.2f, r.getRight() - 3.0f, r.getY() + 1.2f, 1.0f);
    g.setColour(on ? accent.withAlpha(0.8f) : juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r, 3.0f, on ? 1.2f : 1.0f);

    // Jewel lamp: dark glass when off, glowing amber when on.
    const float led = juce::jlimit(3.5f, 7.0f, r.getHeight() * 0.30f);
    const float lx = r.getX() + 7.0f + led * 0.5f;
    const float ly = r.getCentreY();
    if (on)
    {
        g.setColour(accent.withAlpha(0.16f));
        g.fillEllipse(lx - led * 1.7f, ly - led * 1.7f, led * 3.4f, led * 3.4f);
        g.setColour(accent.withAlpha(0.28f));
        g.fillEllipse(lx - led, ly - led, led * 2.0f, led * 2.0f);
    }
    g.setGradientFill(juce::ColourGradient(on ? amberBright : juce::Colour(0xff4a2a1c), lx - led * 0.3f, ly - led * 0.4f,
                                           on ? amber.darker(0.3f) : juce::Colour(0xff160c08), lx + led * 0.4f, ly + led * 0.5f, false));
    g.fillEllipse(lx - led * 0.5f, ly - led * 0.5f, led, led);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.7f : 0.15f));
    g.fillEllipse(lx - led * 0.28f, ly - led * 0.34f, led * 0.26f, led * 0.22f);

    auto textArea = r.withTrimmedLeft(led + 12.0f).withTrimmedRight(3.0f);
    auto font = displayFont(
                           juce::jlimit(7.5f, 11.5f, r.getHeight() * 0.38f), juce::Font::bold);
    font.setExtraKerningFactor(0.03f);
    g.setFont(font);
    g.setColour(on ? amberBright : ivory.withAlpha(0.88f));
    g.drawFittedText(getButtonText(), textArea.toNearestInt(),
                     juce::Justification::centredLeft, 1, 0.7f);
}

RibbonStrip::RibbonStrip()
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setRepaintsOnMouseActivity(true);

}

void RibbonStrip::paint(juce::Graphics& g)
{
    // Black slot with a raised centre ridge, like the original ribbon.
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    drawWell(g, r, 3.0f);
    const auto ridge = r.reduced(10.0f, r.getHeight() * 0.36f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3935), 0, ridge.getY(),
                                           juce::Colour(0xff050504), 0, ridge.getBottom(), false));
    g.fillRoundedRectangle(ridge, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawLine(ridge.getX() + 2.0f, ridge.getY() + 0.8f, ridge.getRight() - 2.0f, ridge.getY() + 0.8f, 1.0f);
    if (touching)
    {
        const float posX = anchorX + value * (float) getWidth();
        g.setColour(juce::Colour(0xfff5e8b3).withAlpha(0.55f));
        g.fillEllipse(posX - 6, r.getCentreY() - 6, 12, 12);
    }
}

void RibbonStrip::setFromX(float x)
{
    value = juce::jlimit(-1.0f, 1.0f, (x - anchorX) / juce::jmax(1.0f, (float) getWidth()));
    if (onValueChange) onValueChange(value);
    repaint();
}

void RibbonStrip::mouseDown(const juce::MouseEvent& e)
{
    touching = true;
    anchorX = e.position.x;
    setFromX(anchorX);
}
void RibbonStrip::mouseDrag(const juce::MouseEvent& e) { setFromX(e.position.x); }
void RibbonStrip::mouseUp(const juce::MouseEvent&)
{
    touching = false;
    value = 0.0f;
    if (onValueChange) onValueChange(0.0f);
    repaint();
}

PerformanceWheel::PerformanceWheel(const juce::String& label, bool springLoaded)
    : wheelLabel(label), spring(springLoaded)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity(true);

}

void PerformanceWheel::setValue(float newValue)
{
    value = juce::jlimit(spring ? -1.0f : 0.0f, 1.0f, newValue);
    repaint();
}

void PerformanceWheel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto title = r.removeFromTop(12.0f);
    g.setColour(juce::Colour(0xffe0d6b6));
    g.setFont(displayFont(
                         juce::jlimit(6.5f, 9.0f, r.getHeight() * 0.06f), juce::Font::bold));
    g.drawFittedText(wheelLabel, title.toNearestInt(), juce::Justification::centred, 1);

    auto track = r.reduced(6.0f, 3.0f);
    drawWell(g, track, 4.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float y = track.getY() + track.getHeight() * (float) i / 10.0f;
        g.setColour(ivory.withAlpha(i == 5 ? 0.22f : 0.08f));
        g.drawHorizontalLine((int) y, track.getX() + 3.0f, track.getRight() - 3.0f);
    }

    const float travel = track.getHeight() - 20.0f;
    float normalised = spring ? (value * 0.5f + 0.5f) : value;
    const float y = track.getBottom() - 10.0f - normalised * travel;
    juce::Rectangle<float> wheel(track.getX() + 2.5f, y - 7.0f, track.getWidth() - 5.0f, 14.0f);
    softShadow(g, wheel, 2.5f, 0.0f, 2.5f, 2.5f, 0.7f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8a867c), wheel.getX(), wheel.getY(),
                                           juce::Colour(0xff25231f), wheel.getX(), wheel.getBottom(), false));
    g.fillRoundedRectangle(wheel, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawHorizontalLine((int) wheel.getY() + 1, wheel.getX() + 2.0f, wheel.getRight() - 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(wheel, 2.5f, 1.0f);
    g.setColour(capPink);
    g.drawHorizontalLine((int) wheel.getCentreY(), wheel.getX() + 3.0f, wheel.getRight() - 3.0f);
}

void PerformanceWheel::setFromY(float y)
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop(12.0f);
    auto track = r.reduced(6.0f, 3.0f);
    const float n = juce::jlimit(0.0f, 1.0f,
                                 (track.getBottom() - 10.0f - y) / juce::jmax(1.0f, track.getHeight() - 20.0f));
    value = spring ? n * 2.0f - 1.0f : n;
    if (onValueChange)
        onValueChange(value);
    repaint();
}

void PerformanceWheel::mouseDown(const juce::MouseEvent& e) { setFromY(e.position.y); }
void PerformanceWheel::mouseDrag(const juce::MouseEvent& e) { setFromY(e.position.y); }

void PerformanceWheel::mouseUp(const juce::MouseEvent&)
{
    if (spring)
    {
        value = 0.0f;
        if (onValueChange)
            onValueChange(0.0f);
        repaint();
    }
}

} // namespace cs80::ui

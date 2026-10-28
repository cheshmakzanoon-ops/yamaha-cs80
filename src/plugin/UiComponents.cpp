#include "UiComponents.h"

#include "PluginProcessor.h"

#include <cmath>

namespace cs80::ui
{

const juce::Colour ivory { 0xffe8dfca };
const juce::Colour ivoryDim { 0xffaaa596 };
const juce::Colour amber { 0xffcf8b3c };
const juce::Colour amberBright { 0xffffbf62 };
const juce::Colour teal { 0xff4f7c82 };
const juce::Colour charcoal { 0xff070b0f };
const juce::Colour panelDark { 0xff0d151c };
const juce::Colour panelMid { 0xff16222c };
const juce::Colour lineColour { 0xff2c3b47 };

namespace
{
const char* const kFeet[] = { "16'", "8'", "4'", "2'", "1'", "1/2'" };
const char* const kSubWaves[] = { "SINE", "TRIANGLE", "SAW", "SQUARE", "S&H", "RANDOM" };
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
    const int local = parameterIndex >= cs80::P_numGlobals
        ? (parameterIndex - cs80::P_ch1Start) % cs80::CP_count
        : -1;

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

LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, charcoal);
    setColour(juce::Label::textColourId, ivory);
    setColour(juce::TextButton::buttonColourId, panelMid);
    setColour(juce::TextButton::buttonOnColourId, amber.withAlpha(0.32f));
    setColour(juce::TextButton::textColourOffId, ivory);
    setColour(juce::TextButton::textColourOnId, amberBright);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0a1117));
    setColour(juce::ComboBox::outlineColourId, lineColour);
    setColour(juce::ComboBox::textColourId, ivory);
    setColour(juce::ComboBox::arrowColourId, amber);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff101920));
    setColour(juce::PopupMenu::textColourId, ivory);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, amberBright);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff081016));
    setColour(juce::TextEditor::textColourId, ivory);
    setColour(juce::TextEditor::highlightColourId, amber.withAlpha(0.4f));
    setColour(juce::TextEditor::outlineColourId, lineColour);
    setColour(juce::TextEditor::focusedOutlineColourId, amber);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff3d4e5b));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff080f15));
    setColour(juce::ListBox::outlineColourId, lineColour);
    setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = (float) juce::jmin(width, height) * 0.5f - 3.0f;
    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const float angle = rotaryStartAngle
        + juce::jlimit(0.0f, 1.0f, sliderPosProportional) * (rotaryEndAngle - rotaryStartAngle);

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(cx - radius + 1.0f, cy - radius + 2.5f, radius * 2.0f, radius * 2.0f);

    juce::ColourGradient rim(juce::Colour(0xff56616b), cx - radius, cy - radius,
                             juce::Colour(0xff0a0e12), cx + radius, cy + radius, false);
    g.setGradientFill(rim);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    const float capRadius = radius * 0.82f;
    juce::ColourGradient cap(juce::Colour(0xff303c46), cx - capRadius * 0.5f, cy - capRadius * 0.7f,
                             juce::Colour(0xff0a1015), cx + capRadius * 0.35f, cy + capRadius * 0.8f, true);
    g.setGradientFill(cap);
    g.fillEllipse(cx - capRadius, cy - capRadius, capRadius * 2.0f, capRadius * 2.0f);

    juce::Path ticks;
    for (int i = 0; i <= 10; ++i)
    {
        const float a = rotaryStartAngle + (float) i / 10.0f * (rotaryEndAngle - rotaryStartAngle);
        const float inner = radius * (i % 5 == 0 ? 0.79f : 0.85f);
        ticks.startNewSubPath(cx + std::sin(a) * inner, cy - std::cos(a) * inner);
        ticks.lineTo(cx + std::sin(a) * radius * 0.94f, cy - std::cos(a) * radius * 0.94f);
    }
    g.setColour(ivory.withAlpha(0.27f));
    g.strokePath(ticks, juce::PathStrokeType(1.0f));

    juce::Path track;
    track.addCentredArc(cx, cy, radius * 0.96f, radius * 0.96f, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff06090c));
    g.strokePath(track, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(cx, cy, radius * 0.96f, radius * 0.96f, 0.0f,
                           rotaryStartAngle, angle, true);
    g.setColour(slider.isEnabled() ? amber : lineColour);
    g.strokePath(valueArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const float pointerLength = capRadius * 0.72f;
    g.setColour(amberBright);
    g.drawLine(cx + std::sin(angle) * capRadius * 0.18f,
               cy - std::cos(angle) * capRadius * 0.18f,
               cx + std::sin(angle) * pointerLength,
               cy - std::cos(angle) * pointerLength,
               juce::jmax(1.4f, radius * 0.10f));

    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawEllipse(cx - capRadius + 1.0f, cy - capRadius + 1.0f,
                  capRadius * 2.0f - 2.0f, capRadius * 2.0f - 2.0f, 0.7f);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&, bool hovered, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = button.getToggleState();
    auto top = on ? juce::Colour(0xff3a2b19) : juce::Colour(0xff1a252f);
    auto bottom = on ? juce::Colour(0xff241a10) : juce::Colour(0xff0b1218);
    if (down)
        std::swap(top, bottom);
    if (hovered)
    {
        top = top.brighter(0.12f);
        bottom = bottom.brighter(0.08f);
    }

    juce::ColourGradient fill(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(on ? amber.withAlpha(0.75f) : lineColour.withAlpha(hovered ? 0.95f : 0.55f));
    g.drawRoundedRectangle(r, 3.0f, on ? 1.3f : 0.8f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.08f : 0.035f));
    g.drawHorizontalLine((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                 bool, bool)
{
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

ParamKnob::ParamKnob(Cs80AudioProcessor& p, int index, juce::String displayName)
    : processor(p), parameterIndex(index), name(displayName.isNotEmpty() ? displayName : shortParamName(index))
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                               juce::MathConstants<float>::pi * 2.8f, true);
    slider.setMouseDragSensitivity(150);
    slider.setDoubleClickReturnValue(true, cs80::paramDef(index).def);
    slider.setWantsKeyboardFocus(false);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), Cs80AudioProcessor::parameterId(index), slider);

}

ParamKnob::~ParamKnob() = default;

void ParamKnob::paint(juce::Graphics& g)
{
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
    setMouseClickGrabsKeyboardFocus(false);
    setWantsKeyboardFocus(false);
}

void LedButton::paintButton(juce::Graphics& g, bool hovered, bool down)
{
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const bool on = getToggleState();
    float alpha = 1.0f;
    if (down) alpha = 0.82f;
    else if (hovered) alpha = 1.0f;

    juce::Colour top = on ? accent.darker(0.62f) : juce::Colour(0xff1d2933);
    juce::Colour bottom = on ? accent.darker(0.82f) : juce::Colour(0xff0a1116);
    if (hovered)
    {
        top = top.brighter(0.12f);
        bottom = bottom.brighter(0.08f);
    }

    juce::ColourGradient fill(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(r, 2.8f);
    g.setColour(on ? accent.withAlpha(0.8f * alpha) : lineColour.withAlpha(hovered ? 0.9f : 0.55f));
    g.drawRoundedRectangle(r, 2.8f, on ? 1.2f : 0.7f);

    const float led = juce::jlimit(3.0f, 6.0f, r.getHeight() * 0.28f);
    const float lx = r.getX() + 6.0f + led * 0.5f;
    const float ly = r.getCentreY();
    if (on)
    {
        g.setColour(accent.withAlpha(0.22f));
        g.fillEllipse(lx - led, ly - led, led * 2.0f, led * 2.0f);
    }            g.setColour(on ? amberBright : juce::Colour(0xff26323c));
    g.fillEllipse(lx - led * 0.5f, ly - led * 0.5f, led, led);

    auto textArea = r.withTrimmedLeft(led + 11.0f).withTrimmedRight(3.0f);
    auto font = displayFont(
                           juce::jlimit(7.5f, 11.5f, r.getHeight() * 0.38f), juce::Font::bold);
    font.setExtraKerningFactor(0.03f);
    g.setFont(font);
    g.setColour(on ? amberBright : ivory.withAlpha(0.86f));
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
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    juce::ColourGradient track(juce::Colour(0xff05090c), r.getX(), r.getY(),
                               juce::Colour(0xff111b23), r.getX(), r.getBottom(), false);
    g.setGradientFill(track);
    g.fillRoundedRectangle(r, 4.0f);

    g.setColour(lineColour.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 4.0f, 0.8f);

    for (int i = 1; i < 32; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 32.0f;
        g.setColour(ivory.withAlpha(i % 4 == 0 ? 0.10f : 0.04f));
        g.drawVerticalLine((int) x, r.getY() + 7.0f, r.getBottom() - 7.0f);
    }

    const float centreX = r.getCentreX();
    g.setColour(amber.withAlpha(0.28f));
    g.drawVerticalLine((int) centreX, r.getY() + 2.0f, r.getBottom() - 2.0f);

    const float posX = centreX + value * (r.getWidth() * 0.5f - 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(posX - 5.0f, r.getY() + 3.0f, 10.0f, r.getHeight() - 6.0f, 3.0f);
    juce::ColourGradient handle(amberBright, posX - 3.0f, r.getY(),
                                amber.darker(0.5f), posX + 3.0f, r.getBottom(), false);
    g.setGradientFill(handle);
    g.fillRoundedRectangle(posX - 2.5f, r.getY() + 4.0f, 5.0f, r.getHeight() - 8.0f, 2.0f);
    g.setColour(amberBright.withAlpha(0.12f));
    g.fillEllipse(posX - 15.0f, r.getCentreY() - 15.0f, 30.0f, 30.0f);

    g.setColour(ivoryDim);
    g.setFont(displayFont(
                         juce::jlimit(7.0f, 10.0f, r.getHeight() * 0.28f), juce::Font::bold));
    g.drawText("RIBBON", (int) r.getX() + 10, (int) r.getY(), 90, (int) r.getHeight(),
               juce::Justification::centredLeft);
    g.drawText("PITCH  +", (int) r.getRight() - 100, (int) r.getY(), 90, (int) r.getHeight(),
               juce::Justification::centredRight);
}

void RibbonStrip::setFromX(float x)
{
    const float centre = (float) getWidth() * 0.5f;
    value = juce::jlimit(-1.0f, 1.0f, (x - centre) / juce::jmax(1.0f, centre - 14.0f));
    if (onValueChange)
        onValueChange(value);
    repaint();
}

void RibbonStrip::mouseDown(const juce::MouseEvent& e) { setFromX(e.position.x); }
void RibbonStrip::mouseDrag(const juce::MouseEvent& e) { setFromX(e.position.x); }

void RibbonStrip::mouseUp(const juce::MouseEvent&)
{
    value = 0.0f;
    if (onValueChange)
        onValueChange(0.0f);
    repaint();
}

PerformanceWheel::PerformanceWheel(const juce::String& label, bool springLoaded)
    : wheelLabel(label), spring(springLoaded)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity(true);

}

void PerformanceWheel::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto title = r.removeFromTop(12.0f);
    g.setColour(ivoryDim);
    g.setFont(displayFont(
                         juce::jlimit(6.5f, 9.0f, r.getHeight() * 0.06f), juce::Font::bold));
    g.drawFittedText(wheelLabel, title.toNearestInt(), juce::Justification::centred, 1);

    auto track = r.reduced(6.0f, 3.0f);
    g.setColour(juce::Colour(0xff05080b));
    g.fillRoundedRectangle(track, 4.0f);
    g.setColour(lineColour.withAlpha(0.6f));
    g.drawRoundedRectangle(track, 4.0f, 0.7f);

    for (int i = 0; i <= 10; ++i)
    {
        const float y = track.getY() + track.getHeight() * (float) i / 10.0f;
        g.setColour(ivory.withAlpha(i == 5 ? 0.18f : 0.06f));
        g.drawHorizontalLine((int) y, track.getX() + 3.0f, track.getRight() - 3.0f);
    }

    const float travel = track.getHeight() - 20.0f;
    float normalised = spring ? (value * 0.5f + 0.5f) : value;
    const float y = track.getBottom() - 10.0f - normalised * travel;
    juce::Rectangle<float> wheel(track.getX() + 3.0f, y - 7.0f, track.getWidth() - 6.0f, 14.0f);
    juce::ColourGradient grip(juce::Colour(0xff4a5762), wheel.getX(), wheel.getY(),
                              juce::Colour(0xff121a21), wheel.getX(), wheel.getBottom(), false);
    g.setGradientFill(grip);
    g.fillRoundedRectangle(wheel, 2.5f);
    g.setColour(amber);
    g.drawRoundedRectangle(wheel, 2.5f, 1.0f);
    g.setColour(amber.withAlpha(0.75f));
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

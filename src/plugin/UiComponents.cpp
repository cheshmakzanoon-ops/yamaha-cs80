#include "UiComponents.h"

#include "PluginProcessor.h"

#include <atomic>
#include <cmath>
#include <map>
#include <tuple>

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


// ---------------------------------------------------------------------------
// Paint infrastructure
// ---------------------------------------------------------------------------
namespace
{
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

std::atomic<int> liveLookAndFeels { 0 };

juce::Colour whiteInk(float alpha) { return juce::Colours::white.withAlpha(alpha); }
juce::Colour blackInk(float alpha) { return juce::Colours::black.withAlpha(alpha); }

// Facet brightness for a surface whose outward normal points along screen angle a
// (0 = east, y down). 1 faces the upper-left light, 0 faces away from it.
float facetLight(float a)
{
    return juce::jmax(0.0f, -0.70711f * (std::cos(a) + std::sin(a)));
}

// Machined metal catches light twice: a strong lobe toward the light and a weak one opposite.
float rimLight(float a)
{
    const float lit = facetLight(a);
    const float back = juce::jmax(0.0f, 0.70711f * (std::cos(a) + std::sin(a)));
    return std::pow(lit, 2.5f) + 0.45f * std::pow(back, 3.0f);
}

// Arc by screen angle (JUCE measures arcs clockwise from twelve o'clock).
juce::Path screenArc(float cx, float cy, float radius, float a0, float a1)
{
    juce::Path p;
    p.addCentredArc(cx, cy, radius, radius, 0.0f, a0 + kPi * 0.5f, a1 + kPi * 0.5f, true);
    return p;
}

void conicRing(juce::Graphics& g, float cx, float cy, float radius, float width,
               juce::Colour shade, juce::Colour light, float gain, int steps = 48)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = kTwoPi * (float) i / (float) steps;
        const float a1 = kTwoPi * (float) (i + 1) / (float) steps;
        g.setColour(shade.interpolatedWith(light, juce::jlimit(0.0f, 1.0f, rimLight((a0 + a1) * 0.5f) * gain)));
        g.strokePath(screenArc(cx, cy, radius, a0 - 0.025f, a1 + 0.025f),
                     juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
}

// Images of moulded caps, knob bodies and plates, keyed by size, colour and state, rendered at
// the physical pixel scale of the target. Painting a control is then a blit plus its legend.
class PaintCache
{
public:
    struct Entry
    {
        juce::Image image;
        int width = 0, height = 0;
    };

    static PaintCache& get()
    {
        static PaintCache cache;
        return cache;
    }

    template <typename Painter>
    Entry fetch(int kind, int w, int h, juce::uint32 colour, int state, float physicalScale, Painter&& paint)
    {
        const int scale4 = juce::jlimit(4, 16, juce::roundToInt(physicalScale * 4.0f));
        const Key key { kind, w, h, colour, state, scale4 };
        const juce::ScopedLock sl(lock);
        if (auto it = images.find(key); it != images.end())
            return it->second;
        if (images.size() > 700)
            images.clear();
        const float scale = (float) scale4 * 0.25f;
        Entry entry;
        entry.width = w;
        entry.height = h;
        entry.image = juce::Image(juce::Image::ARGB, juce::jmax(1, (int) std::ceil((float) w * scale)),
                                  juce::jmax(1, (int) std::ceil((float) h * scale)), true);
        {
            juce::Graphics ig(entry.image);
            ig.addTransform(juce::AffineTransform::scale((float) entry.image.getWidth() / (float) w,
                                                         (float) entry.image.getHeight() / (float) h));
            paint(ig);
        }
        images.emplace(key, entry);
        return entry;
    }

    void clear()
    {
        const juce::ScopedLock sl(lock);
        images.clear();
    }

private:
    using Key = std::tuple<int, int, int, juce::uint32, int, int>;
    juce::CriticalSection lock;
    std::map<Key, Entry> images;
};

float physicalScaleOf(const juce::Graphics& g)
{
    return juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
}

void blit(juce::Graphics& g, const PaintCache::Entry& e, float x, float y)
{
    g.drawImage(e.image, juce::Rectangle<float>(x, y, (float) e.width, (float) e.height));
}

enum CacheKind
{
    kindCap = 1, kindTrack, kindKnob, kindTonePlate, kindSatinPlate, kindRocker,
    kindRibbon, kindWheel, kindComboArrow
};

juce::Colour glowFor(juce::Colour c)
{
    if (c.getSaturation() < 0.22f && c.getBrightness() > 0.55f)
        return juce::Colour(0xffffd98a);
    return c.withMultipliedSaturation(1.15f).brighter(0.25f);
}
} // namespace

// ---------------------------------------------------------------------------
// Shared 2.5D primitives
// ---------------------------------------------------------------------------
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
    // Lit lower and right lip of the cut-out.
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.drawLine(r.getX() + radius, r.getBottom() + 0.6f, r.getRight() - radius, r.getBottom() + 0.6f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawLine(r.getRight() + 0.6f, r.getY() + radius, r.getRight() + 0.6f, r.getBottom() - radius, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, radius, 0.8f);
}

namespace
{
// Black ridged rocker in its cut-out. The raised half carries the light and the lamp.
void paintRockerBody(juce::Graphics& g, juce::Rectangle<float> a, bool on, bool hovered)
{
    drawWell(g, a.expanded(1.5f), 2.5f);
    auto body = a.reduced(1.2f);
    const float pivot = body.getY() + body.getHeight() * 0.5f;
    const auto upper = body.withBottom(pivot);
    const auto lower = body.withTop(pivot);
    const auto raised = on ? upper : lower;
    const auto sunk = on ? lower : upper;
    const float lift = hovered ? 0.07f : 0.0f;

    // Sunk half: a dark bowl, shadowed by the lip of the cut-out and by the raised half.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff050506), sunk.getX(), on ? sunk.getBottom() : sunk.getY(),
                                           juce::Colour(0xff15151a), sunk.getX(), on ? sunk.getY() : sunk.getBottom(), false));
    g.fillRoundedRectangle(sunk, 2.0f);
    {
        const float sy = on ? sunk.getY() : sunk.getBottom();
        const float ey = on ? sunk.getY() + 6.0f : sunk.getBottom() - 6.0f;
        g.setGradientFill(juce::ColourGradient(blackInk(0.65f), sunk.getX(), sy,
                                               juce::Colours::transparentBlack, sunk.getX(), ey, false));
        g.fillRect(sunk);
    }

    // Raised half: moulded face, brightest at the crest that faces the light.
    juce::Colour crest = juce::Colour(0xff5c5c65).brighter(lift);
    juce::Colour root = juce::Colour(0xff1c1c21);
    g.setGradientFill(juce::ColourGradient(on ? root : crest, raised.getX(), raised.getY(),
                                           on ? crest : root, raised.getX(), raised.getBottom(), false));
    g.fillRoundedRectangle(raised, 2.0f);
    g.setColour(whiteInk(0.38f));
    const float crestY = on ? raised.getY() + 0.8f : raised.getY() + 0.8f;
    g.drawLine(raised.getX() + 2.0f, crestY, raised.getRight() - 2.0f, crestY, 1.0f);
    g.setColour(whiteInk(0.14f));
    g.drawLine(raised.getX() + 0.8f, raised.getY() + 2.0f, raised.getX() + 0.8f, raised.getBottom() - 2.0f, 1.0f);
    g.setColour(blackInk(0.45f));
    g.drawLine(raised.getRight() - 0.7f, raised.getY() + 2.0f, raised.getRight() - 0.7f, raised.getBottom() - 2.0f, 1.0f);

    // Grip ridges: strong on the raised half, ghosted on the sunk half.
    for (int half = 0; half < 2; ++half)
    {
        const auto area = half == 0 ? raised : sunk;
        const float strength = half == 0 ? 1.0f : 0.35f;
        const float step = 3.1f;
        for (float y = area.getY() + 3.5f; y < area.getBottom() - 2.5f; y += step)
        {
            g.setColour(blackInk(0.40f * strength));
            g.drawLine(area.getX() + 2.0f, y, area.getRight() - 2.0f, y, 0.9f);
            g.setColour(whiteInk(0.10f * strength));
            g.drawLine(area.getX() + 2.0f, y + 1.0f, area.getRight() - 2.0f, y + 1.0f, 0.8f);
        }
    }

    // Pivot crease.
    g.setColour(juce::Colours::black);
    g.drawLine(body.getX() + 0.5f, pivot, body.getRight() - 0.5f, pivot, 1.3f);
    g.setColour(whiteInk(0.10f));
    g.drawLine(body.getX() + 1.0f, pivot + 1.1f, body.getRight() - 1.0f, pivot + 1.1f, 0.8f);

    // Tiny lamp at the top of the rocker: lit when the rocker is down for ON.
    drawJewelLamp(g, { body.getCentreX(), upper.getY() + 4.2f }, juce::jlimit(1.3f, 2.1f, a.getWidth() * 0.055f),
                  juce::Colour(0xffff8a30), on);
}

void paintKnobBody(juce::Graphics& g, float cx, float cy, float R)
{
    // Cast shadow on the panel.
    softShadow(g, { cx - R, cy - R, R * 2.0f, R * 2.0f }, R, R * 0.17f, R * 0.30f, R * 0.22f, 0.9f);

    // Skirt: machined black body with a knurled band.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff55555a), cx - R * 0.7f, cy - R * 0.8f,
                                           juce::Colour(0xff060607), cx + R * 0.8f, cy + R * 0.95f, false));
    g.fillEllipse(cx - R, cy - R, R * 2.0f, R * 2.0f);

    const int teeth = R >= 20.0f ? 48 : (R >= 11.0f ? 36 : 24);
    const float k0 = R * 0.80f, k1 = R * 0.985f;
    const float lineW = juce::jmax(0.8f, R * 0.052f);
    for (int i = 0; i < teeth; ++i)
    {
        const float a = kTwoPi * (float) i / (float) teeth;
        g.setColour(blackInk(0.78f));
        g.drawLine(cx + std::cos(a) * k0, cy + std::sin(a) * k0, cx + std::cos(a) * k1, cy + std::sin(a) * k1, lineW);
        const float a2 = a + kPi / (float) teeth; // crest between two grooves
        const float lit = facetLight(a2);
        g.setColour(juce::Colour(0xff1d1d20).interpolatedWith(juce::Colour(0xffe2e2e8), lit * 0.92f).withAlpha(0.62f));
        g.drawLine(cx + std::cos(a2) * k0, cy + std::sin(a2) * k0, cx + std::cos(a2) * k1, cy + std::sin(a2) * k1, lineW);
    }
    // Outer rim and chamfer between the knurl and the cap: two machined conic rings.
    conicRing(g, cx, cy, R - 0.5f, juce::jmax(1.0f, R * 0.045f), juce::Colour(0xff0b0b0c), juce::Colour(0xffe4e4ea), 0.95f);
    conicRing(g, cx, cy, R * 0.795f, juce::jmax(1.0f, R * 0.05f), juce::Colour(0xff111113), juce::Colour(0xffd0d0d8), 0.9f);
    g.setColour(blackInk(0.85f));
    g.drawEllipse(cx - R * 0.765f, cy - R * 0.765f, R * 1.53f, R * 1.53f, juce::jmax(1.0f, R * 0.05f));

    // Cap: dark satin disc with fine circular brushing and a conic specular sheen.
    const float rc = R * 0.745f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4a50), cx - rc * 0.42f, cy - rc * 0.5f,
                                           juce::Colour(0xff080809), cx + rc * 0.75f, cy + rc * 0.9f, true));
    g.fillEllipse(cx - rc, cy - rc, rc * 2.0f, rc * 2.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path disc;
        disc.addEllipse(cx - rc, cy - rc, rc * 2.0f, rc * 2.0f);
        g.reduceClipRegion(disc);
        juce::Random rng(81);
        for (int i = 0; i < 34; ++i)
        {
            const float rr = rc * (0.10f + 0.9f * rng.nextFloat());
            g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.02f + rng.nextFloat() * 0.035f));
            g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.7f);
        }
        // Nested wedges give a soft-edged conic highlight without seams.
        const float lightA = -kPi * 0.75f; // upper left
        const float half[] = { 62.0f, 48.0f, 35.0f, 23.0f, 12.0f };
        for (float h : half)
        {
            juce::Path wedge;
            const float hr = h * kPi / 180.0f;
            wedge.addPieSegment(cx - rc * 1.2f, cy - rc * 1.2f, rc * 2.4f, rc * 2.4f,
                                lightA - hr + kPi * 0.5f, lightA + hr + kPi * 0.5f, 0.0f);
            g.setColour(whiteInk(0.050f));
            g.fillPath(wedge);
        }
        const float backA = lightA + kPi;
        for (float h : { 40.0f, 26.0f, 13.0f })
        {
            juce::Path wedge;
            const float hr = h * kPi / 180.0f;
            wedge.addPieSegment(cx - rc * 1.2f, cy - rc * 1.2f, rc * 2.4f, rc * 2.4f,
                                backA - hr + kPi * 0.5f, backA + hr + kPi * 0.5f, 0.0f);
            g.setColour(whiteInk(0.026f));
            g.fillPath(wedge);
        }
    }
    conicRing(g, cx, cy, rc - 0.4f, juce::jmax(0.9f, R * 0.04f), juce::Colour(0xff161618), juce::Colour(0xffc8c8d0), 0.8f);

    // Centre dimple.
    const float dr = R * 0.13f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff050506), cx - dr * 0.6f, cy - dr * 0.7f,
                                           juce::Colour(0xff3d3d43), cx + dr * 0.8f, cy + dr * 0.9f, false));
    g.fillEllipse(cx - dr, cy - dr, dr * 2.0f, dr * 2.0f);
}
} // namespace

void drawRocker(juce::Graphics& g, juce::Rectangle<float> a, bool on, bool hovered)
{
    const int w = (int) std::ceil(a.getWidth()), h = (int) std::ceil(a.getHeight());
    constexpr int pad = 4;
    const auto entry = PaintCache::get().fetch(kindRocker, w + pad * 2, h + pad * 2, 0, (on ? 1 : 0) | (hovered ? 2 : 0),
                                               physicalScaleOf(g), [&](juce::Graphics& ig)
    {
        paintRockerBody(ig, juce::Rectangle<float>((float) pad, (float) pad, (float) w, (float) h), on, hovered);
    });
    blit(g, entry, std::round(a.getX()) - (float) pad, std::round(a.getY()) - (float) pad);
}

void drawKnob(juce::Graphics& g, juce::Point<float> centre, float r, float angle)
{
    const float rq = std::round(r * 4.0f) * 0.25f;
    int size = (int) std::ceil(rq * 2.0f + rq * 1.3f);
    size += size & 1;
    const auto entry = PaintCache::get().fetch(kindKnob, size, size, 0, juce::roundToInt(rq * 4.0f),
                                               physicalScaleOf(g), [&](juce::Graphics& ig)
    {
        paintKnobBody(ig, (float) size * 0.5f, (float) size * 0.5f, rq);
    });
    const float cx = std::round(centre.x), cy = std::round(centre.y);
    blit(g, entry, cx - (float) size * 0.5f, cy - (float) size * 0.5f);

    // Indicator: a cream line on the cap that continues over the chamfer to the skirt edge.
    const float w = juce::jmax(1.5f, rq * 0.11f);
    const float s = std::sin(angle), c = -std::cos(angle);
    const float x0 = cx + s * rq * 0.22f, y0 = cy + c * rq * 0.22f;
    const float x1 = cx + s * rq * 0.70f, y1 = cy + c * rq * 0.70f;
    juce::Path line;
    line.startNewSubPath(x0, y0);
    line.lineTo(x1, y1);
    const juce::PathStrokeType stroke(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(blackInk(0.7f));
    g.strokePath(line, stroke, juce::AffineTransform::translation(w * 0.35f, w * 0.5f));
    g.setColour(juce::Colour(0xfff3ecd4));
    g.strokePath(line, stroke);
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

// ---------------------------------------------------------------------------
// Reusable helpers (documented in the header)
// ---------------------------------------------------------------------------
void drawBevelRect(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour base,
                   float bevel, bool raised)
{
    const auto hi = base.brighter(0.75f), lo = base.darker(0.75f);
    g.setGradientFill(juce::ColourGradient(raised ? hi : lo, r.getX(), r.getY(),
                                           raised ? lo : hi, r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    const auto inner = r.reduced(bevel);
    if (inner.isEmpty())
        return;
    g.setGradientFill(juce::ColourGradient(raised ? base.brighter(0.12f) : base.darker(0.25f), inner.getX(), inner.getY(),
                                           raised ? base.darker(0.14f) : base.darker(0.05f), inner.getX(), inner.getBottom(), false));
    g.fillRoundedRectangle(inner, juce::jmax(0.0f, radius - bevel * 0.5f));
}

void drawBrushedMetal(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour tint,
                      bool horizontalGrain)
{
    juce::ColourGradient grad(tint.brighter(0.45f), r.getX(), r.getY(), tint.darker(0.45f),
                              horizontalGrain ? r.getX() : r.getRight(),
                              horizontalGrain ? r.getBottom() : r.getY(), false);
    grad.addColour(0.30, tint.brighter(0.15f));
    grad.addColour(0.55, tint.darker(0.12f));
    grad.addColour(0.80, tint.brighter(0.08f));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, radius);

    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(r, radius);
    g.reduceClipRegion(clip);
    juce::Random rng(77);
    const int lines = juce::jlimit(8, 220, (int) ((horizontalGrain ? r.getHeight() : r.getWidth()) * 1.4f));
    for (int i = 0; i < lines; ++i)
    {
        g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.02f + rng.nextFloat() * 0.05f));
        if (horizontalGrain)
        {
            const float y = r.getY() + rng.nextFloat() * r.getHeight();
            g.drawHorizontalLine((int) y, r.getX(), r.getRight());
        }
        else
        {
            const float x = r.getX() + rng.nextFloat() * r.getWidth();
            g.drawVerticalLine((int) x, r.getY(), r.getBottom());
        }
    }
}

void drawGlassPanel(juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour tint, float sheen)
{
    g.setColour(whiteInk(0.10f)); // lit lower lip
    g.fillRoundedRectangle(r.translated(0.0f, 0.9f), radius);
    g.setGradientFill(juce::ColourGradient(tint.brighter(0.06f), r.getX(), r.getY(),
                                           tint.darker(0.5f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, radius);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(r, radius);
        g.reduceClipRegion(clip);
        g.setGradientFill(juce::ColourGradient(blackInk(0.60f), r.getX(), r.getY(),
                                               juce::Colours::transparentBlack, r.getX(), r.getY() + juce::jmin(7.0f, r.getHeight() * 0.5f), false));
        g.fillRect(r);
        g.setGradientFill(juce::ColourGradient(blackInk(0.45f), r.getX(), r.getY(),
                                               juce::Colours::transparentBlack, r.getX() + 4.0f, r.getY(), false));
        g.fillRect(r);
        if (sheen > 0.0f)
        {
            juce::Path reflect;
            reflect.addQuadrilateral(r.getX() + 2.0f, r.getY() + 1.5f, r.getX() + r.getWidth() * 0.62f, r.getY() + 1.5f,
                                     r.getX() + r.getWidth() * 0.42f, r.getY() + r.getHeight() * 0.52f,
                                     r.getX() + 2.0f, r.getY() + r.getHeight() * 0.52f);
            g.setGradientFill(juce::ColourGradient(whiteInk(0.09f * sheen), r.getX(), r.getY(),
                                                   juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.52f, false));
            g.fillPath(reflect);
        }
    }
    g.setColour(blackInk(0.85f));
    g.drawRoundedRectangle(r, radius, 1.0f);
}

void drawEngravedText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                      const juce::Font& font, juce::Justification justification, juce::Colour ink)
{
    if (text.isEmpty())
        return;
    const int lines = juce::jmax(1, juce::StringArray::fromLines(text).size());
    const auto box = area.toNearestInt();
    const int fit = juce::jlimit(1, 3, (int) (area.getHeight() / (font.getHeight() * 1.1f)));
    const int maxLines = juce::jmax(lines, fit);
    g.setFont(font);
    if (ink.getPerceivedBrightness() < 0.45f)
    {
        g.setColour(whiteInk(0.34f)); // letterpress into pale plastic: lit lower edge
        g.drawFittedText(text, box.translated(0, 1), justification, maxLines, 0.5f);
    }
    else
    {
        g.setColour(blackInk(0.70f)); // printed ink on a dark panel: drop shadow
        g.drawFittedText(text, box.translated(1, 1), justification, maxLines, 0.5f);
    }
    g.setColour(ink);
    g.drawFittedText(text, box, justification, maxLines, 0.5f);
}

void drawLampGlow(juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour, float intensity)
{
    if (intensity <= 0.0f || radius <= 0.0f)
        return;
    const float k = juce::jmin(1.5f, intensity);
    juce::ColourGradient grad(colour.withAlpha(juce::jmin(1.0f, 0.62f * k)), c.x, c.y,
                              colour.withAlpha(0.0f), c.x + radius, c.y, true);
    grad.addColour(0.30, colour.withAlpha(juce::jmin(1.0f, 0.34f * k)));
    grad.addColour(0.65, colour.withAlpha(juce::jmin(1.0f, 0.10f * k)));
    g.setGradientFill(grad);
    g.fillEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}

void drawJewelLamp(juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, bool on)
{
    if (on)
        drawLampGlow(g, c, r * 3.4f, colour, 0.9f);
    // Brass bezel, then the jewel.
    const float rb = r * 1.32f;
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe1cd92), c.x - rb * 0.7f, c.y - rb * 0.9f,
                                           juce::Colour(0xff40331a), c.x + rb * 0.8f, c.y + rb, false));
    g.fillEllipse(c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
    g.setColour(blackInk(0.65f));
    g.fillEllipse(c.x - r * 1.04f, c.y - r * 1.04f, r * 2.08f, r * 2.08f);
    juce::ColourGradient jewel(on ? colour.brighter(0.9f) : colour.darker(0.65f).withMultipliedBrightness(0.7f),
                               c.x - r * 0.28f, c.y - r * 0.34f,
                               on ? colour.darker(0.35f) : juce::Colour(0xff0a0504), c.x + r * 0.8f, c.y + r * 0.9f, true);
    jewel.addColour(0.55, on ? colour : colour.darker(0.8f));
    g.setGradientFill(jewel);
    g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour(whiteInk(on ? 0.75f : 0.18f));
    g.fillEllipse(c.x - r * 0.5f, c.y - r * 0.62f, r * 0.55f, r * 0.42f);
}

void drawFaderSlot(juce::Graphics& g, float cx, float top, float bottom, float s)
{
    const float slotW = 5.6f * s;
    const float rr = slotW * 0.5f;
    const juce::Rectangle<float> body(cx - rr, top, slotW, bottom - top);
    // Cut edge: lit chamfer on the lower right, shadowed wall on the upper left.
    g.setColour(whiteInk(0.11f));
    g.fillRoundedRectangle(body.expanded(0.7f * s).translated(0.45f * s, 0.9f * s), rr + 0.7f * s);
    g.setColour(blackInk(0.60f));
    g.fillRoundedRectangle(body.expanded(0.8f * s).translated(-0.35f * s, -0.7f * s), rr + 0.8f * s);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff010101), body.getX(), 0.0f,
                                           juce::Colour(0xff0d0c0b), body.getRight(), 0.0f, false));
    g.fillRoundedRectangle(body, rr);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(body, rr);
        g.reduceClipRegion(clip);
        g.setGradientFill(juce::ColourGradient(blackInk(0.9f), 0.0f, body.getY(),
                                               juce::Colours::transparentBlack, 0.0f, body.getY() + 9.0f * s, false));
        g.fillRect(body);
        g.setGradientFill(juce::ColourGradient(blackInk(0.8f), body.getX(), 0.0f,
                                               juce::Colours::transparentBlack, body.getX() + 2.4f * s, 0.0f, false));
        g.fillRect(body);
        // Lit inner wall on the right.
        g.setColour(whiteInk(0.10f));
        g.fillRect(body.getRight() - 0.9f * s, body.getY() + 3.0f * s, 0.9f * s, body.getHeight() - 6.0f * s);

        // The metal rail seated in the slot: a polished rod, lit from the left.
        const juce::Rectangle<float> rail(cx - 0.95f * s, body.getY() + 1.2f * s, 1.9f * s, body.getHeight() - 2.4f * s);
        juce::ColourGradient rod(juce::Colour(0xffd6d3c8), rail.getX(), 0.0f, juce::Colour(0xff1d1c1a), rail.getRight(), 0.0f, false);
        rod.addColour(0.30, juce::Colour(0xffb4b1a6));
        rod.addColour(0.62, juce::Colour(0xff5b5953));
        g.setGradientFill(rod);
        g.fillRect(rail);
        g.setGradientFill(juce::ColourGradient(blackInk(0.65f), 0.0f, rail.getY(),
                                               juce::Colours::transparentBlack, 0.0f, rail.getY() + 6.0f * s, false));
        g.fillRect(rail);
        g.setGradientFill(juce::ColourGradient(blackInk(0.55f), 0.0f, rail.getBottom(),
                                               juce::Colours::transparentBlack, 0.0f, rail.getBottom() - 6.0f * s, false));
        g.fillRect(rail);
    }
}

void drawFaderCap(juce::Graphics& g, juce::Point<float> centre, float faceWidth, juce::Colour c, CapState state)
{
    const float s = faceWidth / 24.5f;
    const bool drag = state == CapState::dragging;
    const bool hover = state == CapState::hover;
    const float faceW = faceWidth, faceH = 14.0f * s;
    const float ex = (drag ? 3.6f : 2.8f) * s, ey = (drag ? 5.0f : 4.0f) * s;
    const float lift = drag ? 0.9f * s : 0.0f;
    const juce::Rectangle<float> face(centre.x - faceW * 0.5f - lift, centre.y - faceH * 0.5f - lift, faceW, faceH);
    const float rad = 1.7f * s;
    const bool pale = c.getPerceivedBrightness() > 0.60f;

    // Hover / drag: warm light spilling around the cap.
    if (hover || drag)
        drawLampGlow(g, face.getCentre().translated(ex * 0.4f, ey * 0.4f), faceW * 0.95f,
                     juce::Colour(0xffffe6a8), drag ? 0.55f : 0.30f);

    // Cast shadow on the panel, displaced along the light direction; it grows when dragged.
    softShadow(g, { face.getX() + ex, face.getY() + ey, faceW, faceH }, rad,
               (drag ? 2.8f : 1.5f) * s, (drag ? 3.8f : 2.2f) * s, (drag ? 3.2f : 2.4f) * s, drag ? 0.95f : 0.82f);

    // Right and front faces (away from the light).
    juce::Path right;
    right.addQuadrilateral(face.getRight(), face.getY() + 1.0f * s, face.getRight() + ex, face.getY() + ey,
                           face.getRight() + ex, face.getBottom() + ey, face.getRight(), face.getBottom());
    g.setGradientFill(juce::ColourGradient(c.darker(0.55f), 0.0f, face.getY(), c.darker(0.92f), 0.0f, face.getBottom() + ey, false));
    g.fillPath(right);
    juce::Path front;
    front.addQuadrilateral(face.getX() + 1.0f * s, face.getBottom(), face.getRight(), face.getBottom(),
                           face.getRight() + ex, face.getBottom() + ey, face.getX() + ex, face.getBottom() + ey);
    g.setGradientFill(juce::ColourGradient(c.darker(0.28f), 0.0f, face.getBottom(), c.darker(0.78f), 0.0f, face.getBottom() + ey, false));
    g.fillPath(front);
    g.setColour(c.brighter(0.6f).withAlpha(0.32f));
    g.drawLine(face.getX() + 1.5f * s, face.getBottom() + 0.5f, face.getRight() - 0.5f, face.getBottom() + 0.5f, 0.9f);
    g.setColour(blackInk(0.45f));
    g.drawLine(face.getRight() + 0.2f, face.getBottom() + 0.3f, face.getRight() + ex, face.getBottom() + ey, 0.9f);
    g.drawLine(face.getX() + ex, face.getBottom() + ey - 0.4f, face.getRight() + ex, face.getBottom() + ey - 0.4f, 0.9f);

    // Top face: moulded chamfer ring, then the glossy plateau.
    g.setGradientFill(juce::ColourGradient(c.brighter(pale ? 0.45f : 0.85f), face.getX(), face.getY(),
                                           c.darker(0.40f), face.getRight(), face.getBottom(), false));
    g.fillRoundedRectangle(face, rad);
    const auto plate = face.reduced(1.25f * s);
    const float lightUp = drag ? 0.14f : (hover ? 0.08f : 0.0f);
    g.setGradientFill(juce::ColourGradient(c.brighter(0.30f + lightUp), plate.getX(), plate.getY(),
                                           c.darker(0.16f - lightUp * 0.5f), plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle(plate, rad * 0.7f);
    {
        juce::Graphics::ScopedSaveState state2(g);
        juce::Path clip;
        clip.addRoundedRectangle(plate, rad * 0.7f);
        g.reduceClipRegion(clip);
        // Glossy sheen: a diagonal window reflection from the upper left.
        g.setGradientFill(juce::ColourGradient(whiteInk(pale ? 0.34f : 0.26f), plate.getX(), plate.getY(),
                                               whiteInk(0.0f), plate.getX() + plate.getWidth() * 0.55f, plate.getY() + plate.getHeight() * 0.85f, false));
        g.fillRect(plate);
        g.setColour(blackInk(0.18f));
        g.fillRect(plate.getX(), plate.getBottom() - 1.4f * s, plate.getWidth(), 1.4f * s);

        // Grip grooves either side of the centre index line.
        const int pairs = faceH >= 11.0f ? 2 : (faceH >= 8.0f ? 1 : 0);
        const float gap = 2.35f * s;
        const float yc = plate.getCentreY();
        for (int k = 1; k <= pairs; ++k)
            for (int side = -1; side <= 1; side += 2)
            {
                const float y = yc + (float) side * (float) k * gap;
                g.setColour(blackInk(pale ? 0.30f : 0.50f));
                g.fillRect(plate.getX() + 2.2f * s, y - 0.45f * s, plate.getWidth() - 4.4f * s, 0.9f * s);
                g.setColour(whiteInk(pale ? 0.55f : 0.15f));
                g.fillRect(plate.getX() + 2.2f * s, y + 0.45f * s, plate.getWidth() - 4.4f * s, 0.8f * s);
            }
        const float lw = juce::jmax(1.0f, 1.3f * s);
        g.setColour(blackInk(0.45f));
        g.fillRect(plate.getX() + 1.5f * s, yc - lw * 0.5f + 0.9f, plate.getWidth() - 3.0f * s, lw);
        g.setColour(pale ? juce::Colour(0xff2a2822) : juce::Colour(0xfff1ead2));
        g.fillRect(plate.getX() + 1.5f * s, yc - lw * 0.5f, plate.getWidth() - 3.0f * s, lw);
    }
    // Specular edge and glint.
    g.setColour(whiteInk(pale ? 0.80f : 0.65f));
    g.drawLine(face.getX() + rad, face.getY() + 0.55f, face.getRight() - rad, face.getY() + 0.55f, 0.9f);
    g.setColour(whiteInk(0.35f));
    g.drawLine(face.getX() + 0.55f, face.getY() + rad, face.getX() + 0.55f, face.getBottom() - rad, 0.9f);
    g.setColour(whiteInk(pale ? 0.75f : 0.45f));
    g.fillEllipse(plate.getX() + 1.6f * s, plate.getY() + 1.2f * s, 2.6f * s, 1.3f * s);
    if (hover || drag)
    {
        g.setColour(juce::Colour(0xfff6df91).withAlpha(drag ? 0.80f : 0.50f));
        g.drawRoundedRectangle(face.reduced(0.4f), rad, 0.9f);
    }
}

void drawSatinPlate(juce::Graphics& g, juce::Rectangle<float> bounds, bool on, bool pressed,
                    bool hovered, bool withLamp)
{
    auto face = bounds.reduced(1.8f, 1.2f).withTrimmedBottom(1.8f);
    if (pressed)
        face = face.translated(0.0f, 1.0f);
    const float rad = 3.0f;
    if (pressed)
        softShadow(g, face, rad, 0.0f, 0.8f, 1.4f, 0.55f);
    else
        softShadow(g, face, rad, 0.0f, 2.3f, 2.6f, 0.75f);

    juce::Colour top = on ? juce::Colour(0xff4d3f27) : juce::Colour(0xff4d4a45);
    juce::Colour bottom = on ? juce::Colour(0xff271d10) : juce::Colour(0xff252320);
    if (pressed)
    {
        top = top.darker(0.35f);
        bottom = bottom.brighter(0.08f);
    }
    if (hovered)
    {
        top = top.brighter(0.16f);
        bottom = bottom.brighter(0.12f);
    }
    g.setGradientFill(juce::ColourGradient(top, face.getX(), face.getY(), bottom, face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, rad);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(face, rad);
        g.reduceClipRegion(clip);
        // Satin grain: fine horizontal brushing.
        for (float y = face.getY() + 1.0f; y < face.getBottom(); y += 2.0f)
        {
            g.setColour(whiteInk(0.028f));
            g.drawHorizontalLine((int) y, face.getX(), face.getRight());
        }
        if (on)
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffd9a640).withAlpha(0.0f), 0.0f, face.getY() + face.getHeight() * 0.35f,
                                                   juce::Colour(0xffd9a640).withAlpha(0.26f), 0.0f, face.getBottom(), false));
            g.fillRect(face);
        }
        g.setGradientFill(juce::ColourGradient(whiteInk(pressed ? 0.02f : 0.10f), face.getX(), face.getY(),
                                               juce::Colours::transparentWhite, face.getX(), face.getY() + face.getHeight() * 0.5f, false));
        g.fillRect(face);
    }
    g.setColour(whiteInk(pressed ? 0.06f : 0.26f));
    g.drawLine(face.getX() + rad, face.getY() + 1.0f, face.getRight() - rad, face.getY() + 1.0f, 1.0f);
    g.setColour(whiteInk(pressed ? 0.03f : 0.10f));
    g.drawLine(face.getX() + 1.0f, face.getY() + rad, face.getX() + 1.0f, face.getBottom() - rad, 1.0f);
    g.setColour(blackInk(0.40f));
    g.drawLine(face.getX() + rad, face.getBottom() - 1.0f, face.getRight() - rad, face.getBottom() - 1.0f, 1.0f);
    g.setColour(on ? juce::Colour(0xffd9a640).withAlpha(0.80f) : blackInk(0.9f));
    g.drawRoundedRectangle(face, rad, on ? 1.2f : 1.0f);

    if (withLamp)
    {
        const float lr = juce::jlimit(1.8f, 3.4f, face.getHeight() * 0.15f);
        drawJewelLamp(g, { face.getX() + 7.0f + lr, face.getCentreY() }, lr, amber, on);
    }
}

namespace
{
struct TabGeometry
{
    juce::Rectangle<float> face;
    float depth = 0.0f;
    float radius = 2.0f;
};

// Tabs travel down and to the right when latched or pressed, so the front and right faces shrink.
TabGeometry tabGeometry(juce::Rectangle<float> b, bool lit, bool pressed)
{
    const float m = juce::jlimit(1.2f, 2.6f, b.getWidth() * 0.065f);
    const float d = juce::jlimit(1.8f, 3.2f, b.getWidth() * 0.08f);
    const float travel = pressed ? d * 0.8f : (lit ? d * 0.45f : 0.0f);
    return { b.reduced(m).withTrimmedRight(d).withTrimmedBottom(d).translated(travel, travel), d - travel,
             juce::jlimit(1.6f, 3.2f, b.getWidth() * 0.07f) };
}
} // namespace

void drawTonePlate(juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, bool lit, bool pressed, bool hovered)
{
    const auto geo = tabGeometry(b, lit, pressed);
    const auto face = geo.face;
    const float depth = geo.depth;
    const float rad = geo.radius;
    const auto glow = glowFor(c);

    // Lamp light bleeding onto the black bezel; it fades to nothing at the bounds.
    if (lit)
    {
        juce::Graphics::ScopedSaveState state(g);
        const auto centre = b.getCentre();
        g.addTransform(juce::AffineTransform::scale(1.0f, b.getHeight() / b.getWidth(), centre.x, centre.y));
        drawLampGlow(g, centre, b.getWidth() * 0.5f, glow, 1.5f);
    }
    // Cast shadow (tight when the tab is pushed in).
    if (depth > 0.3f)
        softShadow(g, face, rad, depth * 0.6f, depth * 0.9f, depth * 1.1f, lit ? 0.6f : 0.85f);

    // Right and front faces.
    const auto side = lit ? c.darker(0.55f).interpolatedWith(glow, 0.22f) : c.darker(0.62f);
    if (depth > 0.2f)
    {
        juce::Path right;
        right.addQuadrilateral(face.getRight(), face.getY() + 1.0f, face.getRight() + depth, face.getY() + depth,
                               face.getRight() + depth, face.getBottom() + depth, face.getRight(), face.getBottom());
        g.setColour(side.darker(0.25f));
        g.fillPath(right);
        juce::Path front;
        front.addQuadrilateral(face.getX() + 1.0f, face.getBottom(), face.getRight(), face.getBottom(),
                               face.getRight() + depth, face.getBottom() + depth, face.getX() + depth, face.getBottom() + depth);
        g.setColour(side);
        g.fillPath(front);
    }

    // Moulded bevel ring.
    g.setGradientFill(juce::ColourGradient(c.brighter(lit ? 0.85f : 0.6f), face.getX(), face.getY(),
                                           c.darker(0.45f), face.getRight(), face.getBottom(), false));
    g.fillRoundedRectangle(face, rad);

    // Translucent diffuser window with the lamp behind it.
    const float bev = juce::jlimit(1.1f, 2.0f, b.getWidth() * 0.055f);
    const auto win = face.reduced(bev);
    const float wr = juce::jmax(0.8f, rad * 0.6f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(win, wr);
        g.reduceClipRegion(clip);
        const auto topC = lit ? c.brighter(0.68f) : c.brighter(0.02f);
        const auto botC = lit ? c.brighter(0.22f) : c.darker(0.42f);
        g.setGradientFill(juce::ColourGradient(topC, win.getX(), win.getY(), botC, win.getX(), win.getBottom(), false));
        g.fillRect(win);
        // The bulb behind the plastic.
        const juce::Point<float> bulb(win.getCentreX(), win.getBottom() - win.getHeight() * 0.22f);
        juce::ColourGradient lamp(lit ? juce::Colour(0xffffffe8).withAlpha(0.78f) : whiteInk(0.05f), bulb.x, bulb.y,
                                  juce::Colours::transparentWhite, bulb.x + win.getWidth() * 0.9f, bulb.y, true);
        g.setGradientFill(lamp);
        g.fillRect(win);
        // Shadowed upper-left inner wall of the bezel.
        g.setGradientFill(juce::ColourGradient(blackInk(lit ? 0.22f : 0.42f), win.getX(), win.getY(),
                                               juce::Colours::transparentBlack, win.getX(), win.getY() + 3.0f, false));
        g.fillRect(win);
        g.setGradientFill(juce::ColourGradient(blackInk(lit ? 0.14f : 0.28f), win.getX(), win.getY(),
                                               juce::Colours::transparentBlack, win.getX() + 2.2f, win.getY(), false));
        g.fillRect(win);
        // Gloss from the upper left.
        g.setGradientFill(juce::ColourGradient(whiteInk(lit ? 0.14f : 0.24f), win.getX(), win.getY(),
                                               whiteInk(0.0f), win.getRight(), win.getY() + win.getHeight() * 0.55f, false));
        g.fillRect(win);
        if (pressed)
        {
            g.setColour(blackInk(0.12f));
            g.fillRect(win);
        }
        else if (hovered)
        {
            g.setColour(whiteInk(0.09f));
            g.fillRect(win);
        }
        // Lamp slit.
        const juce::Rectangle<float> slit(win.getCentreX() - win.getWidth() * 0.2f, win.getBottom() - 3.4f, win.getWidth() * 0.4f, 1.6f);
        if (lit)
        {
            drawLampGlow(g, slit.getCentre(), win.getWidth() * 0.55f, juce::Colour(0xffffefb8), 0.7f);
            g.setColour(juce::Colour(0xfffff6d6));
        }
        else
            g.setColour(blackInk(0.36f));
        g.fillRoundedRectangle(slit, 0.8f);
        if (! lit)
        {
            g.setColour(whiteInk(0.14f));
            g.fillRect(slit.getX(), slit.getBottom(), slit.getWidth(), 0.8f);
        }
    }
    g.setColour(blackInk(0.35f));
    g.drawRoundedRectangle(win, wr, 0.8f);
    // Specular edge of the bevel and the dark far edges.
    g.setColour(whiteInk(lit ? 0.75f : 0.60f));
    g.drawLine(face.getX() + rad, face.getY() + 0.55f, face.getRight() - rad, face.getY() + 0.55f, 0.9f);
    g.setColour(whiteInk(0.28f));
    g.drawLine(face.getX() + 0.55f, face.getY() + rad, face.getX() + 0.55f, face.getBottom() - rad, 0.9f);
    g.setColour(blackInk(0.55f));
    g.drawLine(face.getX() + rad, face.getBottom() - 0.5f, face.getRight() - rad, face.getBottom() - 0.5f, 0.9f);
    g.drawLine(face.getRight() - 0.5f, face.getY() + rad, face.getRight() - 0.5f, face.getBottom() - rad, 0.9f);
}

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------
namespace
{
struct FaderGeom
{
    float s = 1.0f, faceW = 22.0f, faceH = 13.0f, ex = 2.6f, ey = 3.8f, r = 12.0f;
};

FaderGeom faderGeom(int sliderWidth)
{
    FaderGeom f;
    f.s = juce::jlimit(0.6f, 1.5f, (float) sliderWidth / 36.0f);
    f.faceW = 24.5f * f.s;
    f.faceH = 14.0f * f.s;
    f.ex = 2.8f * f.s;
    f.ey = 4.0f * f.s;
    f.r = std::round(12.0f * f.s);
    return f;
}

void paintFaderTrack(juce::Graphics& g, int W, int H, const FaderGeom& f)
{
    const float cx = (float) W * 0.5f;
    const float yA = f.r - f.ey * 0.5f, yB = (float) H - f.r - f.ey * 0.5f;
    const float th = juce::jmax(0.8f, 0.9f * f.s);
    // Engraved cream tick marks either side of the slot.
    for (int i = 0; i <= 10; ++i)
    {
        const float yy = yA + (yB - yA) * (float) i / 10.0f;
        const bool major = i % 5 == 0;
        const float a = major ? 0.88f : 0.50f;
        for (int side = -1; side <= 1; side += 2)
        {
            const float x0 = cx + (float) side * 5.4f * f.s;
            const float x1 = cx + (float) side * (major ? 12.4f : 9.2f) * f.s;
            g.setColour(blackInk(0.60f * a));
            g.drawLine(x0, yy + 1.0f, x1, yy + 1.0f, th);
            g.setColour(juce::Colour(0xffdcd3b4).withAlpha(a));
            g.drawLine(x0, yy, x1, yy, th);
        }
    }
    drawFaderSlot(g, cx, 0.6f * f.s, (float) H - 0.6f * f.s, f.s);
}

juce::Colour tabInkFor(juce::Colour base)
{
    return base.getPerceivedBrightness() < 0.56f ? juce::Colour(0xfff6f2e4) : juce::Colour(0xff1d1c18);
}

juce::String compactTabLegend(const juce::Button& button, juce::String text)
{
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
    return text;
}

void drawTabLegend(juce::Graphics& g, juce::Rectangle<float> bounds, bool lit, bool pressed,
                   juce::Colour base, const juce::String& text)
{
    const auto geo = tabGeometry(bounds, lit, pressed);
    const float bev = juce::jlimit(1.1f, 2.0f, bounds.getWidth() * 0.055f);
    auto area = geo.face.reduced(bev);
    area.removeFromBottom(4.2f);
    const auto font = displayFont(juce::jlimit(7.0f, 9.5f, bounds.getWidth() * 0.25f), juce::Font::bold);
    auto lines = juce::StringArray::fromLines(text);
    if (lines.size() == 1 && text.containsChar(' ')
        && (float) juce::GlyphArrangement::getStringWidthInt(font, text) > area.getWidth() * 1.2f)
    {
        const int split = text.lastIndexOfChar(' ');
        lines = juce::StringArray(text.substring(0, split), text.substring(split + 1));
    }
    const int count = juce::jmax(1, lines.size());
    // Lines sit close together as one block centred on the face, however tall the plate is.
    const float lineHeight = juce::jmin(area.getHeight() / (float) count, font.getHeight() * 1.45f);
    const float blockTop = area.getCentreY() - lineHeight * (float) count * 0.5f;
    for (int i = 0; i < lines.size(); ++i)
    {
        auto lineFont = font;
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt(font, lines[i]);
        if (tw > area.getWidth())
            lineFont = font.withHorizontalScale(juce::jmax(0.55f, area.getWidth() / tw));
        drawEngravedText(g, lines[i], { area.getX(), blockTop + (float) i * lineHeight, area.getWidth(), lineHeight },
                         lineFont, juce::Justification::centred, tabInkFor(base));
    }
}
} // namespace

LookAndFeel::LookAndFeel()
{
    ++liveLookAndFeels;
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
    setColour(juce::PopupMenu::headerTextColourId, gold);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, amberBright);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0f0e0c));
    setColour(juce::TextEditor::textColourId, ivory);
    setColour(juce::TextEditor::highlightColourId, amber.withAlpha(0.4f));
    setColour(juce::TextEditor::highlightedTextColourId, ivory);
    setColour(juce::TextEditor::outlineColourId, lineColour);
    setColour(juce::TextEditor::focusedOutlineColourId, amber);
    setColour(juce::CaretComponent::caretColourId, amberBright);
    setColour(juce::ScrollBar::thumbColourId, juce::Colour(0xff5b5548));
    setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff12110f));
    setColour(juce::ListBox::outlineColourId, lineColour);
    setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff17150f));
    setColour(juce::TooltipWindow::textColourId, ivory);
    setColour(juce::TooltipWindow::outlineColourId, gold);
}

LookAndFeel::~LookAndFeel()
{
    if (--liveLookAndFeels <= 0)
        PaintCache::get().clear();
}

int LookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
    if (slider.getSliderStyle() == juce::Slider::LinearVertical)
        return juce::jlimit(1, juce::jmax(1, (int) faderGeom(slider.getWidth()).r),
                            juce::jmax(1, slider.getHeight() / 2 - 1));
    return juce::LookAndFeel_V4::getSliderThumbRadius(slider);
}

void LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPos, float minPos, float maxPos,
                                   juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
        return;
    }
    const auto f = faderGeom(slider.getWidth());
    const float ps = physicalScaleOf(g);
    const bool enabled = slider.isEnabled();
    juce::Graphics::ScopedSaveState state(g);
    if (! enabled)
        g.setOpacity(0.45f);

    // Static layer: engraved scale, recessed slot and rail (one blit per fader).
    const juce::String legendTop = slider.getProperties()["faderLegendTop"].toString();
    const juce::String legendBottom = slider.getProperties()["faderLegendBottom"].toString();
    // JUCE hands over the slider region already trimmed by the thumb radius; the artwork spans
    // the whole component so the slot and scale extend beyond the cap's centre travel.
    const int fullW = slider.getWidth(), fullH = slider.getHeight();
    const auto track = PaintCache::get().fetch(kindTrack, fullW, fullH, 0, juce::roundToInt(f.s * 100.0f), ps,
                                               [&](juce::Graphics& ig) { paintFaderTrack(ig, fullW, fullH, f); });
    blit(g, track, 0.0f, 0.0f);
    if (legendTop.isNotEmpty() || legendBottom.isNotEmpty())
    {
        const float lx = (float) fullW * 0.5f + 13.0f * f.s;
        const auto font = displayFont(juce::jmax(5.5f, 6.5f * f.s), juce::Font::bold);
        if (legendTop.isNotEmpty())
            drawEngravedText(g, legendTop, { lx, f.r - f.ey * 0.5f - 4.0f, 9.0f * f.s, 8.0f },
                             font, juce::Justification::centredLeft, juce::Colour(0xffd8cfb0));
        if (legendBottom.isNotEmpty())
            drawEngravedText(g, legendBottom, { lx, (float) fullH - f.r - f.ey * 0.5f - 4.0f, 9.0f * f.s, 8.0f },
                             font, juce::Justification::centredLeft, juce::Colour(0xffd8cfb0));
    }

    // The cap: pre-rendered moulded block, blitted at the slider position.
    const bool dragging = enabled && slider.isMouseButtonDown();
    const bool hover = enabled && slider.isMouseOver();
    const auto capState = dragging ? CapState::dragging : (hover ? CapState::hover : CapState::idle);
    const auto colour = slider.findColour(juce::Slider::thumbColourId);
    const int margin = (int) std::ceil(12.0f * f.s);
    const int imgW = (int) std::ceil(f.faceW + 4.0f * f.s) + margin * 2;
    const int imgH = (int) std::ceil(f.faceH + 5.5f * f.s) + margin * 2;
    const juce::Point<float> faceCentre((float) margin + f.faceW * 0.5f, (float) margin + f.faceH * 0.5f);
    const auto cap = PaintCache::get().fetch(kindCap, imgW, imgH, colour.getARGB(),
                                             (juce::roundToInt(f.s * 100.0f) << 3) | (int) capState, ps,
                                             [&](juce::Graphics& ig) { drawFaderCap(ig, faceCentre, f.faceW, colour, capState); });
    const float cx = (float) fullW * 0.5f - f.ex * 0.5f;
    const float cy = sliderPos - f.ey * 0.5f;
    blit(g, cap, std::round(cx - faceCentre.x), std::round(cy - faceCentre.y));
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float sliderPosProportional, float rotaryStartAngle,
                                   float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = (float) juce::jmin(width, height) * 0.5f - 4.0f;
    const float cx = std::round((float) x + (float) width * 0.5f);
    const float cy = std::round((float) y + (float) height * 0.5f);
    const float angle = rotaryStartAngle
        + juce::jlimit(0.0f, 1.0f, sliderPosProportional) * (rotaryEndAngle - rotaryStartAngle);
    const bool hardware = (bool) slider.getProperties()["hardwareRotary"];
    const bool enabled = slider.isEnabled();
    const float knobRadius = hardware ? radius : radius * 0.8f;
    juce::Graphics::ScopedSaveState state(g);
    if (! enabled)
        g.setOpacity(0.5f);

    if (hardware)
    {
        // Engraved scale dots around the knob.
        for (int n = 0; n <= 10; ++n)
        {
            const float a = rotaryStartAngle + (float) n / 10.0f * (rotaryEndAngle - rotaryStartAngle);
            const float rr = radius + 2.4f;
            const float dot = n % 5 == 0 ? 1.0f : 0.72f;
            const float px = cx + std::sin(a) * rr, py = cy - std::cos(a) * rr;
            g.setColour(blackInk(0.6f));
            g.fillEllipse(px - dot + 0.5f, py - dot + 0.9f, dot * 2.0f, dot * 2.0f);
            g.setColour(juce::Colour(0xffe0d6b6).withAlpha(n % 5 == 0 ? 0.95f : 0.65f));
            g.fillEllipse(px - dot, py - dot, dot * 2.0f, dot * 2.0f);
        }
    }
    else
    {
        // Lit value ring: an engraved groove with an amber filament and a bright end point.
        const float ringR = radius;
        const float w = juce::jlimit(2.2f, 3.4f, radius * 0.11f);
        juce::Path groove, value;
        groove.addCentredArc(cx, cy, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        value.addCentredArc(cx, cy, ringR, ringR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(whiteInk(0.10f));
        g.strokePath(groove, juce::PathStrokeType(w + 1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                     juce::AffineTransform::translation(0.0f, 0.8f));
        g.setColour(blackInk(0.85f));
        g.strokePath(groove, juce::PathStrokeType(w + 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (enabled && sliderPosProportional > 0.001f)
        {
            g.setColour(amber.withAlpha(0.20f));
            g.strokePath(value, juce::PathStrokeType(w + 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(amber);
            g.strokePath(value, juce::PathStrokeType(w - 0.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(juce::Colour(0xffffe7a6).withAlpha(0.85f));
            g.strokePath(value, juce::PathStrokeType(juce::jmax(0.8f, w * 0.3f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const float ex = cx + std::sin(angle) * ringR, ey = cy - std::cos(angle) * ringR;
            drawLampGlow(g, { ex, ey }, w * 2.4f, amberBright, 0.9f);
            g.setColour(juce::Colour(0xfffff4cf));
            g.fillEllipse(ex - w * 0.45f, ey - w * 0.45f, w * 0.9f, w * 0.9f);
        }
    }
    drawKnob(g, { cx, cy }, knobRadius, angle);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour&, bool hovered, bool down)
{
    const int w = button.getWidth(), h = button.getHeight();
    if (w < 2 || h < 2)
        return;
    const float ps = physicalScaleOf(g);
    if ((bool) button.getProperties()["hardwareTab"])
    {
        const auto base = button.findColour(juce::TextButton::buttonColourId);
        const bool lit = button.getToggleState();
        const int bits = (lit ? 1 : 0) | (down ? 2 : 0) | (hovered && ! lit ? 4 : 0);
        const auto plate = PaintCache::get().fetch(kindTonePlate, w, h, base.getARGB(), bits, ps, [&](juce::Graphics& ig)
        {
            drawTonePlate(ig, juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h), base, lit, down, hovered && ! lit);
        });
        blit(g, plate, 0.0f, 0.0f);
        return;
    }
    const bool on = button.getToggleState();
    const int bits = (on ? 1 : 0) | (down ? 2 : 0) | (hovered ? 4 : 0);
    const auto plate = PaintCache::get().fetch(kindSatinPlate, w, h, 0, bits, ps, [&](juce::Graphics& ig)
    {
        drawSatinPlate(ig, juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h), on, down, hovered, false);
    });
    blit(g, plate, 0.0f, 0.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                 bool, bool shouldDrawButtonAsDown)
{
    if (button.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xfff6df91));
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.0f), 2.0f, 1.5f);
    }
    if ((bool) button.getProperties()["hardwareTab"])
    {
        const auto base = button.findColour(juce::TextButton::buttonColourId);
        const bool lit = button.getToggleState();
        drawTabLegend(g, button.getLocalBounds().toFloat(), lit, shouldDrawButtonAsDown, base,
                      compactTabLegend(button, button.getButtonText()));
        return;
    }
    auto font = displayFont(juce::jlimit(8.0f, 14.0f, (float) button.getHeight() * 0.43f), juce::Font::bold);
    font.setExtraKerningFactor(0.08f);
    drawEngravedText(g, button.getButtonText(),
                     button.getLocalBounds().toFloat().reduced(4.0f, 1.0f).translated(0.0f, shouldDrawButtonAsDown ? 1.0f : -0.5f),
                     font, juce::Justification::centred,
                     button.getToggleState() ? amberBright : ivory);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                               int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    drawGlassPanel(g, r, 2.5f, juce::Colour(0xff0a0d10), 0.6f);

    // Raised key at the right with an up/down chevron pair.
    const float keyW = juce::jlimit(14.0f, 24.0f, (float) height * 0.85f);
    const auto key = juce::Rectangle<float>(r.getRight() - keyW - 1.5f, r.getY() + 2.0f, keyW, r.getHeight() - 4.0f);
    drawBevelRect(g, key, 2.0f, juce::Colour(isButtonDown ? 0xff2a2824 : 0xff45423c), 1.0f, ! isButtonDown);
    const float ax = key.getCentreX(), ay = key.getCentreY() + (isButtonDown ? 0.6f : 0.0f);
    juce::Path chevrons;
    chevrons.startNewSubPath(ax - 3.2f, ay - 1.2f);
    chevrons.lineTo(ax, ay - 4.2f);
    chevrons.lineTo(ax + 3.2f, ay - 1.2f);
    chevrons.startNewSubPath(ax - 3.2f, ay + 1.2f);
    chevrons.lineTo(ax, ay + 4.2f);
    chevrons.lineTo(ax + 3.2f, ay + 1.2f);
    g.setColour(blackInk(0.6f));
    g.strokePath(chevrons, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0.0f, 1.0f));
    g.setColour(box.isEnabled() ? amber : lineColour);
    g.strokePath(chevrons, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (box.hasKeyboardFocus(false))
    {
        g.setColour(amber.withAlpha(0.20f));
        g.drawRoundedRectangle(r.expanded(0.8f), 3.2f, 2.0f);
        g.setColour(amber);
        g.drawRoundedRectangle(r, 2.5f, 1.2f);
    }
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    const float keyW = juce::jlimit(14.0f, 24.0f, (float) box.getHeight() * 0.85f);
    label.setBounds(7, 0, box.getWidth() - 12 - (int) keyW, box.getHeight());
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

// ---- Tooltips, bubbles, menus, editors, scroll bars -----------------------
namespace
{
juce::TextLayout tooltipLayout(const juce::String& text)
{
    juce::AttributedString s;
    s.setJustification(juce::Justification::centredLeft);
    const auto body = displayFont(12.0f, juce::Font::plain);
    const auto head = displayFont(12.0f, juce::Font::bold);
    const int colon = text.indexOf(": ");
    if (colon > 0 && colon < 44 && ! text.substring(0, colon).containsChar('.'))
    {
        s.append(text.substring(0, colon), head, gold);
        const auto value = text.substring(colon + 2);
        const auto trimmed = value.upToFirstOccurrenceOf(".", false, false).trim();
        const auto valueColour = trimmed == "ON" ? amberBright : (trimmed == "OFF" ? ivoryDim : ivory);
        s.append("  ", body, ivory);
        s.append(value, trimmed == "ON" || trimmed == "OFF" ? head : body, valueColour);
    }
    else
        s.append(text, body, ivory);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(s, 360.0f);
    return layout;
}

void fillBubblePlate(juce::Graphics& g, const juce::Path& plate, juce::Rectangle<float> bounds, float edgeAlpha)
{
    {
        juce::Graphics::ScopedSaveState state(g);
        g.setColour(blackInk(0.42f));
        g.fillPath(plate, juce::AffineTransform::translation(1.0f, 2.2f));
    }
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34302a), 0.0f, bounds.getY(),
                                           juce::Colour(0xff13110e), 0.0f, bounds.getBottom(), false));
    g.fillPath(plate);
    g.setColour(gold.withAlpha(edgeAlpha));
    g.strokePath(plate, juce::PathStrokeType(1.2f));
}
} // namespace

juce::Rectangle<int> LookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                                   juce::Rectangle<int> parentArea)
{
    const auto layout = tooltipLayout(tipText);
    const int w = (int) std::ceil(layout.getWidth()) + 20;
    const int h = (int) std::ceil(layout.getHeight()) + 14;
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 22,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 8,
                                w, h).constrainedWithin(parentArea);
}

void LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
    g.fillAll(juce::Colour(0xff0a0907));
    const auto plate = r.reduced(1.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff35312a), 0.0f, plate.getY(),
                                           juce::Colour(0xff15130f), 0.0f, plate.getBottom(), false));
    g.fillRoundedRectangle(plate, 3.0f);
    g.setColour(whiteInk(0.09f));
    g.drawLine(plate.getX() + 3.0f, plate.getY() + 1.6f, plate.getRight() - 3.0f, plate.getY() + 1.6f, 1.0f);
    g.setColour(gold.withAlpha(0.85f));
    g.drawRoundedRectangle(plate.reduced(0.5f), 3.0f, 1.2f);
    g.setColour(blackInk(0.6f));
    g.drawRoundedRectangle(plate.reduced(2.0f), 2.0f, 0.8f);
    tooltipLayout(text).draw(g, r.reduced(10.0f, 7.0f));
}

void LookAndFeel::drawBubble(juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>& tip,
                             const juce::Rectangle<float>& body)
{
    juce::Path plate;
    plate.addBubble(body, body.getUnion(juce::Rectangle<float>(tip.x - 1.0f, tip.y - 1.0f, 2.0f, 2.0f)), tip, 4.0f, 8.0f);
    fillBubblePlate(g, plate, body, 0.85f);
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
    g.fillAll(juce::Colour(0xff0b0a08));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2e2b26), 0.0f, 0.0f, juce::Colour(0xff171512), 0.0f, (float) height, false));
    g.fillRect(r.reduced(1.0f));
    g.setColour(whiteInk(0.07f));
    g.drawLine(2.0f, 1.5f, (float) width - 2.0f, 1.5f, 1.0f);
    g.setColour(gold.withAlpha(0.80f));
    g.drawRect(r, 1.0f);
    g.setColour(blackInk(0.6f));
    g.drawRect(r.reduced(1.0f), 1.0f);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return displayFont(13.5f, juce::Font::plain);
}

int LookAndFeel::getPopupMenuBorderSize()
{
    return 3;
}

void LookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                                    bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                                    const juce::String& text, const juce::String& shortcutKeyText,
                                    const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    if (isSeparator)
    {
        const auto r = area.reduced(8, 0).toFloat();
        const float y = std::round(r.getCentreY());
        g.setColour(blackInk(0.7f));
        g.drawLine(r.getX(), y, r.getRight(), y, 1.0f);
        g.setColour(gold.withAlpha(0.32f));
        g.drawLine(r.getX(), y + 1.0f, r.getRight(), y + 1.0f, 1.0f);
        return;
    }
    auto r = area.reduced(2, 1);
    const bool lit = isHighlighted && isActive;
    juce::Colour ink = textColourToUse != nullptr ? *textColourToUse : ivory;
    if (lit)
    {
        const auto bar = r.toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7d5d24), 0.0f, bar.getY(),
                                               juce::Colour(0xff473312), 0.0f, bar.getBottom(), false));
        g.fillRoundedRectangle(bar, 2.0f);
        g.setColour(amberBright.withAlpha(0.45f));
        g.drawLine(bar.getX() + 2.0f, bar.getY() + 0.8f, bar.getRight() - 2.0f, bar.getY() + 0.8f, 1.0f);
        g.setColour(blackInk(0.5f));
        g.drawRoundedRectangle(bar, 2.0f, 0.8f);
        ink = juce::Colour(0xfffff0c8);
    }
    else
        ink = ink.withMultipliedAlpha(isActive ? 1.0f : 0.42f);

    r.reduce(juce::jmin(5, area.getWidth() / 20), 0);
    auto font = getPopupMenuFont();
    const float maxFontHeight = (float) r.getHeight() / 1.3f;
    if (font.getHeight() > maxFontHeight)
        font.setHeight(maxFontHeight);
    g.setFont(font);

    auto gutter = r.removeFromLeft(juce::roundToInt(maxFontHeight)).toFloat();
    if (icon != nullptr)
    {
        icon->drawWithin(g, gutter, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
        r.removeFromLeft(juce::roundToInt(maxFontHeight * 0.5f));
    }
    else if (isTicked)
    {
        juce::Path tick;
        const auto t = gutter.reduced(gutter.getWidth() * 0.18f, gutter.getHeight() * 0.22f);
        tick.startNewSubPath(t.getX(), t.getCentreY());
        tick.lineTo(t.getX() + t.getWidth() * 0.38f, t.getBottom());
        tick.lineTo(t.getRight(), t.getY());
        g.setColour(blackInk(0.6f));
        g.strokePath(tick, juce::PathStrokeType(2.0f), juce::AffineTransform::translation(0.0f, 1.0f));
        g.setColour(lit ? juce::Colour(0xfffff0c8) : amberBright);
        g.strokePath(tick, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
        r.removeFromLeft(juce::roundToInt(maxFontHeight * 0.35f));

    if (hasSubMenu)
    {
        const float arrowH = 0.6f * font.getAscent();
        const float x = (float) r.removeFromRight((int) arrowH).getX();
        const float cy = (float) r.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath(x, cy - arrowH * 0.5f);
        arrow.lineTo(x + arrowH * 0.6f, cy);
        arrow.lineTo(x, cy + arrowH * 0.5f);
        g.setColour(ink);
        g.strokePath(arrow, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    r.removeFromRight(3);
    if (shortcutKeyText.isNotEmpty())
    {
        auto small = font;
        small.setHeight(font.getHeight() * 0.85f);
        g.setFont(small);
        g.setColour(ink.withMultipliedAlpha(0.6f));
        g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
        g.setFont(font);
    }
    if (lit)
    {
        g.setColour(blackInk(0.55f));
        g.drawFittedText(text, r.translated(1, 1), juce::Justification::centredLeft, 1);
    }
    g.setColour(ink);
    g.drawFittedText(text, r, juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                             const juce::String& sectionName)
{
    auto font = getPopupMenuFont().boldened();
    font.setHeight(font.getHeight() * 0.82f);
    font.setExtraKerningFactor(0.08f);
    const auto text = area.withTrimmedLeft(12).withTrimmedRight(8).withTrimmedBottom(3);
    drawEngravedText(g, sectionName.toUpperCase(), text.toFloat(), font, juce::Justification::bottomLeft, gold);
    etchLine(g, (float) area.getX() + 8.0f, (float) area.getBottom() - 2.0f, (float) area.getRight() - 8.0f,
             (float) area.getBottom() - 2.0f, 0.5f);
}

void LookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId).darker(0.4f));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(blackInk(0.75f), 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, juce::jmin(7.0f, r.getHeight() * 0.5f), false));
    g.fillRect(r);
    g.setGradientFill(juce::ColourGradient(blackInk(0.55f), 0.0f, 0.0f, juce::Colours::transparentBlack, 4.0f, 0.0f, false));
    g.fillRect(r);
    g.setColour(whiteInk(0.08f));
    g.drawLine(1.0f, (float) height - 0.5f, (float) width - 1.0f, (float) height - 0.5f, 1.0f);
}

void LookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    if (! editor.isEnabled())
        return;
    const auto r = juce::Rectangle<float>(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f);
    if (editor.hasKeyboardFocus(true) && ! editor.isReadOnly())
    {
        g.setColour(amber.withAlpha(0.18f));
        g.drawRect(r.expanded(0.5f), 2.0f);
        g.setColour(amber);
        g.drawRect(r, 1.2f);
    }
    else
    {
        g.setColour(blackInk(0.85f));
        g.drawRect(r, 1.0f);
        g.setColour(lineColour.withAlpha(0.7f));
        g.drawRect(r.reduced(1.0f), 1.0f);
    }
}

void LookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                bool vertical, int thumbStart, int thumbSize, bool isMouseOver, bool isMouseDown)
{
    const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const auto groove = vertical ? bounds.reduced(bounds.getWidth() * 0.34f, 1.0f) : bounds.reduced(1.0f, bounds.getHeight() * 0.34f);
    g.setColour(blackInk(0.5f));
    g.fillRoundedRectangle(groove, groove.getWidth() * 0.5f);
    if (thumbSize <= 0)
        return;
    auto thumb = vertical ? juce::Rectangle<float>((float) x + 1.5f, (float) (y + thumbStart), (float) width - 3.0f, (float) thumbSize)
                          : juce::Rectangle<float>((float) (x + thumbStart), (float) y + 1.5f, (float) thumbSize, (float) height - 3.0f);
    thumb = thumb.reduced(0.0f);
    const auto c = isMouseDown ? juce::Colour(0xff8c8372) : (isMouseOver ? juce::Colour(0xff7a7262) : juce::Colour(0xff625c4e));
    g.setGradientFill(juce::ColourGradient(c.brighter(0.25f), thumb.getX(), thumb.getY(),
                                           c.darker(0.55f), vertical ? thumb.getRight() : thumb.getX(), vertical ? thumb.getY() : thumb.getBottom(), false));
    g.fillRoundedRectangle(thumb, 2.5f);
    g.setColour(whiteInk(0.22f));
    if (vertical)
        g.drawLine(thumb.getX() + 1.0f, thumb.getY() + 2.5f, thumb.getX() + 1.0f, thumb.getBottom() - 2.5f, 1.0f);
    else
        g.drawLine(thumb.getX() + 2.5f, thumb.getY() + 1.0f, thumb.getRight() - 2.5f, thumb.getY() + 1.0f, 1.0f);
    g.setColour(blackInk(0.7f));
    g.drawRoundedRectangle(thumb, 2.5f, 0.8f);
}

// ---------------------------------------------------------------------------
// Drag read-out bubble
// ---------------------------------------------------------------------------
namespace
{
// A small brass-edged plate with the parameter name and a lit value window. It is parented to
// the editor while a fader or knob is dragged and never takes mouse, focus or accessibility.
class DragBubble final : public juce::Component
{
public:
    DragBubble()
    {
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
        setAccessible(false);
        setOpaque(false);
    }

    void restart() { widest = 0; }

    void setReadout(const juce::String& newName, const juce::String& newValue, float scale)
    {
        name = newName.toUpperCase();
        value = newValue;
        s = scale;
        nameFont = displayFont(8.6f * s, juce::Font::bold);
        nameFont.setExtraKerningFactor(0.07f);
        valueFont = displayFont(12.2f * s, juce::Font::bold);
        const float nameW = (float) juce::GlyphArrangement::getStringWidthInt(nameFont, name);
        const float valueW = (float) juce::GlyphArrangement::getStringWidthInt(valueFont, value);
        pillW = juce::jmax(valueW + 13.0f * s, 40.0f * s);
        pillH = std::ceil(valueFont.getHeight() * 1.12f);
        const float bodyW = 8.0f * s + nameW + 7.0f * s + pillW + 5.0f * s;
        bodyH = pillH + 8.0f * s;
        tipLen = 6.0f * s;
        widest = juce::jmax(widest, (int) std::ceil(bodyW));
        const int w = widest + 5;
        const int h = (int) std::ceil(bodyH + tipLen) + 4;
        if (w != getWidth() || h != getHeight())
            setSize(w, h);
        repaint();
    }

    void pointAt(juce::Rectangle<int> target)
    {
        auto* parent = getParentComponent();
        if (parent == nullptr)
            return;
        const int bw = getWidth(), bh = getHeight();
        const int x = juce::jlimit(2, juce::jmax(2, parent->getWidth() - bw - 2), target.getCentreX() - bw / 2);
        pointDown = target.getY() - bh - 2 >= 2;
        const int y = pointDown ? target.getY() - bh - 2 : target.getBottom() + 2;
        tipX = juce::jlimit(10.0f * s, (float) bw - 14.0f * s, (float) (target.getCentreX() - x));
        setTopLeftPosition(x, juce::jlimit(2, juce::jmax(2, parent->getHeight() - bh - 2), y));
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const float w = (float) getWidth() - 4.0f;
        const juce::Rectangle<float> body(1.0f, pointDown ? 1.0f : 1.0f + tipLen, w, bodyH);
        const juce::Point<float> tip(tipX, pointDown ? body.getBottom() + tipLen : body.getY() - tipLen);
        juce::Path plate;
        plate.addBubble(body, getLocalBounds().toFloat().withTrimmedRight(3.0f).withTrimmedBottom(3.0f), tip,
                        3.5f * s, 9.0f * s);
        fillBubblePlate(g, plate, body, 0.95f);
        g.setColour(whiteInk(0.10f));
        g.drawLine(body.getX() + 4.0f, body.getY() + 1.8f, body.getRight() - 4.0f, body.getY() + 1.8f, 1.0f);

        auto inner = body.reduced(8.0f * s, 4.0f * s);
        auto pill = inner.removeFromRight(pillW).withSizeKeepingCentre(pillW, pillH);
        drawEngravedText(g, name, inner.withTrimmedRight(7.0f * s), nameFont, juce::Justification::centredLeft,
                         juce::Colour(0xffe3d7ae));
        drawGlassPanel(g, pill, 2.5f * s, juce::Colour(0xff0b0a07), 0.5f);
        g.setColour(amber.withAlpha(0.10f));
        g.fillRoundedRectangle(pill.reduced(1.0f), 2.0f * s);
        g.setFont(valueFont);
        g.setColour(amberBright.withAlpha(0.35f));
        g.drawFittedText(value, pill.toNearestInt().translated(0, 0), juce::Justification::centred, 1, 0.7f);
        g.setColour(amberBright);
        g.drawFittedText(value, pill.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }

private:
    juce::String name, value;
    juce::Font nameFont { juce::FontOptions() }, valueFont { juce::FontOptions() };
    float s = 1.0f, pillW = 40.0f, pillH = 16.0f, bodyH = 24.0f, tipLen = 6.0f, tipX = 20.0f;
    int widest = 0;
    bool pointDown = true;
};
} // namespace

FocusSlider::~FocusSlider() = default;

void FocusSlider::showDragBubble()
{
    auto* top = getTopLevelComponent();
    if (top == nullptr || top == this)
        return;
    if (dragBubble == nullptr)
        dragBubble = std::make_unique<DragBubble>();
    if (dragBubble->getParentComponent() != top)
        top->addChildComponent(dragBubble.get());
    static_cast<DragBubble*>(dragBubble.get())->restart();
    dragBubble->setVisible(true);
    dragBubble->toFront(false);
    updateDragBubble();
}

void FocusSlider::hideDragBubble()
{
    if (dragBubble != nullptr)
        dragBubble->setVisible(false);
}

void FocusSlider::updateDragBubble()
{
    auto* bubble = static_cast<DragBubble*>(dragBubble.get());
    if (bubble == nullptr || ! bubble->isVisible())
        return;
    auto* top = bubble->getParentComponent();
    if (top == nullptr)
        return;
    const double v = getValue();
    bubble->setReadout(bubbleTitleText ? bubbleTitleText() : getTitle(),
                       bubbleValueText ? bubbleValueText(v) : getTextFromValue(v),
                       juce::jlimit(0.75f, 1.4f, (float) top->getWidth() / 1600.0f));
    juce::Rectangle<int> target;
    if (getSliderStyle() == juce::Slider::LinearVertical)
    {
        const auto f = faderGeom(getWidth());
        const float pos = getPositionOfValue(v);
        target = top->getLocalArea(this, juce::Rectangle<float>((float) getWidth() * 0.5f - f.ex * 0.5f - f.faceW * 0.5f,
                                                                pos - f.ey * 0.5f - f.faceH * 0.5f, f.faceW, f.faceH).toNearestInt());
    }
    else
        target = top->getLocalArea(this, getLocalBounds().reduced(2));
    bubble->pointAt(target);
}

// ---------------------------------------------------------------------------
// ParamKnob
// ---------------------------------------------------------------------------
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

// Name shown in the drag bubble: the short parameter name, with its section or channel when the
// bare name (A, D, R, LEVEL...) would be ambiguous.
juce::String bubbleNameFor(int index, const juce::String& panelName)
{
    const auto& def = cs80::paramDef(index);
    juce::String name = def.name;
    if (name.length() <= 3 || name == "LEVEL")
        name = juce::String(def.group) + " " + name;
    if (index >= cs80::P_numGlobals && index < cs80::P_legacyNumParams)
        name = juce::String(index < cs80::P_ch2Start ? "I " : "II ") + name;
    juce::ignoreUnused(panelName);
    return name;
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
    slider.bubbleTitleText = [this] { return bubbleNameFor(parameterIndex, name); };
    slider.bubbleValueText = [this](double v) { return formatParamValue(parameterIndex, (float) v); };
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
        auto nameFont = displayFont(juce::jmax(7.0f, 9.0f * scale), juce::Font::bold);
        nameFont.setExtraKerningFactor(0.02f);
        // A small gutter keeps neighbouring legends from running into each other when one is squeezed.
        drawEngravedText(g, name, { 1.5f, 0.0f, (float) getWidth() - 3.0f, 24.0f * scale }, nameFont,
                         juce::Justification::centred, juce::Colour(0xffe6dcbc));
        const float vh = 16.0f * scale;
        drawEngravedText(g, formatParamValue(parameterIndex, (float) slider.getValue()),
                         { 0.0f, (float) getHeight() - vh, (float) getWidth(), vh },
                         displayFont(juce::jmax(7.0f, 7.5f * scale)), juce::Justification::centred, juce::Colour(0xffcfc49c));
        return;
    }

    const float h = (float) getHeight();
    const float nameHeight = juce::jlimit(10.0f, 15.0f, h * 0.20f);
    const float valueHeight = juce::jlimit(9.0f, 14.0f, h * 0.18f);

    auto nameFont = displayFont(
                               juce::jlimit(7.5f, 11.5f, h * 0.155f), juce::Font::bold);
    nameFont.setExtraKerningFactor(0.055f);
    drawEngravedText(g, name, { 0.0f, 0.0f, (float) getWidth(), nameHeight + 2.0f }, nameFont,
                     juce::Justification::centred, ivory.withAlpha(0.95f));

    // Value in a small lit window, like a segment readout.
    const auto valueFont = displayFont(juce::jlimit(7.0f, 11.0f, h * 0.15f), juce::Font::plain);
    const auto valueText = formatParamValue(parameterIndex, (float) slider.getValue());
    const float tw = (float) juce::GlyphArrangement::getStringWidthInt(valueFont, valueText);
    const float pw = juce::jmin((float) getWidth() - 2.0f, tw + 12.0f);
    const juce::Rectangle<float> pill(((float) getWidth() - pw) * 0.5f, (float) getHeight() - valueHeight - 1.0f,
                                      pw, valueHeight + 0.5f);
    drawGlassPanel(g, pill, 3.0f, juce::Colour(0xff0b0a08), 0.45f);
    g.setFont(valueFont);
    g.setColour(amber.withAlpha(0.95f));
    g.drawFittedText(valueText, pill.toNearestInt(), juce::Justification::centred, 1, 0.65f);
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

// ---------------------------------------------------------------------------
// LedButton
// ---------------------------------------------------------------------------
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
    const bool on = getToggleState();
    const float ps = physicalScaleOf(g);
    if (hardwareTab)
    {
        if (accent.getPerceivedBrightness() < 0.2f)
        {
            // Black rocker with its printed legend, as on the original oscillator row.
            const auto r = getLocalBounds().toFloat().reduced(getWidth() * 0.07f, 1.0f);
            drawRocker(g, r, on || down, hovered);
            const auto upper = r.withHeight(r.getHeight() * 0.5f);
            const float cx = upper.getCentreX(), cy = upper.getCentreY() + juce::jmax(1.5f, r.getHeight() * 0.05f);
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
            g.setColour(blackInk(0.7f));
            g.strokePath(glyph, juce::PathStrokeType(1.4f), juce::AffineTransform::translation(0.6f, 0.9f));
            g.setColour(on ? juce::Colour(0xfff6efd6) : juce::Colour(0xff8b8574));
            g.strokePath(glyph, juce::PathStrokeType(1.3f));
            auto font = displayFont(juce::jlimit(6.0f, 8.5f, r.getWidth() * 0.22f), juce::Font::bold);
            drawEngravedText(g, getButtonText(), r.withTrimmedTop(r.getHeight() * 0.56f).withTrimmedBottom(2.0f), font,
                             juce::Justification::centred, on ? juce::Colour(0xffc8bf9b) : juce::Colour(0xff7b7566));
            return;
        }
        const int w = getWidth(), h = getHeight();
        const bool lit = on;
        const int bits = (lit ? 1 : 0) | (down ? 2 : 0) | (hovered && ! lit ? 4 : 0);
        const auto plate = PaintCache::get().fetch(kindTonePlate, w, h, accent.getARGB(), bits, ps, [&](juce::Graphics& ig)
        {
            drawTonePlate(ig, juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h), accent, lit, down, hovered && ! lit);
        });
        blit(g, plate, 0.0f, 0.0f);
        drawTabLegend(g, getLocalBounds().toFloat(), lit, down, accent, getButtonText());
        return;
    }

    // Software key: raised satin plate with an amber jewel lamp.
    const int w = getWidth(), h = getHeight();
    const int bits = (on ? 1 : 0) | (down ? 2 : 0) | (hovered ? 4 : 0);
    const auto plate = PaintCache::get().fetch(kindSatinPlate, w, h, 1, bits, ps, [&](juce::Graphics& ig)
    {
        drawSatinPlate(ig, juce::Rectangle<float>(0.0f, 0.0f, (float) w, (float) h), on, down, hovered, true);
    });
    blit(g, plate, 0.0f, 0.0f);

    const auto face = getLocalBounds().toFloat().reduced(1.8f, 1.2f).withTrimmedBottom(1.8f);
    const float lr = juce::jlimit(1.8f, 3.4f, face.getHeight() * 0.15f);
    auto textArea = face.withTrimmedLeft(7.0f + lr * 2.6f + 4.0f).withTrimmedRight(3.0f)
                        .translated(0.0f, down ? 1.0f : 0.0f);
    auto font = displayFont(juce::jlimit(7.5f, 11.5f, face.getHeight() * 0.38f), juce::Font::bold);
    font.setExtraKerningFactor(0.03f);
    drawEngravedText(g, getButtonText(), textArea, font, juce::Justification::centredLeft,
                     on ? amberBright : ivory.withAlpha(0.9f));
}

// ---------------------------------------------------------------------------
// RibbonStrip
// ---------------------------------------------------------------------------
namespace
{
juce::Rectangle<float> ribbonChannel(float W, float H)
{
    return juce::Rectangle<float>(0.0f, 0.0f, W, H).reduced(0.5f).reduced(1.9f);
}

juce::Rectangle<float> ribbonBand(juce::Rectangle<float> channel)
{
    return channel.reduced(9.0f, channel.getHeight() * 0.27f);
}

void paintRibbonBase(juce::Graphics& g, int W, int H)
{
    const juce::Rectangle<float> frame = juce::Rectangle<float>(0.0f, 0.0f, (float) W, (float) H).reduced(0.5f);
    // Brass edge: lit on top, shaded below, with a bevel.
    juce::ColourGradient brass(juce::Colour(0xfff2dfa0), 0.0f, frame.getY(), juce::Colour(0xff8f7433), 0.0f, frame.getBottom(), false);
    brass.addColour(0.30, juce::Colour(0xffc7a95a));
    brass.addColour(0.62, juce::Colour(0xff6b5320));
    g.setGradientFill(brass);
    g.fillRoundedRectangle(frame, 4.0f);
    g.setColour(blackInk(0.75f));
    g.drawRoundedRectangle(frame, 4.0f, 1.0f);
    g.setColour(whiteInk(0.35f));
    g.drawLine(frame.getX() + 4.0f, frame.getY() + 1.0f, frame.getRight() - 4.0f, frame.getY() + 1.0f, 1.0f);

    const auto channel = ribbonChannel((float) W, (float) H);
    drawWell(g, channel, 2.5f);
    const auto band = ribbonBand(channel);

    // Printed scale on the channel floor above and below the ribbon.
    const float tickLen = juce::jmax(1.5f, (band.getY() - channel.getY()) - 2.2f);
    for (int i = 0; i <= 40; ++i)
    {
        const float x = band.getX() + band.getWidth() * (float) i / 40.0f;
        const bool major = i % 10 == 0;
        const bool centre = i == 20;
        const float len = centre ? tickLen + 1.0f : (major ? tickLen : tickLen * 0.55f);
        g.setColour(centre ? gold.withAlpha(0.95f) : juce::Colour(0xffd8cfb0).withAlpha(major ? 0.55f : 0.30f));
        g.drawLine(x, channel.getY() + 1.8f, x, channel.getY() + 1.8f + len, centre ? 1.4f : 0.8f);
        g.drawLine(x, channel.getBottom() - 1.8f - len, x, channel.getBottom() - 1.8f, centre ? 1.4f : 0.8f);
    }

    // The ribbon: glossy black, a thin lit crown along its upper edge.
    g.setColour(blackInk(0.7f));
    g.fillRoundedRectangle(band.expanded(0.8f).translated(0.0f, 0.8f), 2.4f);
    juce::ColourGradient strip(juce::Colour(0xff55534d), 0.0f, band.getY(), juce::Colour(0xff060605), 0.0f, band.getBottom(), false);
    strip.addColour(0.22, juce::Colour(0xff33322e));
    strip.addColour(0.50, juce::Colour(0xff121110));
    g.setGradientFill(strip);
    g.fillRoundedRectangle(band, 2.0f);
    g.setColour(whiteInk(0.42f));
    g.drawLine(band.getX() + 2.0f, band.getY() + 0.9f, band.getRight() - 2.0f, band.getY() + 0.9f, 1.0f);
    g.setColour(whiteInk(0.10f));
    g.drawLine(band.getX() + 2.0f, band.getBottom() - 0.8f, band.getRight() - 2.0f, band.getBottom() - 0.8f, 1.0f);
    g.setColour(whiteInk(0.035f));
    for (float f : { 0.38f, 0.62f })
        g.drawLine(band.getX() + 3.0f, band.getY() + band.getHeight() * f, band.getRight() - 3.0f, band.getY() + band.getHeight() * f, 0.8f);
}
} // namespace

RibbonStrip::RibbonStrip()
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setRepaintsOnMouseActivity(true);

}

void RibbonStrip::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    if (W < 4 || H < 4)
        return;
    const auto base = PaintCache::get().fetch(kindRibbon, W, H, 0, 0, physicalScaleOf(g),
                                              [&](juce::Graphics& ig) { paintRibbonBase(ig, W, H); });
    blit(g, base, 0.0f, 0.0f);

    const auto channel = ribbonChannel((float) W, (float) H);
    const auto band = ribbonBand(channel);
    const float cy = band.getCentreY();
    const bool hovering = ! touching && hoverX >= 0.0f;
    if (! touching && ! hovering)
        return;

    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(channel, 2.5f);
    g.reduceClipRegion(clip);

    const float posX = touching ? juce::jlimit(band.getX(), band.getRight(), anchorX + value * (float) getWidth()) : hoverX;
    if (touching)
    {
        // Displacement from the point of first contact, lit on the ribbon.
        const float x0 = juce::jmin(anchorX, posX), x1 = juce::jmax(anchorX, posX);
        g.setColour(amber.withAlpha(0.22f));
        g.fillRect(x0, band.getY(), x1 - x0, band.getHeight());
        g.setColour(amber.withAlpha(0.55f));
        g.drawLine(anchorX, channel.getY() + 1.0f, anchorX, channel.getBottom() - 1.0f, 1.0f);
    }
    drawLampGlow(g, { posX, cy }, juce::jmax(10.0f, (float) H * 1.1f), touching ? amberBright : juce::Colour(0xffd8c98a),
                 touching ? 1.15f : 0.35f);
    // Contact line across the ribbon.
    juce::ColourGradient contact(juce::Colour(0xfffff6d8).withAlpha(touching ? 0.95f : 0.4f), posX, cy,
                                 amber.withAlpha(0.0f), posX + 3.0f, cy, true);
    g.setGradientFill(contact);
    g.fillRect(posX - 3.0f, band.getY(), 6.0f, band.getHeight());
    // Brass position markers on both edges of the channel.
    const float mk = juce::jlimit(3.0f, 5.0f, (float) H * 0.22f);
    for (int top = 0; top < 2; ++top)
    {
        juce::Path tri;
        if (top == 1)
            tri.addTriangle(posX - mk, channel.getY(), posX + mk, channel.getY(), posX, channel.getY() + mk);
        else
            tri.addTriangle(posX - mk, channel.getBottom(), posX + mk, channel.getBottom(), posX, channel.getBottom() - mk);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2dfa0), posX - mk, channel.getY(),
                                               juce::Colour(0xff8f7433), posX + mk, channel.getBottom(), false));
        g.fillPath(tri);
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
void RibbonStrip::mouseMove(const juce::MouseEvent& e)
{
    hoverX = e.position.x;
    repaint();
}
void RibbonStrip::mouseExit(const juce::MouseEvent&)
{
    hoverX = -1.0f;
    repaint();
}

// ---------------------------------------------------------------------------
// PerformanceWheel
// ---------------------------------------------------------------------------
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

juce::Rectangle<float> PerformanceWheel::trackArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromTop(12.0f);
    r.removeFromBottom(juce::jlimit(6.0f, 9.0f, (float) getHeight() * 0.07f));
    return r.reduced(juce::jlimit(3.0f, 6.0f, (float) getWidth() * 0.18f), 2.0f);
}

namespace
{
constexpr float kWheelSweep = 1.2f; // radians either side of centre

void paintWheelHousing(juce::Graphics& g, juce::Rectangle<float> track, bool spring)
{
    drawWell(g, track, 3.5f);
    const auto win = track.reduced(1.8f);
    const float cy = win.getCentreY(), radius = win.getHeight() * 0.5f;
    // Position scale on the bezel beside the wheel; a gold detent pair marks the spring centre.
    for (int i = 0; i <= 4; ++i)
    {
        const float n = (float) i / 4.0f;
        const float y = cy - radius * std::sin((n - 0.5f) * 2.0f * kWheelSweep);
        const bool centre = i == 2;
        if (centre && spring)
        {
            for (int side = -1; side <= 1; side += 2)
            {
                const float x = side < 0 ? track.getX() - 0.8f : track.getRight() + 0.8f;
                juce::Path inward;
                inward.addTriangle(x - (float) side * 0.2f, y - 2.4f, x - (float) side * 0.2f, y + 2.4f, x + (float) side * 3.2f, y);
                g.setColour(blackInk(0.6f));
                g.fillPath(inward, juce::AffineTransform::translation(0.0f, 0.9f));
                g.setColour(gold);
                g.fillPath(inward);
            }
        }
        else
        {
            g.setColour(juce::Colour(0xffd8cfb0).withAlpha(0.45f));
            g.drawLine(track.getX() - 3.0f, y, track.getX() - 0.8f, y, 0.9f);
            g.drawLine(track.getRight() + 0.8f, y, track.getRight() + 3.0f, y, 0.9f);
        }
    }
}
} // namespace

void PerformanceWheel::paint(juce::Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    const auto title = full.removeFromTop(12.0f);
    drawEngravedText(g, wheelLabel, title,
                     displayFont(juce::jlimit(6.5f, 9.0f, (float) getHeight() * 0.07f), juce::Font::bold),
                     juce::Justification::centred, juce::Colour(0xffe0d6b6));

    const auto track = trackArea();
    if (track.getWidth() < 4.0f || track.getHeight() < 8.0f)
        return;
    const int W = getWidth(), H = getHeight();
    const auto housing = PaintCache::get().fetch(kindWheel, W, H, 0, spring ? 1 : 0, physicalScaleOf(g),
                                                 [&](juce::Graphics& ig) { paintWheelHousing(ig, trackArea(), spring); });
    blit(g, housing, 0.0f, 0.0f);

    // The wheel itself, seen edge-on: a ribbed cylinder turning about its horizontal axis.
    const auto win = track.reduced(1.8f);
    const float n = spring ? (value * 0.5f + 0.5f) : value;
    const float rot = (n - 0.5f) * 2.0f * kWheelSweep;
    const float radius = win.getHeight() * 0.5f, cy = win.getCentreY();
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(win, 2.4f);
        g.reduceClipRegion(clip);
        juce::ColourGradient cylinder(juce::Colour(0xff050505), 0.0f, win.getY(), juce::Colour(0xff040404), 0.0f, win.getBottom(), false);
        cylinder.addColour(0.10, juce::Colour(0xff26251f));
        cylinder.addColour(0.30, juce::Colour(0xff79766c));
        cylinder.addColour(0.42, juce::Colour(0xff9c988b));
        cylinder.addColour(0.60, juce::Colour(0xff4b4944));
        cylinder.addColour(0.85, juce::Colour(0xff161513));
        g.setGradientFill(cylinder);
        g.fillRect(win);
        g.setGradientFill(juce::ColourGradient(whiteInk(0.16f), win.getX(), 0.0f, blackInk(0.40f), win.getRight(), 0.0f, false));
        g.fillRect(win);

        const float pitch = 0.2094f; // 12 degrees between ribs
        const int first = (int) std::floor((-kPi * 0.5f - rot) / pitch);
        const int last = (int) std::ceil((kPi * 0.5f - rot) / pitch);
        for (int k = first; k <= last; ++k)
        {
            const float theta = (float) k * pitch + rot;
            const float c = std::cos(theta);
            if (c < 0.08f)
                continue;
            const float y = cy - radius * std::sin(theta);
            if (k == 0)
            {
                // The index rib, in pink, shows where the wheel sits relative to the detent.
                g.setColour(blackInk(0.55f));
                g.fillRect(win.getX() + 0.5f, y - 1.6f * c - 0.6f, win.getWidth() - 1.0f, 3.2f * c + 1.2f);
                g.setColour(capPink.withMultipliedBrightness(0.55f + 0.45f * c));
                g.fillRect(win.getX() + 0.5f, y - 1.2f * c, win.getWidth() - 1.0f, 2.4f * c);
                g.setColour(whiteInk(0.45f * c));
                g.fillRect(win.getX() + 0.5f, y - 1.2f * c, win.getWidth() - 1.0f, 0.8f);
                continue;
            }
            g.setColour(blackInk(0.60f * (0.35f + 0.65f * c)));
            g.fillRect(win.getX(), y - 0.6f * c - 0.2f, win.getWidth(), 1.2f * c + 0.4f);
            g.setColour(whiteInk(0.16f * c));
            g.fillRect(win.getX(), y + 0.6f * c + 0.2f, win.getWidth(), 0.8f);
        }
        // Housing lips shade the ends of the wheel.
        g.setGradientFill(juce::ColourGradient(blackInk(0.75f), 0.0f, win.getY(), juce::Colours::transparentBlack, 0.0f, win.getY() + 7.0f, false));
        g.fillRect(win);
        g.setGradientFill(juce::ColourGradient(blackInk(0.75f), 0.0f, win.getBottom(), juce::Colours::transparentBlack, 0.0f, win.getBottom() - 7.0f, false));
        g.fillRect(win);
    }
    g.setColour(blackInk(0.7f));
    g.drawRoundedRectangle(win, 2.4f, 0.9f);

    // Spring or latch indicator under the wheel.
    const float rowTop = track.getBottom() + 2.0f;
    const float ix = (float) getWidth() * 0.5f, iy = rowTop + ((float) getHeight() - rowTop) * 0.5f;
    if (spring)
    {
        const bool engaged = std::abs(value) > 0.02f;
        juce::Path zig;
        const float hw = juce::jmin(5.0f, (float) getWidth() * 0.18f), zh = 2.0f;
        zig.startNewSubPath(ix - hw, iy);
        for (int i = 0; i < 5; ++i)
            zig.lineTo(ix - hw + hw * 2.0f * ((float) i + 0.5f) / 5.0f, iy + (i % 2 == 0 ? -zh : zh));
        zig.lineTo(ix + hw, iy);
        g.setColour(blackInk(0.6f));
        g.strokePath(zig, juce::PathStrokeType(1.2f), juce::AffineTransform::translation(0.0f, 0.9f));
        g.setColour(engaged ? amberBright : gold.withAlpha(0.7f));
        g.strokePath(zig, juce::PathStrokeType(1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
        drawJewelLamp(g, { ix, iy }, juce::jlimit(1.3f, 2.0f, (float) getWidth() * 0.06f), amber, value > 0.01f);
}

void PerformanceWheel::setFromY(float y)
{
    const auto track = trackArea();
    const auto win = track.reduced(1.8f);
    const float radius = win.getHeight() * 0.5f;
    // Invert the screen position of the index rib: y = cy - R sin(theta), theta = (n - 0.5) * 2 * sweep.
    const float s = juce::jlimit(-1.0f, 1.0f, (win.getCentreY() - y) / juce::jmax(1.0f, radius));
    const float theta = juce::jlimit(-kWheelSweep, kWheelSweep, std::asin(s));
    const float n = juce::jlimit(0.0f, 1.0f, theta / (2.0f * kWheelSweep) + 0.5f);
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

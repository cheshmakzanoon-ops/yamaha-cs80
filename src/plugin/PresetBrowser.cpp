#include "PresetBrowser.h"

#include "PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>

namespace cs80::ui
{

// ===========================================================================
// Patch signature: a drawable summary computed from parameter values only.
// Factory presets, user files and the live patch all produce comparable
// pictures. It indicates character; it is not a model of the circuit.
// ===========================================================================
struct PresetSignature
{
    static constexpr int kEnvPoints = 56;
    static constexpr int kRadarAxes = 6;

    struct Channel
    {
        float attack = 0.0f, decay = 0.0f, sustain = 0.0f, release = 0.0f;
        float gain = 0.0f;       // loudness relative to the louder channel, 0.08 .. 1
        float hpf = 20.0f, lpf = 18000.0f, hpfRes = 0.0f, lpfRes = 0.0f;
        bool hpfOn = true, lpfOn = true;
        float saw = 0.0f, pulse = 0.0f, sine = 0.0f, noise = 0.0f;
        std::array<float, kEnvPoints> env {};   // amplitude on the shared time axis
    };

    std::array<Channel, 2> ch;
    // Bright, Resonant, Swell, Hold, Motion, Width; each 0..1.
    std::array<float, kRadarAxes> radar {};
    float cutoff = 0.0f, resonance = 0.0f, attack = 0.0f, release = 0.0f;
    float sustain = 0.0f, decay = 0.0f, chorus = 0.0f;
    int dominant = 0;
    std::uint32_t styles = 0;
};

namespace
{
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

// ---------------------------------------------------------------------------
// Style tags. Each is a fixed threshold on a number the picture also shows, so
// the filter and the signature can never disagree. Thresholds sit near the
// quartiles of the factory bank so every chip selects a useful slice.
//   SWELL       gain-weighted amp attack >= 0.45 s
//   PERCUSSIVE  attack <= 30 ms
//   BRIGHT      gain-weighted low-pass cutoff >= 4.3 kHz
//   WARM        cutoff <= 1.9 kHz
//   RESONANT    low-pass or panel resonance >= 0.45
//   MOVING      strongest of LFO, PWM, ring mod, chorus depth, arpeggiator >= 0.45
//   WIDE        pan spread, voice dispersion, chorus and detune together >= 0.40
//   LONG TAIL   release >= 1.8 s
// ---------------------------------------------------------------------------
enum StyleTag : int { styleSwell, stylePercussive, styleBright, styleWarm, styleResonant,
                      styleMoving, styleWide, styleLongTail, styleCount };

const char* const kStyleNames[styleCount] = { "SWELL", "PERCUSSIVE", "BRIGHT", "WARM",
                                              "RESONANT", "MOVING", "WIDE", "LONG TAIL" };

float unit01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

float logNorm(float t) // 1 ms .. 20 s onto 0..1
{
    return unit01(std::log10(std::max(t, 0.001f) / 0.001f) / std::log10(20000.0f));
}

PresetSignature computeSignature(const cs80::ParamValues& v)
{
    using namespace cs80;
    constexpr int points = PresetSignature::kEnvPoints;
    PresetSignature s;
    const float mix = unit01(v[P_mix]);
    float raw[2] {};
    for (int c = 0; c < 2; ++c)
    {
        auto& ch = s.ch[(size_t) c];
        const auto p = [&](int local) { return v[chParam(c, local)]; };
        ch.attack = p(CP_vA);   ch.decay = p(CP_vD);
        ch.sustain = unit01(p(CP_vS));  ch.release = p(CP_vR);
        ch.hpf = p(CP_hpfCutoff);  ch.lpf = p(CP_lpfCutoff);
        ch.hpfRes = p(CP_hpfRes);  ch.lpfRes = p(CP_lpfRes);
        ch.hpfOn = p(CP_hpfOn) >= 0.5f;  ch.lpfOn = p(CP_lpfOn) >= 0.5f;
        ch.saw = p(CP_saw);  ch.pulse = p(CP_pulse);  ch.sine = p(CP_sine);  ch.noise = p(CP_noise);
        raw[c] = (c == 0 ? 1.0f - mix : mix) * unit01(p(CP_vLevel));   // engine channel gain
    }
    const float loudest = std::max({ raw[0], raw[1], 1.0e-4f });
    s.dominant = raw[1] > raw[0] ? 1 : 0;
    const float sum = raw[0] + raw[1] + 1.0e-6f;
    const float weight[2] = { raw[0] / sum, raw[1] / sum };
    for (int c = 0; c < 2; ++c)
        s.ch[(size_t) c].gain = std::max(0.08f, raw[c] / loudest);

    // Shared time axis on a compressed scale so 5 ms and 5 s both stay readable.
    const auto span = [](float t)
    {
        return 0.14f + 0.86f * std::log10(1.0f + t * 40.0f) / std::log10(1.0f + 20.0f * 40.0f);
    };
    constexpr float hold = 0.55f;
    float total[2] {}, segA[2] {}, segD[2] {}, segR[2] {};
    for (int c = 0; c < 2; ++c)
    {
        const auto& ch = s.ch[(size_t) c];
        segA[c] = span(ch.attack);  segD[c] = span(ch.decay);  segR[c] = span(ch.release);
        total[c] = segA[c] + segD[c] + hold + segR[c];
    }
    const float axis = std::max(total[0], total[1]);
    for (int c = 0; c < 2; ++c)
    {
        auto& ch = s.ch[(size_t) c];
        for (int i = 0; i < points; ++i)
        {
            const float x = axis * (float) i / (float) (points - 1);
            float y = 0.0f;
            if (x < segA[c])
            {
                const float t = x / segA[c];
                y = 1.0f - (1.0f - t) * (1.0f - t);
            }
            else if (x < segA[c] + segD[c])
            {
                const float t = (x - segA[c]) / segD[c];
                y = ch.sustain + (1.0f - ch.sustain) * (1.0f - t) * (1.0f - t);
            }
            else if (x < segA[c] + segD[c] + hold)
                y = ch.sustain;
            else if (x < total[c])
            {
                const float t = (x - segA[c] - segD[c] - hold) / segR[c];
                y = ch.sustain * std::pow(1.0f - t, 1.7f);
            }
            ch.env[(size_t) i] = y * ch.gain;
        }
    }

    // Gain-weighted summary of the two channels.
    float lpfLog = 0.0f, res = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        const auto& ch = s.ch[(size_t) c];
        const float cut = ch.lpfOn ? ch.lpf : 18000.0f;
        lpfLog += weight[c] * std::log(std::max(cut, 20.0f));
        res += weight[c] * (ch.lpfOn ? ch.lpfRes : 0.0f);
        s.attack += weight[c] * ch.attack;    s.release += weight[c] * ch.release;
        s.sustain += weight[c] * ch.sustain;  s.decay += weight[c] * ch.decay;
    }
    s.cutoff = std::exp(lpfLog);
    s.resonance = std::max(res, v[P_resonance]);

    const float brightness = std::log(s.cutoff / 40.0f) / std::log(18000.0f / 40.0f)
                             + 0.08f * clampValue(v[P_brilliance], -2.0f, 2.0f);
    float pwm = 0.0f;
    for (int c = 0; c < 2; ++c)
        pwm = std::max(pwm, v[chParam(c, CP_pwmDepth)] * (s.ch[(size_t) c].pulse > 0.05f ? 1.0f : 0.0f));
    const float lfo = std::max({ v[P_lfo1Depth], v[P_lfo2Depth], v[P_lfo3Depth] });
    const float ring = std::max(v[P_ringDepth], v[P_ringAmount]);
    s.chorus = v[P_chorusOn] >= 0.5f ? v[P_chorusDepth] : 0.0f;
    const float motion = std::max({ lfo, pwm * 0.8f, ring, s.chorus * 0.5f,
                                    v[P_arpOn] >= 0.5f ? 0.9f : 0.0f });
    const float pan = 0.5f * (std::abs(v[chParam(0, CP_pan)]) + std::abs(v[chParam(1, CP_pan)]));
    const float width = 0.35f * pan + 0.30f * v[P_voiceDispersion] + 0.25f * s.chorus
                        + 0.20f * std::min(v[P_detune] / 30.0f, 1.0f);
    s.radar = { unit01(brightness), unit01(s.resonance), logNorm(s.attack),
                unit01(0.55f * s.sustain + 0.45f * logNorm(s.release)), unit01(motion), unit01(width) };

    const auto tag = [&](StyleTag t, bool on) { if (on) s.styles |= 1u << (unsigned) t; };
    tag(styleSwell, s.attack >= 0.45f);
    tag(stylePercussive, s.attack <= 0.03f);
    tag(styleBright, s.cutoff >= 4300.0f);
    tag(styleWarm, s.cutoff <= 1900.0f);
    tag(styleResonant, s.resonance >= 0.45f);
    tag(styleMoving, motion >= 0.45f);
    tag(styleWide, width >= 0.40f);
    tag(styleLongTail, s.release >= 1.8f);
    return s;
}

// Reads the parameter values of a saved user patch (an APVTS state document)
// without touching the processor. Unknown or missing entries keep defaults.
std::shared_ptr<const PresetSignature> signatureForUserFile(const juce::File& file)
{
    static const std::unordered_map<std::string, int> ids = []
    {
        std::unordered_map<std::string, int> map;
        for (int i = 0; i < cs80::P_numParams; ++i)
            map.emplace(cs80::paramId(i), i);
        return map;
    }();

    cs80::ParamValues values;
    if (auto xml = juce::XmlDocument::parse(file))
        for (auto* child : xml->getChildIterator())
        {
            if (! child->hasAttribute("id") || ! child->hasAttribute("value"))
                continue;
            const auto found = ids.find(child->getStringAttribute("id").toStdString());
            if (found == ids.end())
                continue;
            const auto& def = cs80::paramDef(found->second);
            values[found->second] = cs80::clampValue((float) child->getDoubleAttribute("value"),
                                                     def.min, def.max);
        }
    return std::make_shared<const PresetSignature>(computeSignature(values));
}

// ---------------------------------------------------------------------------
// Instrument type. Factory programs carry a category that maps directly. The 22
// "Hardware Studies" tone patches and all user patches have no usable category,
// so the lower-case name is searched for the first keyword in kKeywords (table
// order decides ties). A name with no keyword is OTHER; the Init patch is INIT.
// ---------------------------------------------------------------------------
enum TypeId : int { tBrass, tStrings, tKeys, tOrgan, tWinds, tPluck, tBass, tPads,
                    tLeads, tFx, tSequence, tInit, tOther, typeCount };

struct TypeInfo { const char* label; std::uint32_t colour; };
const TypeInfo kTypes[typeCount] = {
    { "BRASS", 0xffe6b43a },    { "STRINGS", 0xff44c474 },  { "KEYS", 0xffe6dcbc },
    { "ORGAN", 0xffd9593f },    { "WINDS", 0xff3fb8b0 },    { "PLUCK", 0xfff08f3a },
    { "BASS", 0xff5c8fe0 },     { "PADS", 0xff9a7ae6 },     { "LEADS", 0xffee4f86 },
    { "FX", 0xffbdb7ae },       { "SEQUENCE", 0xffa6d84a }, { "INIT", 0xff9a9588 },
    { "OTHER", 0xff8a8578 } };

struct Keyword { const char* word; TypeId type; };
const Keyword kKeywords[] = {
    { "string", tStrings }, { "brass", tBrass }, { "horn", tBrass }, { "flute", tWinds },
    { "wind", tWinds }, { "reed", tWinds }, { "piano", tKeys }, { "clav", tKeys },
    { "harpsi", tKeys }, { "keys", tKeys }, { "organ", tOrgan }, { "guitar", tPluck },
    { "pluck", tPluck }, { "funky", tPluck }, { "bass", tBass }, { "pad", tPads },
    { "choir", tPads }, { "lead", tLeads }, { "bell", tFx }, { "fx", tFx }, { "sweep", tFx },
    { "arp", tSequence }, { "seq", tSequence }, { "init", tInit } };

int classifyType(const juce::String& category, const juce::String& name)
{
    if (category == "Brass") return tBrass;
    if (category == "Strings") return tStrings;
    if (category == "Pads") return tPads;
    if (category == "Bass") return tBass;
    if (category == "Leads") return tLeads;
    if (category == "Keys") return tKeys;
    if (category == "FX") return tFx;
    if (category == "Sequences") return tSequence;
    if (category == "Init") return tInit;
    const auto lower = name.toLowerCase();
    for (const auto& k : kKeywords)
        if (lower.contains(k.word))
            return k.type;
    return tOther;
}

Colour typeColour(int type) { return Colour(kTypes[juce::jlimit(0, (int) typeCount - 1, type)].colour); }

constexpr const char* kScopeNames[] = { "ALL PATCHES", "FAVOURITES", "FACTORY PROGRAMS",
                                        "HARDWARE TONES", "USER" };
constexpr const char* kSortNames[] = { "BANK", "A-Z", "Z-A", "TYPE", "NEW" };
constexpr const char* kSortTitles[] = { "Sort in bank order", "Sort A to Z", "Sort Z to A",
                                        "Sort by instrument type", "Sort newest user patches first" };
constexpr int kScopeCount = 5;
constexpr int kSortCount = 5;

// Session-only favourites shared by every library window of this process.
// Deliberately not persisted: the self-test must never write files.
juce::StringArray& favouriteKeys()
{
    static juce::StringArray keys;
    return keys;
}

// ---------------------------------------------------------------------------
// Text and drawing helpers
// ---------------------------------------------------------------------------
juce::Font uiFont(float height, int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), height, style));
}

juce::Font monoFont(float height, int style = juce::Font::bold)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height, style));
}

juce::Font tracked(juce::Font font, float kerning)
{
    font.setExtraKerningFactor(kerning);
    return font;
}

float textWidth(const juce::Font& font, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText(font, text, 0.0f, 0.0f);
    return glyphs.getBoundingBox(0, -1, true).getWidth();
}

juce::Font fitFont(juce::Font font, const juce::String& text, float maxWidth)
{
    const float w = textWidth(font, text);
    if (w <= maxWidth || w <= 0.0f)
        return font;
    const float k = maxWidth / w;
    if (k >= 0.8f)
        return font.withHorizontalScale(k);
    return font.withHorizontalScale(0.8f).withHeight(font.getHeight() * k / 0.8f);
}

// Silkscreened legend: a dark undercut below the ink, like the panel print.
void legend(Graphics& g, const juce::String& text, Rectangle<float> area, const juce::Font& font,
            Colour ink, juce::Justification justification = juce::Justification::centredLeft,
            bool ellipsis = false)
{
    g.setFont(font);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawText(text, area.translated(0.0f, 1.0f), justification, ellipsis);
    g.setColour(ink);
    g.drawText(text, area, justification, ellipsis);
}

void caption(Graphics& g, const juce::String& text, Rectangle<float> area,
             juce::Justification justification = juce::Justification::centredLeft, float alpha = 0.95f)
{
    legend(g, text, area, tracked(uiFont(10.0f, juce::Font::bold), 0.16f), gold.withAlpha(alpha),
           justification);
}

// Wrapped text with the same engraved drop shadow; the area is in design units.
void paragraph(Graphics& g, const juce::String& text, Rectangle<float> area, const juce::Font& font, Colour ink,
               juce::Justification justification, int maxLines)
{
    g.setFont(font);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawFittedText(text, area.translated(0.0f, 1.0f).toNearestInt(), justification, maxLines, 0.9f);
    g.setColour(ink);
    g.drawFittedText(text, area.toNearestInt(), justification, maxLines, 0.9f);
}

juce::String formatTime(float seconds)
{
    if (seconds < 0.1f)
        return juce::String(juce::roundToInt(seconds * 1000.0f)) + " ms";
    if (seconds < 1.0f)
        return juce::String(juce::roundToInt(seconds * 1000.0f / 10.0f) * 10) + " ms";
    return juce::String(seconds, seconds < 10.0f ? 2 : 1) + " s";
}

juce::String formatHertz(float hz)
{
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
}

// Gold etched group frame with a gap for its legend, as printed around the
// control groups on the main panel.
void etchFrame(Graphics& g, Rectangle<float> r, float radius, const juce::String& title)
{
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom();
    const auto font = tracked(uiFont(10.5f, juce::Font::bold), 0.2f);
    const float gapStart = x0 + radius + 14.0f;
    const float gapEnd = gapStart + textWidth(font, title) + 16.0f;
    etchLine(g, x0 + radius, y0, gapStart, y0);
    etchLine(g, gapEnd, y0, x1 - radius, y0);
    etchLine(g, x0 + radius, y1, x1 - radius, y1);
    etchLine(g, x0, y0 + radius, x0, y1 - radius);
    etchLine(g, x1, y0 + radius, x1, y1 - radius);
    const float quarter = juce::MathConstants<float>::halfPi;
    const float corners[4][3] = { { x0 + radius, y0 + radius, 3.0f * quarter },
                                  { x1 - radius, y0 + radius, 0.0f },
                                  { x1 - radius, y1 - radius, quarter },
                                  { x0 + radius, y1 - radius, 2.0f * quarter } };
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& c : corners)
        {
            juce::Path arc;
            arc.addCentredArc(c[0], c[1] + (pass == 0 ? 1.0f : 0.0f), radius, radius, 0.0f, c[2],
                              c[2] + quarter, true);
            g.setColour(pass == 0 ? juce::Colours::black.withAlpha(0.47f) : gold.withAlpha(0.85f));
            g.strokePath(arc, juce::PathStrokeType(1.0f));
        }
    legend(g, title, { gapStart + 8.0f, y0 - 8.0f, gapEnd - gapStart, 16.0f }, font, ivory.withAlpha(0.92f),
           juce::Justification::centredLeft);
}

// Rosewood side cheek: same recipe as the chassis so the library reads as the
// same instrument.
void paintCheek(Graphics& g, Rectangle<float> r, bool left)
{
    softShadow(g, r, 6.0f, left ? 3.0f : -3.0f, 5.0f, 10.0f, 0.85f);
    g.setGradientFill(juce::ColourGradient(Colour(left ? 0xffa5522b : 0xff743419), r.getX(), 0.0f,
                                           Colour(left ? 0xff5c2a14 : 0xff3a180b), r.getRight(), 0.0f, false));
    g.fillRoundedRectangle(r, 6.0f);
    juce::Random rng(left ? 11 : 12);
    for (int i = 0; i < 30; ++i)
    {
        const float gx = r.getX() + 2.0f + rng.nextFloat() * (r.getWidth() - 4.0f);
        const float wob = (rng.nextFloat() - 0.5f) * 9.0f;
        juce::Path grain;
        grain.startNewSubPath(gx, r.getY() + 5.0f);
        grain.cubicTo(gx + wob, r.getY() + r.getHeight() * 0.33f, gx - wob * 0.8f,
                      r.getY() + r.getHeight() * 0.66f, gx + wob * 0.3f, r.getBottom() - 5.0f);
        g.setColour((rng.nextBool() ? Colour(0xffe09a5e) : Colour(0xff2a1006))
                        .withAlpha(0.05f + rng.nextFloat() * 0.10f));
        g.strokePath(grain, juce::PathStrokeType(0.6f + rng.nextFloat() * 1.3f));
    }
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.16f), 0.0f, r.getY(),
                                           juce::Colours::transparentWhite, 0.0f,
                                           r.getY() + r.getHeight() * 0.45f, false));
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(Colour(0xffe9a96c).withAlpha(0.5f));
    const float edge = left ? r.getX() + 1.0f : r.getRight() - 1.0f;
    g.drawLine(edge, r.getY() + 6.0f, edge, r.getBottom() - 6.0f, 1.2f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(r, 6.0f, 1.0f);
}

float drawPill(Graphics& g, float x, float y, float height, const juce::String& text, Colour colour,
               bool lit, float minWidth = 0.0f)
{
    const auto font = tracked(uiFont(10.0f, juce::Font::bold), 0.1f);
    const float w = std::max(minWidth, textWidth(font, text) + 18.0f);
    const Rectangle<float> r(x, y, w, height);
    g.setColour(colour.withAlpha(lit ? 0.22f : 0.09f));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(colour.withAlpha(lit ? 0.9f : 0.5f));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    g.setFont(font);
    g.setColour(colour.brighter(lit ? 0.45f : 0.3f));
    g.drawText(text, r, juce::Justification::centred, false);
    return w;
}

void fillStar(Graphics& g, juce::Point<float> c, float radius, Colour fill, bool filled, float lineWidth)
{
    juce::Path star;
    star.addStar(c, 5, radius * 0.46f, radius, -juce::MathConstants<float>::halfPi * 0.0f);
    if (filled)
    {
        g.setColour(fill);
        g.fillPath(star);
    }
    else
    {
        g.setColour(fill);
        g.strokePath(star, juce::PathStrokeType(lineWidth, juce::PathStrokeType::mitered));
    }
}

// Envelope sparkline in a tiny backlit window (result rows).
void drawMiniSignature(Graphics& g, Rectangle<float> r, const PresetSignature& sig, bool lit)
{
    g.setColour(Colour(0xff080a09));
    g.fillRoundedRectangle(r, 3.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.75f), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX(), r.getY() + 12.0f, false));
    g.fillRoundedRectangle(r.reduced(0.5f), 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawLine(r.getX() + 3.0f, r.getBottom() + 0.5f, r.getRight() - 3.0f, r.getBottom() + 0.5f, 1.0f);

    const auto outer = r.reduced(5.0f, 6.0f);
    const auto p = outer.withTrimmedBottom(6.0f);
    g.setColour(amber.withAlpha(0.07f));
    for (int i = 1; i < 4; ++i)
        g.drawVerticalLine(juce::roundToInt(p.getX() + p.getWidth() * (float) i / 4.0f), p.getY(), p.getBottom());
    g.drawHorizontalLine(juce::roundToInt(p.getCentreY()), p.getX(), p.getRight());
    g.setColour(amber.withAlpha(0.22f));
    g.drawHorizontalLine(juce::roundToInt(p.getBottom()), p.getX(), p.getRight());

    constexpr int points = PresetSignature::kEnvPoints;
    for (int pass = 0; pass < 2; ++pass)
    {
        const int c = pass == 0 ? 1 - sig.dominant : sig.dominant;
        const auto& env = sig.ch[(size_t) c].env;
        juce::Path line;
        for (int i = 0; i < points; ++i)
        {
            const float x = p.getX() + p.getWidth() * (float) i / (float) (points - 1);
            const float y = p.getBottom() - env[(size_t) i] * p.getHeight();
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        if (pass == 0)
        {
            g.setColour(ivory.withAlpha(0.42f));
            g.strokePath(line, juce::PathStrokeType(1.0f));
            continue;
        }
        juce::Path area(line);
        area.lineTo(p.getRight(), p.getBottom());
        area.lineTo(p.getX(), p.getBottom());
        area.closeSubPath();
        g.setGradientFill(juce::ColourGradient(amber.withAlpha(lit ? 0.70f : 0.52f), 0.0f, p.getY(),
                                               amber.withAlpha(0.05f), 0.0f, p.getBottom(), false));
        g.fillPath(area);
        g.setColour(lit ? amberBright : amberBright.withAlpha(0.9f));
        g.strokePath(line, juce::PathStrokeType(1.3f));
    }

    // Tone bar under the envelope: how far the low-pass opens (40 Hz .. 18 kHz).
    // Patches that share an envelope still differ here.
    const Rectangle<float> bar(outer.getX(), outer.getBottom() - 2.5f, outer.getWidth(), 2.5f);
    const float open = juce::jlimit(0.0f, 1.0f, std::log(std::max(sig.cutoff, 40.0f) / 40.0f) / std::log(18000.0f / 40.0f));
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRoundedRectangle(bar, 1.2f);
    g.setColour((lit ? amberBright : amber).withAlpha(0.9f));
    g.fillRoundedRectangle(bar.withWidth(std::max(2.5f, bar.getWidth() * open)), 1.2f);

    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

// 12 dB/oct state-variable magnitude, used only to draw the filter curve.
float filterMagnitudeDb(const PresetSignature::Channel& ch, float hz)
{
    float power = 1.0f;
    if (ch.lpfOn)
    {
        const float r = hz / std::max(ch.lpf, 20.0f);
        const float q = 0.55f + 9.0f * ch.lpfRes;
        const float a = 1.0f - r * r, b = r / q;
        power *= 1.0f / (a * a + b * b);
    }
    if (ch.hpfOn)
    {
        const float r = hz / std::max(ch.hpf, 20.0f);
        const float q = 0.55f + 9.0f * ch.hpfRes;
        const float a = 1.0f - r * r, b = r / q;
        power *= (r * r * r * r) / (a * a + b * b);
    }
    return 10.0f * std::log10(std::max(power, 1.0e-9f));
}

const Colour kChannelOne(0xffffc24d);
const Colour kChannelTwo(0xffe9dfc2);

} // namespace

// ===========================================================================
// Controls
// ===========================================================================
PlateButton::PlateButton(const juce::String& text, Kind k) : juce::Button(text), kind(k)
{
    setButtonText(text);
    setWantsKeyboardFocus(true);
    if (kind == Kind::segment)
        setClickingTogglesState(true);
    if (kind == Kind::star)
        setClickingTogglesState(false);
}

void PlateButton::paintOverChildren(Graphics& g)
{
    if (hasKeyboardFocus(true) && kind != Kind::clear)
    {
        g.setColour(Colour(0xfff6df91));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.5f);
    }
}

void PlateButton::paintButton(Graphics& g, bool hover, bool down)
{
    const float u = juce::jmax(0.5f, scale);
    const auto bounds = getLocalBounds().toFloat();
    const bool enabled = isEnabled();
    const bool on = getToggleState();
    const auto dim = [&](Colour c) { return enabled ? c : c.withMultipliedSaturation(0.3f).withMultipliedBrightness(0.62f); };

    if (kind == Kind::segment)
    {
        const auto area = bounds.reduced(1.5f * u, 1.5f * u);
        if (on)
        {
            softShadow(g, area, 3.0f * u, 0.0f, 1.0f * u, 2.0f * u, 0.5f);
            g.setGradientFill(juce::ColourGradient(Colour(0xfff0bc54), area.getX(), area.getY(),
                                                   Colour(0xffb97c20), area.getX(), area.getBottom(), false));
            g.fillRoundedRectangle(area, 3.5f * u);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.drawLine(area.getX() + 3.0f * u, area.getY() + 0.8f * u, area.getRight() - 3.0f * u,
                       area.getY() + 0.8f * u, 1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.45f));
            g.drawRoundedRectangle(area, 3.5f * u, 1.0f);
        }
        else if (hover || down)
        {
            g.setColour(ivory.withAlpha(0.09f));
            g.fillRoundedRectangle(area, 3.5f * u);
        }
        g.setFont(tracked(uiFont(11.5f * u, juce::Font::bold), 0.12f));
        g.setColour(on ? Colour(0xff2b1b05) : ivory.withAlpha(hover ? 0.95f : 0.7f));
        g.drawText(getButtonText(), area, juce::Justification::centred, false);
        return;
    }

    if (kind == Kind::star)
    {
        const auto c = bounds.getCentre();
        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.40f;
        if (on)
        {
            g.setColour(amber.withAlpha(0.18f));
            g.fillEllipse(c.x - radius * 1.5f, c.y - radius * 1.5f, radius * 3.0f, radius * 3.0f);
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.6f), true, 1.0f);
            fillStar(g, c, radius, hover ? amberBright.brighter(0.2f) : amberBright, true, 1.0f);
        }
        else
        {
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.6f), false, 1.4f * u);
            fillStar(g, c, radius, hover ? amberBright : ivory.withAlpha(0.55f), false, 1.4f * u);
        }
        return;
    }

    if (kind == Kind::clear)
    {
        const auto c = bounds.getCentre();
        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.30f;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillEllipse(c.x - radius, c.y - radius + u, radius * 2.0f, radius * 2.0f);
        g.setGradientFill(juce::ColourGradient(Colour(hover ? 0xff5c564b : 0xff45413a), c.x, c.y - radius,
                                               Colour(0xff201e1b), c.x, c.y + radius, false));
        g.fillEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
        const float k = radius * 0.42f;
        g.setColour(hover ? amberBright : ivory.withAlpha(0.8f));
        g.drawLine(c.x - k, c.y - k, c.x + k, c.y + k, 1.6f * u);
        g.drawLine(c.x - k, c.y + k, c.x + k, c.y - k, 1.6f * u);
        return;
    }

    // Raised key: cast shadow, visible thickness, lit top face.
    Colour top(0xff4c4841), bottom(0xff2a2824), ink = ivory;
    if (kind == Kind::primary)
    {
        top = Colour(0xfff2c05a);
        bottom = Colour(0xffc5882b);
        ink = Colour(0xff2b1b05);
    }
    else if (kind == Kind::danger)
    {
        top = Colour(0xff4f3733);
        bottom = Colour(0xff2b1b1a);
        ink = Colour(0xffff9d89);
    }
    top = dim(hover && enabled ? top.brighter(0.14f) : top);
    bottom = dim(hover && enabled ? bottom.brighter(0.10f) : bottom);
    ink = dim(ink);

    const float depth = 2.4f * u;
    const float radius = 4.0f * u;
    const auto base = bounds.reduced(1.5f * u, 1.5f * u).withTrimmedBottom(depth);
    const auto face = base.translated(0.0f, down ? depth * 0.6f : 0.0f);
    g.setColour(juce::Colours::black.withAlpha(0.38f));
    g.fillRoundedRectangle(base.translated(0.0f, depth + u).expanded(0.8f * u, 0.4f * u), radius + u);
    g.setColour(bottom.darker(0.75f));
    g.fillRoundedRectangle(base.translated(0.0f, depth), radius);
    g.setGradientFill(juce::ColourGradient(top, face.getX(), face.getY(), bottom, face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle(face, radius);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.10f), face.getX(), face.getY(),
                                           juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
    g.fillRoundedRectangle(face, radius);
    g.setColour(juce::Colours::white.withAlpha(enabled ? 0.30f : 0.10f));
    g.drawLine(face.getX() + radius * 0.8f, face.getY() + 0.8f * u, face.getRight() - radius * 0.8f,
               face.getY() + 0.8f * u, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(face, radius, 1.0f);
    if (kind == Kind::primary && enabled)
    {
        g.setColour(amberBright.withAlpha(hover ? 0.55f : 0.32f));
        g.drawRoundedRectangle(face.expanded(1.5f * u), radius + 1.5f * u, 1.3f * u);
    }

    auto textArea = face;
    const float iconBox = 16.0f * u;
    g.setColour(ink);
    if (kind == Kind::previous || kind == Kind::next)
    {
        const bool left = kind == Kind::previous;
        auto icon = left ? textArea.removeFromLeft(iconBox + 8.0f * u) : textArea.removeFromRight(iconBox + 8.0f * u);
        const auto c = icon.getCentre();
        juce::Path tri;
        const float h = 5.5f * u, w = 4.5f * u;
        if (left) tri.addTriangle(c.x + w, c.y - h, c.x + w, c.y + h, c.x - w, c.y);
        else      tri.addTriangle(c.x - w, c.y - h, c.x - w, c.y + h, c.x + w, c.y);
        g.fillPath(tri);
    }
    else if (kind == Kind::close)
    {
        auto icon = textArea.removeFromLeft(iconBox + 8.0f * u);
        const auto c = icon.getCentre();
        const float k = 4.2f * u;
        g.drawLine(c.x - k, c.y - k, c.x + k, c.y + k, 1.8f * u);
        g.drawLine(c.x - k, c.y + k, c.x + k, c.y - k, 1.8f * u);
    }
    g.setFont(tracked(uiFont(12.5f * u, juce::Font::bold), 0.14f));
    if (kind == Kind::primary)
    {
        g.setColour(juce::Colours::white.withAlpha(enabled ? 0.28f : 0.0f));
        g.drawText(getButtonText(), textArea.translated(0.0f, u), juce::Justification::centred, false);
    }
    g.setColour(ink);
    g.drawText(getButtonText(), textArea, juce::Justification::centred, false);
}

ChipButton::ChipButton(const juce::String& label, juce::Colour accent) : juce::Button(label), accentColour(accent)
{
    setButtonText(label);
    setClickingTogglesState(true);
    setWantsKeyboardFocus(true);
}

void ChipButton::setCount(int newCount)
{
    if (count == newCount)
        return;
    count = newCount;
    repaint();
}

void ChipButton::paintOverChildren(Graphics& g)
{
    if (hasKeyboardFocus(true))
    {
        g.setColour(Colour(0xfff6df91));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f * scale, 1.5f);
    }
}

void ChipButton::paintButton(Graphics& g, bool hover, bool down)
{
    const float u = juce::jmax(0.5f, scale);
    const auto b = getLocalBounds().toFloat().reduced(0.5f * u, 0.5f * u).withTrimmedBottom(1.0f * u);
    const bool on = getToggleState();
    const bool faded = count == 0 && ! on;
    const float radius = 4.0f * u;

    Colour top(0xff35322d), bottom(0xff1f1d1a);
    if (hover || down)
    {
        top = top.brighter(0.16f);
        bottom = bottom.brighter(0.12f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(b.translated(0.0f, 1.2f * u), radius);
    g.setGradientFill(juce::ColourGradient(top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, radius);
    if (on)
    {
        g.setGradientFill(juce::ColourGradient(accentColour.withAlpha(0.34f), b.getX(), b.getY(),
                                               accentColour.withAlpha(0.10f), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b, radius);
        g.setColour(accentColour.withAlpha(0.28f));
        g.drawRoundedRectangle(b.expanded(1.3f * u), radius + u, 1.4f * u);
        g.setColour(accentColour.withAlpha(0.95f));
        g.drawRoundedRectangle(b, radius, 1.2f * u);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(faded ? 0.05f : 0.11f));
        g.drawLine(b.getX() + radius, b.getY() + 0.8f * u, b.getRight() - radius, b.getY() + 0.8f * u, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawRoundedRectangle(b, radius, 1.0f);
    }

    const float lampX = b.getX() + 12.0f * u, lampY = b.getCentreY();
    if (starGlyph)
    {
        if (on)
        {
            g.setColour(accentColour.withAlpha(0.28f));
            g.fillEllipse(lampX - 8.0f * u, lampY - 8.0f * u, 16.0f * u, 16.0f * u);
        }
        fillStar(g, { lampX, lampY }, 5.6f * u, on ? accentColour.brighter(0.5f) : accentColour.withAlpha(faded ? 0.3f : 0.7f),
                 on, 1.2f * u);
    }
    else
    {
        if (on)
        {
            g.setColour(accentColour.withAlpha(0.30f));
            g.fillEllipse(lampX - 7.0f * u, lampY - 7.0f * u, 14.0f * u, 14.0f * u);
        }
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillEllipse(lampX - 3.9f * u, lampY - 3.4f * u, 7.8f * u, 7.8f * u);
        g.setColour(on ? accentColour.brighter(0.55f) : accentColour.withAlpha(faded ? 0.22f : 0.5f).darker(0.2f));
        g.fillEllipse(lampX - 3.3f * u, lampY - 3.6f * u, 6.6f * u, 6.6f * u);
        if (on)
        {
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(lampX - 1.6f * u, lampY - 2.6f * u, 2.4f * u, 2.0f * u);
        }
    }

    auto text = b.withTrimmedLeft(24.0f * u).withTrimmedRight(7.0f * u);
    const auto countText = juce::String(count);
    const auto countFont = monoFont(10.0f * u, juce::Font::bold);
    const float countWidth = textWidth(countFont, countText);
    g.setFont(countFont);
    g.setColour(on ? accentColour.brighter(0.4f) : ivoryDim.withAlpha(faded ? 0.35f : 0.8f));
    g.drawText(countText, text.removeFromRight(countWidth + 2.0f * u), juce::Justification::centredRight, false);
    text.removeFromRight(4.0f * u);
    const auto label = getButtonText();
    const auto font = fitFont(tracked(uiFont(10.5f * u, juce::Font::bold), 0.1f), label, text.getWidth());
    g.setFont(font);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawText(label, text.translated(0.0f, 1.0f), juce::Justification::centredLeft, false);
    g.setColour(on ? accentColour.brighter(0.65f) : ivory.withAlpha(faded ? 0.38f : 0.9f));
    g.drawText(label, text, juce::Justification::centredLeft, false);
}

void InfoLabel::paint(Graphics& g)
{
    const auto font = getFont();
    g.setFont(font);
    g.setColour(findColour(juce::Label::textColourId));
    const int lines = juce::jmax(1, (int) ((float) getHeight() / std::max(1.0f, font.getHeight() * 1.22f)));
    g.drawFittedText(getText(), getLocalBounds(), getJustificationType(), lines, getMinimumHorizontalScale());
}

GlassField::GlassField(Icon i) : icon(i)
{
    setMultiLine(false);
    setReturnKeyStartsNewLine(false);
    setScrollbarsShown(false);
    setSelectAllWhenFocused(false);
    setPopupMenuEnabled(true);
    setJustification(juce::Justification::centredLeft);
    setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::textColourId, ivory);
    setColour(juce::TextEditor::highlightColourId, amber.withAlpha(0.4f));
    setColour(juce::TextEditor::highlightedTextColourId, ivory.brighter(0.3f));
    setColour(juce::CaretComponent::caretColourId, amberBright);
}

void GlassField::paint(Graphics& g)
{
    const float u = juce::jmax(0.5f, scale);
    const auto r = getLocalBounds().toFloat().reduced(1.0f, 1.0f);
    drawWell(g, r.withTrimmedBottom(1.0f), 5.0f * u);
    if (icon == Icon::search)
    {
        const float cx = r.getX() + 21.0f * u, cy = r.getCentreY() - 1.0f * u, rad = 6.2f * u;
        g.setColour(gold.withAlpha(hasKeyboardFocus(true) ? 1.0f : 0.8f));
        g.drawEllipse(cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.8f * u);
        g.drawLine(cx + rad * 0.72f, cy + rad * 0.72f, cx + rad * 1.65f, cy + rad * 1.65f, 2.2f * u);
    }
}

void GlassField::paintOverChildren(Graphics& g)
{
    juce::TextEditor::paintOverChildren(g); // placeholder text
    if (hasKeyboardFocus(true))
    {
        const float u = juce::jmax(0.5f, scale);
        g.setColour(amber.withAlpha(0.22f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f * u, 2.5f * u);
        g.setColour(amberBright.withAlpha(0.9f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.2f), 5.0f * u, 1.2f * u);
    }
}

bool GlassField::keyPressed(const juce::KeyPress& key)
{
    if (onSpecialKey != nullptr && onSpecialKey(key))
        return true;
    // Escape belongs to the editor, which closes the library.
    if (key == juce::KeyPress::escapeKey)
        return false;
    return juce::TextEditor::keyPressed(key);
}

void GlassField::focusGained(juce::Component::FocusChangeType cause)
{
    juce::TextEditor::focusGained(cause);
    repaint();
}

void GlassField::focusLost(juce::Component::FocusChangeType cause)
{
    juce::TextEditor::focusLost(cause);
    repaint();
}

// ===========================================================================
// Nested helper components
// ===========================================================================

// Result list with extra keys: Space favourites, typing jumps to the search box.
class PresetBrowser::ResultList final : public juce::ListBox
{
public:
    ResultList() : juce::ListBox("presets", nullptr)
    {
        setTitle("Preset results");
    }

    std::function<bool(const juce::KeyPress&)> onExtraKey;
    std::function<void()> onFocusChange;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (juce::ListBox::keyPressed(key))
            return true;
        return onExtraKey != nullptr && onExtraKey(key);
    }

    void focusGained(juce::Component::FocusChangeType cause) override
    {
        juce::ListBox::focusGained(cause);
        if (onFocusChange != nullptr) onFocusChange();
    }

    void focusLost(juce::Component::FocusChangeType cause) override
    {
        juce::ListBox::focusLost(cause);
        if (onFocusChange != nullptr) onFocusChange();
    }
};

// Static art: wooden cheeks, charcoal face, etched frames, wells. Painted once
// per size into the buffered image; nothing here changes while browsing.
class PresetBrowser::Surface final : public juce::Component
{
public:
    explicit Surface(PresetBrowser& o) : owner(o)
    {
        setOpaque(true);
        setInterceptsMouseClicks(false, false);
        setBufferedToImage(true);
        setAccessible(false);
    }

    void paint(Graphics& g) override
    {
        const auto& L = owner.layout;
        g.fillAll(Colour(0xff0a0908));
        g.addTransform(juce::AffineTransform::scale(L.s));
        const float W = L.width, H = L.height;

        paintCheek(g, { 4.0f, 6.0f, 40.0f, H - 12.0f }, true);
        paintCheek(g, { W - 44.0f, 6.0f, 40.0f, H - 12.0f }, false);

        fillPanelFace(g, L.face);
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.45f), L.face.getX(), 0.0f,
                                               juce::Colours::transparentBlack, L.face.getX() + 34.0f, 0.0f, false));
        g.fillRect(L.face.getX(), L.face.getY(), 34.0f, L.face.getHeight());
        g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.45f), L.face.getRight(), 0.0f,
                                               juce::Colours::transparentBlack, L.face.getRight() - 34.0f, 0.0f, false));
        g.fillRect(L.face.getRight() - 34.0f, L.face.getY(), 34.0f, L.face.getHeight());
        {
            juce::ColourGradient pool(Colour(0xffffe9c0).withAlpha(0.075f), W * 0.35f, H * 0.3f,
                                      juce::Colours::transparentBlack, W * 0.8f, H * 0.95f, true);
            g.setGradientFill(pool);
            g.fillRect(L.face);
            juce::ColourGradient edge(juce::Colours::transparentBlack, W * 0.5f, H * 0.5f,
                                      juce::Colours::black.withAlpha(0.30f), W * 0.95f, H * 1.05f, true);
            g.setGradientFill(edge);
            g.fillRect(L.face);
        }
        g.setColour(Colour(0xff77726a).withAlpha(0.75f));
        g.drawLine(L.face.getX(), L.face.getY() + 1.0f, L.face.getRight(), L.face.getY() + 1.0f, 1.6f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawLine(L.face.getX(), L.face.getBottom(), L.face.getRight(), L.face.getBottom(), 1.5f);

        // Header band: a darker strip with a double etched rule beneath it.
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRect(L.header);
        etchLine(g, L.face.getX() + 22.0f, L.header.getBottom(), L.face.getRight() - 22.0f, L.header.getBottom(), 0.8f);
        etchLine(g, L.face.getX() + 22.0f, L.header.getBottom() + 4.0f, L.face.getRight() - 22.0f,
                 L.header.getBottom() + 4.0f, 0.3f);

        auto title = tracked(uiFont(24.0f, juce::Font::bold), 0.14f);
        legend(g, "PRESET LIBRARY", { 78.0f, 26.0f, 300.0f, 34.0f }, title, ivory);
        caption(g, "SORT BY", { L.sortStrip.getX() + 4.0f, 22.0f, 120.0f, 14.0f });

        const float cornerX = L.face.getX() + 13.0f, cornerY = L.face.getY() + 13.0f;
        drawScrew(g, { cornerX, cornerY }, 4.6f, 0.4f);
        drawScrew(g, { L.face.getRight() - 13.0f, cornerY }, 4.6f, 1.1f);
        drawScrew(g, { cornerX, L.face.getBottom() - 13.0f }, 4.6f, 2.0f);
        drawScrew(g, { L.face.getRight() - 13.0f, L.face.getBottom() - 13.0f }, 4.6f, 0.9f);

        // Sort strip trough.
        drawWell(g, L.sortStrip.expanded(2.0f), 6.0f);

        // The three etched groups.
        etchFrame(g, L.leftPlate, 6.0f, "FILTERS");
        etchFrame(g, L.results, 6.0f, "PATCHES");
        etchFrame(g, L.detail, 6.0f, "INSPECTOR");
        for (auto plate : { L.leftPlate, L.detail })
        {
            g.setColour(juce::Colours::black.withAlpha(0.13f));
            g.fillRoundedRectangle(plate.reduced(2.0f), 5.0f);
        }

        // Recessed results well with a felt-like speckle.
        drawWell(g, L.well, 8.0f);
        g.setGradientFill(juce::ColourGradient(Colour(0xff161513).withAlpha(0.55f), 0.0f, L.well.getY(),
                                               Colour(0xff0d0c0b).withAlpha(0.2f), 0.0f, L.well.getBottom(), false));
        g.fillRoundedRectangle(L.well.reduced(1.0f), 8.0f);
        juce::Random rng(94);
        for (int i = 0; i < (int) (L.well.getWidth() * L.well.getHeight() / 260.0f); ++i)
        {
            g.setColour((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha(0.012f + rng.nextFloat() * 0.025f));
            g.fillRect(L.well.getX() + 4.0f + rng.nextFloat() * (L.well.getWidth() - 8.0f),
                       L.well.getY() + 4.0f + rng.nextFloat() * (L.well.getHeight() - 8.0f), 1.0f, 1.0f);
        }
    }

private:
    PresetBrowser& owner;
};

// Dynamic overlay painted between the static art and the controls: counts,
// group captions, the inspector (name, tags, signature scopes) and the empty
// state. Only the parts that intersect the dirty clip are drawn, so list
// scrolling never pays for the inspector.
class PresetBrowser::Legends final : public juce::Component
{
public:
    explicit Legends(PresetBrowser& o) : owner(o)
    {
        setInterceptsMouseClicks(false, false);
        setAccessible(false);
    }

    void repaintDetail()
    {
        repaint(toPixels(owner.layout.detail.expanded(2.0f)));
    }
    void repaintStrip()
    {
        repaint(toPixels(owner.layout.strip.expanded(2.0f)));
        repaint(toPixels({ 70.0f, 56.0f, 330.0f, 30.0f }));
    }
    void repaintFilters()
    {
        repaint(toPixels(owner.layout.leftPlate));
    }
    void repaintWell()
    {
        repaint(toPixels(owner.layout.well.expanded(4.0f)));
    }

    void paint(Graphics& g) override
    {
        const auto& L = owner.layout;
        if (L.s <= 0.0f)
            return;
        const auto clip = g.getClipBounds().toFloat().transformedBy(juce::AffineTransform::scale(1.0f / L.s));
        g.addTransform(juce::AffineTransform::scale(L.s));

        if (clip.intersects(Rectangle<float>(70.0f, 56.0f, 330.0f, 30.0f)))
            paintSubtitle(g);
        if (clip.intersects(L.strip.expanded(2.0f)))
            paintStrip(g);
        if (clip.intersects(L.leftPlate))
            paintFilterCaptions(g);
        if (clip.intersects(L.well.expanded(4.0f)))
        {
            if (owner.filtered.empty())
                paintEmpty(g);
            if (owner.list != nullptr && owner.list->hasKeyboardFocus(true))
            {
                g.setColour(amber.withAlpha(0.20f));
                g.drawRoundedRectangle(L.well.expanded(1.5f), 9.5f, 3.0f);
                g.setColour(amberBright.withAlpha(0.8f));
                g.drawRoundedRectangle(L.well.expanded(0.5f), 8.5f, 1.2f);
            }
        }
        if (clip.intersects(L.detail))
            paintDetail(g);
    }

private:
    juce::Rectangle<int> toPixels(Rectangle<float> r) const
    {
        return r.transformedBy(juce::AffineTransform::scale(owner.layout.s)).getSmallestIntegerContainer();
    }

    void paintSubtitle(Graphics& g)
    {
        int factory = 0, hardware = 0, user = 0;
        for (const auto& e : owner.entries)
        {
            if (e.isUser) ++user;
            else if (e.category == "Hardware Studies") ++hardware;
            else ++factory;
        }
        const auto dot = juce::String::charToString(0x00b7);
        const auto text = juce::String((int) owner.entries.size()) + " PATCHES   " + dot + "   "
                          + juce::String(hardware) + " HARDWARE TONES   " + dot + "   "
                          + juce::String(user) + " USER";
        (void) factory;
        legend(g, text, { 79.0f, 60.0f, 330.0f, 16.0f }, tracked(uiFont(9.5f, juce::Font::bold), 0.1f),
               amber.withAlpha(0.95f));
    }

    void paintStrip(Graphics& g)
    {
        const auto& L = owner.layout;
        const Rectangle<float> window(L.strip.getX(), L.strip.getY() + 1.0f, 118.0f, L.strip.getHeight() - 2.0f);
        drawWell(g, window, 4.0f);
        const auto shown = juce::String((int) owner.filtered.size()).paddedLeft('0', 3);
        const auto total = juce::String((int) owner.entries.size()).paddedLeft('0', 3);
        g.setFont(monoFont(14.0f));
        g.setColour(amber.withAlpha(0.20f));
        g.drawText(shown + "/" + total, window.translated(0.0f, 0.5f), juce::Justification::centred, false);
        g.setColour(amberBright);
        g.drawText(shown + "/" + total, window, juce::Justification::centred, false);
        caption(g, "PATCHES SHOWN", { window.getRight() + 12.0f, window.getY(), 130.0f, window.getHeight() });

        const auto hint = juce::String("UP / DOWN BROWSE   ENTER LOAD   SPACE FAVOURITE   DEL REMOVE USER");
        legend(g, hint, { window.getRight() + 150.0f, window.getY(), L.strip.getRight() - window.getRight() - 150.0f,
                          window.getHeight() },
               tracked(uiFont(8.5f, juce::Font::bold), 0.1f), ivoryDim.withAlpha(0.75f), juce::Justification::centredRight, true);
    }

    void paintFilterCaptions(Graphics& g)
    {
        const auto& L = owner.layout;
        const float x = L.leftPlate.getX() + 20.0f, w = L.leftPlate.getWidth() - 40.0f;
        const auto head = [&](const juce::String& text, const juce::String& hint, float y)
        {
            caption(g, text, { x, y, 160.0f, 14.0f });
            if (hint.isNotEmpty())
                legend(g, hint, { x + w - 90.0f, y, 90.0f, 14.0f },
                       tracked(uiFont(8.5f, juce::Font::bold), 0.12f), ivoryDim.withAlpha(0.7f),
                       juce::Justification::centredRight);
            etchLine(g, x, y + 17.0f, x + w, y + 17.0f, 0.35f);
        };
        head("LIBRARY", "ONE OF", L.scopeCaptionY);
        if (L.typeCaptionY > 0.0f)
            head("INSTRUMENT", "ANY OF", L.typeCaptionY);
        if (L.styleCaptionY > 0.0f)
            head("STYLE", "ALL OF", L.styleCaptionY);
        paintActiveFilters(g);
    }

    // A small readout in the free space above CLEAR ALL FILTERS that spells out
    // what is narrowing the list, so a long chip column never hides the cause.
    void paintActiveFilters(Graphics& g)
    {
        const auto& L = owner.layout;
        const float x = L.leftPlate.getX() + 20.0f, w = L.leftPlate.getWidth() - 40.0f;
        const float top = L.chipsBottom + 8.0f;
        const float bottom = L.leftPlate.getBottom() - 20.0f - 38.0f - 12.0f;
        if (bottom - top < 40.0f)
            return;
        const Rectangle<float> box(x, std::max(top, bottom - 54.0f), w, std::min(54.0f, bottom - top));
        drawWell(g, box, 5.0f);

        juce::StringArray parts;
        const auto query = owner.search.getText().trim();
        if (query.isNotEmpty())
            parts.add(juce::String(juce::CharPointer_UTF8("\xe2\x80\x9c")) + query
                      + juce::String(juce::CharPointer_UTF8("\xe2\x80\x9d")));
        if (owner.scopeFilter != 0)
            parts.add(kScopeNames[owner.scopeFilter]);
        juce::StringArray types, styles;
        for (int t = 0; t < typeCount; ++t)
            if (owner.typeFilter & (1u << (unsigned) t))
                types.add(kTypes[t].label);
        for (int t = 0; t < styleCount; ++t)
            if (owner.styleFilter & (1u << (unsigned) t))
                styles.add(kStyleNames[t]);
        if (! types.isEmpty()) parts.add(types.joinIntoString(" + "));
        if (! styles.isEmpty()) parts.add(styles.joinIntoString(" + "));

        caption(g, "ACTIVE FILTERS", { box.getX() + 12.0f, box.getY() + 4.0f, 150.0f, 18.0f }, juce::Justification::centredLeft, 0.8f);
        const bool none = parts.isEmpty();
        legend(g, none ? juce::String("NONE") : juce::String(parts.size()),
               { box.getRight() - 52.0f, box.getY() + 4.0f, 40.0f, 18.0f }, monoFont(11.5f),
               none ? ivoryDim.withAlpha(0.7f) : amberBright, juce::Justification::centredRight);
        const auto dot = juce::String("  ") + juce::String::charToString(0x00b7) + "  ";
        paragraph(g, none ? juce::String("Whole library shown") : parts.joinIntoString(dot),
                  { box.getX() + 12.0f, box.getY() + 22.0f, box.getWidth() - 24.0f, box.getHeight() - 26.0f },
                  uiFont(11.5f, juce::Font::bold), none ? ivoryDim.withAlpha(0.8f) : ivory.withAlpha(0.92f),
                  juce::Justification::topLeft, 2);
    }

    void paintEmpty(Graphics& g)
    {
        const auto& L = owner.layout;
        const auto c = L.well.getCentre();
        const auto font = tracked(uiFont(22.0f, juce::Font::bold), 0.16f);
        legend(g, "NO PATCHES MATCH", { L.well.getX(), c.y - 56.0f, L.well.getWidth(), 34.0f }, font, ivory.withAlpha(0.9f),
               juce::Justification::centred);
        legend(g, "Loosen the search words or switch off a filter.",
               { L.well.getX(), c.y - 20.0f, L.well.getWidth(), 20.0f }, uiFont(12.5f), ivoryDim,
               juce::Justification::centred);
    }

    // ---- inspector ---------------------------------------------------------
    void paintDetail(Graphics& g)
    {
        const auto& L = owner.layout;
        const auto& D = L.detail;
        const float x = D.getX() + 22.0f, w = D.getWidth() - 44.0f;
        caption(g, "SELECTED PATCH", { x, D.getY() + 20.0f, 200.0f, 14.0f });

        const auto* e = owner.selectedEntry();
        if (e == nullptr)
        {
            const auto c = D.getCentre();
            legend(g, "SELECT A PATCH", { D.getX(), c.y - 40.0f, D.getWidth(), 30.0f },
                   tracked(uiFont(20.0f, juce::Font::bold), 0.16f), ivory.withAlpha(0.85f), juce::Justification::centred);
            paragraph(g, "Click a card or use the arrow keys to inspect its envelope, filter and character.",
                      { D.getX() + 50.0f, c.y - 2.0f, D.getWidth() - 100.0f, 44.0f }, uiFont(12.5f), ivoryDim,
                      juce::Justification::centredTop, 3);
            caption(g, "SAVE CURRENT SOUND AS", { x, D.getY() + 706.0f, w, 14.0f });
            return;
        }

        // Name
        const auto nameFont = fitFont(tracked(uiFont(30.0f, juce::Font::bold), 0.02f), e->name, w);
        legend(g, e->name, { x, D.getY() + 44.0f, w, 40.0f }, nameFont, ivory);

        // Tag pills
        const float px = x;
        float cursor = px;
        cursor += drawPill(g, cursor, D.getY() + 92.0f, 22.0f, kTypes[e->type].label, typeColour(e->type), true) + 8.0f;
        const juce::String bank = e->isUser ? "USER" : (e->category == "Hardware Studies" ? "HARDWARE STUDIES" : "FACTORY");
        cursor += drawPill(g, cursor, D.getY() + 92.0f, 22.0f, bank, e->isUser ? teal.brighter(0.4f) : ivory, false) + 8.0f;
        if (owner.isFavourite(*e))
            drawPill(g, cursor, D.getY() + 92.0f, 22.0f, "FAVOURITE", amberBright, true);

        float sx = px;
        int hidden = 0;
        for (int t = 0; t < styleCount; ++t)
        {
            if ((e->styles & (1u << (unsigned) t)) == 0)
                continue;
            const auto font = tracked(uiFont(10.0f, juce::Font::bold), 0.1f);
            const float pw = textWidth(font, kStyleNames[t]) + 18.0f;
            if (sx + pw > px + w - 36.0f)
            {
                ++hidden;
                continue;
            }
            sx += drawPill(g, sx, D.getY() + 122.0f, 20.0f, kStyleNames[t], amber, false) + 6.0f;
        }
        if (hidden > 0)
            legend(g, "+" + juce::String(hidden), { sx, D.getY() + 122.0f, 30.0f, 20.0f }, uiFont(10.5f, juce::Font::bold),
                   ivoryDim);
        else if (e->styles == 0)
            legend(g, "NO STYLE TAGS", { px, D.getY() + 122.0f, 160.0f, 20.0f },
                   tracked(uiFont(9.5f, juce::Font::bold), 0.12f), ivoryDim.withAlpha(0.6f));

        // Signature
        etchLine(g, x, D.getY() + 214.0f, x + w, D.getY() + 214.0f, 0.5f);
        caption(g, "SIGNATURE", { x, D.getY() + 222.0f, 140.0f, 14.0f });
        legend(g, "DRAWN FROM THE PATCH PARAMETERS", { x + w - 220.0f, D.getY() + 222.0f, 220.0f, 14.0f },
               tracked(uiFont(8.5f, juce::Font::bold), 0.1f), ivoryDim.withAlpha(0.7f), juce::Justification::centredRight);

        if (e->signature == nullptr)
            return;
        const auto& sig = *e->signature;
        paintEnvelopeScope(g, { x, D.getY() + 242.0f, w, 96.0f }, sig);
        paintFilterScope(g, { x, D.getY() + 344.0f, w, 96.0f }, sig);
        paintRadar(g, { x, D.getY() + 446.0f, 164.0f, 148.0f }, sig);
        paintFacts(g, { x + 164.0f + 16.0f, D.getY() + 446.0f, w - 164.0f - 16.0f, 148.0f }, sig);

        // Position counter between PREV and NEXT.
        const auto counter = (owner.selectedFilteredRow >= 0 ? juce::String(owner.selectedFilteredRow + 1) : juce::String("--"))
                             + " / " + juce::String((int) owner.filtered.size());
        legend(g, counter, { x + 116.0f, D.getY() + 602.0f, w - 232.0f, 38.0f }, monoFont(15.0f), amberBright.withAlpha(0.95f),
               juce::Justification::centred);

        caption(g, "SAVE CURRENT SOUND AS", { x, D.getY() + 706.0f, w, 14.0f });
    }

    static void scopeFrame(Graphics& g, Rectangle<float> r, const juce::String& title, const PresetSignature& sig,
                           bool identical)
    {
        drawWell(g, r, 5.0f);
        caption(g, title, { r.getX() + 11.0f, r.getY() + 6.0f, 150.0f, 14.0f }, juce::Justification::centredLeft, 0.85f);
        float lx = r.getRight() - 12.0f;
        const auto font = tracked(uiFont(9.5f, juce::Font::bold), 0.1f);
        const auto key = [&](const juce::String& text, Colour c)
        {
            const float tw = textWidth(font, text);
            lx -= tw;
            legend(g, text, { lx, r.getY() + 6.0f, tw + 2.0f, 14.0f }, font, c);
            lx -= 14.0f;
            g.setColour(c);
            g.fillEllipse(lx + 2.0f, r.getY() + 10.5f, 6.0f, 6.0f);
            lx -= 8.0f;
        };
        (void) sig;
        if (identical)
            key("I = II", kChannelOne);
        else
        {
            key("II", kChannelTwo);
            key("I", kChannelOne);
        }
    }

    static void paintEnvelopeScope(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        constexpr int points = PresetSignature::kEnvPoints;
        float diff = 0.0f;
        for (int i = 0; i < points; ++i)
            diff = std::max(diff, std::abs(sig.ch[0].env[(size_t) i] - sig.ch[1].env[(size_t) i]));
        scopeFrame(g, r, "AMP ENVELOPE", sig, diff < 0.02f);
        const auto p = Rectangle<float>(r.getX() + 12.0f, r.getY() + 26.0f, r.getWidth() - 24.0f, r.getHeight() - 36.0f);
        g.setColour(amber.withAlpha(0.07f));
        for (int i = 1; i < 6; ++i)
            g.drawVerticalLine(juce::roundToInt(p.getX() + p.getWidth() * (float) i / 6.0f), p.getY(), p.getBottom());
        g.drawHorizontalLine(juce::roundToInt(p.getCentreY()), p.getX(), p.getRight());
        g.drawHorizontalLine(juce::roundToInt(p.getY()), p.getX(), p.getRight());
        g.setColour(amber.withAlpha(0.3f));
        g.drawHorizontalLine(juce::roundToInt(p.getBottom()), p.getX(), p.getRight());

        for (int pass = 0; pass < 2; ++pass)
        {
            const int c = pass == 0 ? 1 : 0;
            if (c == 1 && diff < 0.02f)
                continue;
            const auto colour = c == 0 ? kChannelOne : kChannelTwo;
            juce::Path line;
            for (int i = 0; i < points; ++i)
            {
                const float x = p.getX() + p.getWidth() * (float) i / (float) (points - 1);
                const float y = p.getBottom() - sig.ch[(size_t) c].env[(size_t) i] * p.getHeight();
                if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
            }
            juce::Path area(line);
            area.lineTo(p.getRight(), p.getBottom());
            area.lineTo(p.getX(), p.getBottom());
            area.closeSubPath();
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(c == 0 ? 0.42f : 0.20f), 0.0f, p.getY(),
                                                   colour.withAlpha(0.03f), 0.0f, p.getBottom(), false));
            g.fillPath(area);
            g.setColour(colour.withAlpha(c == 0 ? 1.0f : 0.8f));
            g.strokePath(line, juce::PathStrokeType(c == 0 ? 1.7f : 1.3f));
        }
    }

    static void paintFilterScope(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        constexpr int points = 90;
        constexpr float dbTop = 18.0f, dbBottom = -36.0f;
        bool identical = sig.ch[0].lpf == sig.ch[1].lpf && sig.ch[0].hpf == sig.ch[1].hpf
                         && sig.ch[0].lpfRes == sig.ch[1].lpfRes && sig.ch[0].hpfRes == sig.ch[1].hpfRes
                         && sig.ch[0].lpfOn == sig.ch[1].lpfOn && sig.ch[0].hpfOn == sig.ch[1].hpfOn;
        scopeFrame(g, r, "VCF RESPONSE", sig, identical);
        const auto p = Rectangle<float>(r.getX() + 12.0f, r.getY() + 26.0f, r.getWidth() - 24.0f, r.getHeight() - 42.0f);
        const auto xForHz = [&](float hz) { return p.getX() + p.getWidth() * std::log(hz / 20.0f) / std::log(1000.0f); };
        const auto yForDb = [&](float db)
        {
            return p.getY() + p.getHeight() * (dbTop - juce::jlimit(dbBottom, dbTop, db)) / (dbTop - dbBottom);
        };
        const auto labelFont = tracked(uiFont(8.0f, juce::Font::bold), 0.08f);
        const float markHz[3] = { 100.0f, 1000.0f, 10000.0f };
        const char* const markText[3] = { "100", "1k", "10k" };
        for (int m = 0; m < 3; ++m)
        {
            const float mx = xForHz(markHz[m]);
            g.setColour(amber.withAlpha(0.09f));
            g.drawVerticalLine(juce::roundToInt(mx), p.getY(), p.getBottom());
            legend(g, markText[m], { mx - 14.0f, p.getBottom() + 1.0f, 28.0f, 12.0f }, labelFont,
                   ivoryDim.withAlpha(0.75f), juce::Justification::centred);
        }
        g.setColour(amber.withAlpha(0.18f));
        g.drawHorizontalLine(juce::roundToInt(yForDb(0.0f)), p.getX(), p.getRight());

        for (int pass = 0; pass < 2; ++pass)
        {
            const int c = pass == 0 ? 1 : 0;
            if (c == 1 && identical)
                continue;
            const auto colour = c == 0 ? kChannelOne : kChannelTwo;
            juce::Path line;
            for (int i = 0; i < points; ++i)
            {
                const float hz = 20.0f * std::pow(1000.0f, (float) i / (float) (points - 1));
                const float x = p.getX() + p.getWidth() * (float) i / (float) (points - 1);
                const float y = yForDb(filterMagnitudeDb(sig.ch[(size_t) c], hz));
                if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
            }
            juce::Path area(line);
            area.lineTo(p.getRight(), p.getBottom());
            area.lineTo(p.getX(), p.getBottom());
            area.closeSubPath();
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(c == 0 ? 0.34f : 0.16f), 0.0f, p.getY(),
                                                   colour.withAlpha(0.02f), 0.0f, p.getBottom(), false));
            g.fillPath(area);
            g.setColour(colour.withAlpha(c == 0 ? 1.0f : 0.8f));
            g.strokePath(line, juce::PathStrokeType(c == 0 ? 1.7f : 1.3f));
        }
    }

    static void paintRadar(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        drawWell(g, r, 5.0f);
        constexpr int axes = PresetSignature::kRadarAxes;
        const char* const names[axes] = { "BRIGHT", "RESO", "SWELL", "HOLD", "MOTION", "WIDE" };
        const auto c = r.getCentre().translated(0.0f, 1.0f);
        const float radius = 38.0f;
        const auto pointAt = [&](int axis, float amount)
        {
            const float a = -juce::MathConstants<float>::halfPi + (float) axis * juce::MathConstants<float>::twoPi / (float) axes;
            return juce::Point<float>(c.x + std::cos(a) * radius * amount, c.y + std::sin(a) * radius * amount);
        };
        for (int ring = 1; ring <= 3; ++ring)
        {
            juce::Path hex;
            for (int a = 0; a < axes; ++a)
            {
                const auto pt = pointAt(a, (float) ring / 3.0f);
                if (a == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
            }
            hex.closeSubPath();
            g.setColour(amber.withAlpha(ring == 3 ? 0.26f : 0.11f));
            g.strokePath(hex, juce::PathStrokeType(1.0f));
        }
        g.setColour(amber.withAlpha(0.12f));
        for (int a = 0; a < axes; ++a)
            g.drawLine({ c, pointAt(a, 1.0f) }, 1.0f);

        juce::Path shape;
        for (int a = 0; a < axes; ++a)
        {
            const auto pt = pointAt(a, std::max(0.07f, sig.radar[(size_t) a]));
            if (a == 0) shape.startNewSubPath(pt); else shape.lineTo(pt);
        }
        shape.closeSubPath();
        g.setGradientFill(juce::ColourGradient(amber.withAlpha(0.55f), c.x, c.y, amber.withAlpha(0.18f),
                                               c.x, c.y - radius, true));
        g.fillPath(shape);
        g.setColour(amberBright);
        g.strokePath(shape, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered));
        for (int a = 0; a < axes; ++a)
        {
            const auto pt = pointAt(a, std::max(0.07f, sig.radar[(size_t) a]));
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(pt.x - 2.8f, pt.y - 2.2f, 5.6f, 5.6f);
            g.setColour(amberBright.brighter(0.4f));
            g.fillEllipse(pt.x - 2.2f, pt.y - 2.6f, 4.4f, 4.4f);
        }
        const auto font = tracked(uiFont(7.5f, juce::Font::bold), 0.08f);
        for (int a = 0; a < axes; ++a)
        {
            const auto pt = pointAt(a, 1.0f);
            const float dx = pt.x - c.x, dy = pt.y - c.y;
            Rectangle<float> box(pt.x - 28.0f, pt.y - 6.0f, 56.0f, 12.0f);
            auto justification = juce::Justification::centred;
            if (dx > 4.0f)       { box = { pt.x + 6.0f, pt.y - 6.0f, 40.0f, 12.0f }; justification = juce::Justification::centredLeft; }
            else if (dx < -4.0f) { box = { pt.x - 46.0f, pt.y - 6.0f, 40.0f, 12.0f }; justification = juce::Justification::centredRight; }
            else                 box = box.translated(0.0f, dy < 0.0f ? -9.0f : 9.0f);
            legend(g, names[a], box, font, ivory.withAlpha(0.8f), justification);
        }
    }

    static void paintFacts(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        const auto& lead = sig.ch[(size_t) sig.dominant];
        juce::StringArray waves;
        if (lead.saw > 0.5f) waves.add("SAW");
        if (lead.pulse > 0.5f) waves.add("PULSE");
        if (lead.sine > 0.05f) waves.add("SINE");
        if (lead.noise > 0.01f) waves.add("NOISE");
        const struct { const char* name; juce::String value; } rows[] = {
            { "WAVES", waves.isEmpty() ? juce::String("--") : waves.joinIntoString(" + ") },
            { "CUTOFF", formatHertz(sig.cutoff) },
            { "RESONANCE", juce::String(juce::roundToInt(sig.resonance * 100.0f)) + " %" },
            { "ATTACK", formatTime(sig.attack) },
            { "RELEASE", formatTime(sig.release) },
            { "CHORUS", sig.chorus > 0.0f ? juce::String(juce::roundToInt(sig.chorus * 100.0f)) + " %" : juce::String("OFF") } };
        const float pitch = r.getHeight() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const Rectangle<float> row(r.getX(), r.getY() + pitch * (float) i, r.getWidth(), pitch);
            if (i > 0)
                etchLine(g, row.getX(), row.getY(), row.getRight(), row.getY(), 0.16f);
            caption(g, rows[i].name, row.withTrimmedRight(row.getWidth() * 0.5f), juce::Justification::centredLeft, 0.8f);
            legend(g, rows[i].value, row.withTrimmedLeft(row.getWidth() * 0.38f), uiFont(12.0f, juce::Font::bold),
                   ivory, juce::Justification::centredRight, true);
        }
    }

    PresetBrowser& owner;
};

// Slim custom scroll rail: a groove with a raised grip. Mirrors the list's own
// (hidden) scroll bar, which keeps the viewport maths.
class PresetBrowser::RailBar final : public juce::Component
{
public:
    RailBar()
    {
        setAccessible(false);
    }

    void setMetrics(double totalHeight, double visibleHeight, double position)
    {
        total = totalHeight;
        visible = visibleHeight;
        pos = position;
        setVisible(total > visible + 1.0);
        repaint();
    }

    std::function<void(double)> onDrag;
    std::function<void(double)> onPage;

    void paint(Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float w = std::min(8.0f * std::max(0.6f, uiScale), b.getWidth());
        const auto groove = Rectangle<float>(b.getCentreX() - w * 0.5f, b.getY(), w, b.getHeight());
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(groove, w * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawLine(groove.getRight(), groove.getY() + w, groove.getRight(), groove.getBottom() - w, 1.0f);
        const auto grip = thumb().withX(groove.getX() - 1.0f).withWidth(w + 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRoundedRectangle(grip.translated(0.0f, 1.5f), (w + 2.0f) * 0.5f);
        g.setGradientFill(juce::ColourGradient(Colour(dragging || hovering ? 0xffe8bd62 : 0xffc7a04d), grip.getX(), grip.getY(),
                                               Colour(0xff7d5a1f), grip.getRight(), grip.getY(), false));
        g.fillRoundedRectangle(grip, (w + 2.0f) * 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawLine(grip.getX() + 1.5f, grip.getY() + (w + 2.0f) * 0.5f, grip.getX() + 1.5f,
                   grip.getBottom() - (w + 2.0f) * 0.5f, 1.0f);
    }

    void mouseEnter(const juce::MouseEvent&) override { hovering = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hovering = false; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto t = thumb();
        if (t.contains(e.position))
        {
            dragging = true;
            dragOffset = e.position.y - t.getY();
            repaint();
        }
        else if (onPage != nullptr)
            onPage(e.position.y < t.getY() ? -visible * 0.9 : visible * 0.9);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (! dragging || onDrag == nullptr)
            return;
        const auto t = thumb();
        const double track = (double) getHeight() - t.getHeight();
        if (track <= 0.0)
            return;
        const double fraction = juce::jlimit(0.0, 1.0, ((double) e.position.y - dragOffset) / track);
        onDrag(fraction * (total - visible));
    }

    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }

    float uiScale = 1.0f;

private:
    Rectangle<float> thumb() const
    {
        const float h = (float) getHeight();
        const float len = (float) juce::jlimit(36.0, std::max(36.0, (double) h), h * visible / std::max(1.0, total));
        const float travel = h - len;
        const double range = std::max(1.0, total - visible);
        return { 0.0f, travel * (float) juce::jlimit(0.0, 1.0, pos / range), (float) getWidth(), len };
    }

    double total = 1.0, visible = 1.0, pos = 0.0, dragOffset = 0.0;
    bool dragging = false, hovering = false;
};

// Transparent hit area at the right of each card: the favourite star. It does
// not take any other clicks, so selecting and double-clicking a card still work.
class PresetBrowser::StarCell final : public juce::Component
{
public:
    explicit StarCell(PresetBrowser& o) : owner(o)
    {
        setAccessible(false);
    }

    int row = -1;

    bool hitTest(int x, int y) override { return starArea().contains(x, y); }

    void paint(Graphics& g) override
    {
        const int index = owner.entryIndexForRow(row);
        if (index < 0)
            return;
        const float u = owner.layout.s;
        const bool fav = owner.isFavourite(owner.entries[(size_t) index]);
        const bool selected = row == owner.selectedFilteredRow;
        const auto c = starArea().getCentre().toFloat();
        const float radius = 9.5f * u;
        if (fav)
        {
            g.setColour(amber.withAlpha(0.18f));
            g.fillEllipse(c.x - radius * 1.6f, c.y - radius * 1.6f, radius * 3.2f, radius * 3.2f);
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.55f), true, 1.0f);
            fillStar(g, c, radius, over ? amberBright.brighter(0.3f) : amberBright, true, 1.0f);
        }
        else
        {
            const float a = over ? 1.0f : (selected || owner.hoveredRow == row ? 0.62f : 0.30f);
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.5f), false, 1.4f * u);
            fillStar(g, c, radius, over ? amberBright : ivory.withAlpha(a), false, 1.4f * u);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { over = false; repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        const int index = owner.entryIndexForRow(row);
        if (index >= 0)
            owner.toggleFavourite(index);
    }

private:
    Rectangle<int> starArea() const
    {
        const float u = owner.layout.s;
        return Rectangle<float>((float) getWidth() - 46.0f * u, 0.0f, 40.0f * u, (float) getHeight()).toNearestInt();
    }

    PresetBrowser& owner;
    bool over = false;
};

// Tracks the pointer over the list and its rows for the hover lift. A plain
// mouse listener: unlike overriding the component's handlers it never sees the
// wheel, so scrolling is handled in exactly one place.
class PresetBrowser::HoverTracker final : public juce::MouseListener
{
public:
    explicit HoverTracker(PresetBrowser& o) : owner(o) {}

    void mouseMove(const juce::MouseEvent& e) override { update(e); }
    void mouseDrag(const juce::MouseEvent& e) override { update(e); }
    void mouseEnter(const juce::MouseEvent& e) override { update(e); }
    void mouseExit(const juce::MouseEvent&) override { setRow(-1); }

private:
    void update(const juce::MouseEvent& e)
    {
        if (owner.list == nullptr)
            return;
        const auto p = e.getEventRelativeTo(owner.list.get()).getPosition();
        const bool inside = owner.list->getLocalBounds().contains(p);
        setRow(inside ? owner.list->getRowContainingPosition(p.x, p.y) : -1);
    }

    void setRow(int row)
    {
        if (row == owner.hoveredRow)
            return;
        const int before = owner.hoveredRow;
        owner.hoveredRow = row;
        if (owner.list != nullptr)
        {
            if (before >= 0) owner.list->repaintRow(before);
            if (row >= 0) owner.list->repaintRow(row);
        }
    }

    PresetBrowser& owner;
};

// ===========================================================================
// PresetBrowser
// ===========================================================================
PresetBrowser::PresetBrowser(Cs80AudioProcessor& p) : processor(p)
{
    setOpaque(true);
    setTitle("Preset library");
    setWantsKeyboardFocus(false);

    surface = std::make_unique<Surface>(*this);
    legends = std::make_unique<Legends>(*this);
    addAndMakeVisible(*surface);
    addAndMakeVisible(*legends);

    // Header controls.
    search.setComponentID("presetSearch");
    search.setTitle("Search presets");
    search.setTooltip("Search names, categories, instrument types and style tags. Press Ctrl+F to jump here.");
    search.setTextToShowWhenEmpty("Search patches, instruments, character...", ivoryDim.withAlpha(0.6f));
    search.onTextChange = [this] { rebuildFilter(); };
    search.onSpecialKey = [this](const juce::KeyPress& key)
    {
        if (key.isKeyCode(juce::KeyPress::escapeKey) && search.getText().isNotEmpty())
        {
            search.setText({}, true);
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::returnKey))
        {
            loadSelected();
            return true;
        }
        const bool navigation = key.isKeyCode(juce::KeyPress::upKey) || key.isKeyCode(juce::KeyPress::downKey)
                                || key.isKeyCode(juce::KeyPress::pageUpKey) || key.isKeyCode(juce::KeyPress::pageDownKey);
        if (navigation && ! key.getModifiers().isAnyModifierKeyDown())
        {
            if (list->getSelectedRow() < 0 && ! filtered.empty())
                list->selectRow(0);
            else
                list->keyPressed(key);
            return true;
        }
        return false;
    };
    addAndMakeVisible(search);

    clearSearch.setTitle("Clear search");
    clearSearch.setTooltip("Clear the search box");
    clearSearch.setWantsKeyboardFocus(false);
    clearSearch.onClick = [this] { search.setText({}, true); search.grabKeyboardFocus(); };
    addChildComponent(clearSearch);

    for (int i = 0; i < kSortCount; ++i)
    {
        auto button = std::make_unique<PlateButton>(kSortNames[i], PlateButton::Kind::segment);
        button->setRadioGroupId(7002);
        button->setTitle(kSortTitles[i]);
        button->setTooltip(kSortTitles[i]);
        button->setToggleState(i == sortMode, juce::dontSendNotification);
        button->onClick = [this, i] { sortMode = i; rebuildFilter(); };
        addAndMakeVisible(*button);
        sortButtons.push_back(std::move(button));
    }

    closeButton.setTitle("CLOSE");
    closeButton.setTooltip("Close the library (Esc)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    buildFilterChips();

    resetButton.setTitle("Clear all filters");
    resetButton.onClick = [this] { clearFilters(); };
    addAndMakeVisible(resetButton);
    emptyResetButton.setTitle("Clear all filters and search");
    emptyResetButton.onClick = [this] { clearFilters(); };
    addChildComponent(emptyResetButton);

    // Results.
    list = std::make_unique<ResultList>();
    list->setModel(this);
    list->setMultipleSelectionEnabled(false);
    list->setOutlineThickness(0);
    list->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list->setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    list->setWantsKeyboardFocus(true);
    list->setRowHeight(66);
    list->getViewport()->setScrollBarsShown(false, false); // the custom rail replaces it
    list->getVerticalScrollBar().addListener(this);
    list->onFocusChange = [this] { legends->repaintWell(); };
    list->onExtraKey = [this](const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::spaceKey)
        {
            const int index = entryIndexForRow(selectedFilteredRow);
            if (index >= 0)
                toggleFavourite(index);
            return true;
        }
        const auto c = key.getTextCharacter();
        if (c >= 33 && c != 127 && ! key.getModifiers().isCtrlDown() && ! key.getModifiers().isCommandDown()
            && ! key.getModifiers().isAltDown())
        {
            search.grabKeyboardFocus();
            search.insertTextAtCaret(juce::String::charToString(c));
            return true;
        }
        return false;
    };
    addAndMakeVisible(*list);
    hoverTracker = std::make_unique<HoverTracker>(*this);
    list->addMouseListener(hoverTracker.get(), true);

    rail = std::make_unique<RailBar>();
    rail->onDrag = [this](double y)
    {
        stopTimer();
        list->getViewport()->setViewPosition(0, juce::roundToInt(y));
    };
    rail->onPage = [this](double delta)
    {
        smoothScrollTo((isTimerRunning() ? scrollTarget : (double) list->getViewport()->getViewPositionY()) + delta);
    };
    addChildComponent(*rail);

    // Inspector.
    favouriteButton.setTitle("Favourite");
    favouriteButton.setTooltip("Add or remove this patch from your favourites (session only). Space toggles it in the list.");
    favouriteButton.onClick = [this]
    {
        const int index = entryIndexForRow(selectedFilteredRow);
        if (index >= 0)
            toggleFavourite(index);
    };
    addAndMakeVisible(favouriteButton);

    previousButton.setTitle("Previous patch");
    previousButton.setTooltip("Select the previous patch in the list");
    previousButton.onClick = [this] { stepSelection(-1); };
    addAndMakeVisible(previousButton);
    nextButton.setTitle("Next patch");
    nextButton.setTooltip("Select the next patch in the list");
    nextButton.onClick = [this] { stepSelection(1); };
    addAndMakeVisible(nextButton);

    description.setJustificationType(juce::Justification::topLeft);
    description.setColour(juce::Label::textColourId, ivoryDim);
    description.setMinimumHorizontalScale(0.85f);
    description.setBorderSize({ 0, 0, 0, 0 });
    description.setInterceptsMouseClicks(false, false);
    description.setTitle("Patch description");
    addAndMakeVisible(description);

    loadButton.setTitle("LOAD");
    loadButton.setTooltip("Load the selected patch (Enter or double-click)");
    loadButton.onClick = [this] { loadSelected(); };
    addAndMakeVisible(loadButton);
    deleteButton.setTitle("DELETE");
    deleteButton.setTooltip("Move the selected user patch to the trash (Delete)");
    deleteButton.onClick = [this] { deleteSelected(); };
    addAndMakeVisible(deleteButton);

    saveName.setTextToShowWhenEmpty("New user patch name", ivoryDim.withAlpha(0.6f));
    saveName.setText("User Patch", false);
    saveName.setComponentID("presetSaveName");
    saveName.setTitle("Name for the saved user patch");
    saveName.onSpecialKey = [this](const juce::KeyPress& key)
    {
        if (key.isKeyCode(juce::KeyPress::returnKey))
        {
            saveButton.onClick();
            return true;
        }
        return false;
    };
    addAndMakeVisible(saveName);

    saveButton.setTitle("SAVE USER");
    saveButton.setTooltip("Save the current sound as a user patch with the name above");
    saveButton.onClick = [this]
    {
        auto name = saveName.getText().trim();
        if (name.isEmpty())
            name = processor.getPresetName() + " Edit";
        saveCurrentPreset(name);
    };
    addAndMakeVisible(saveButton);

    saveStatus.setComponentID("presetSaveStatus");
    saveStatus.setTitle("Save status");
    saveStatus.setJustificationType(juce::Justification::topLeft);
    saveStatus.setColour(juce::Label::textColourId, ivoryDim);
    saveStatus.setMinimumHorizontalScale(0.85f);
    saveStatus.setBorderSize({ 0, 0, 0, 0 });
    saveStatus.setInterceptsMouseClicks(true, false);
    addAndMakeVisible(saveStatus);

    // Explicit focus order: search, sort, filters, list, inspector, close.
    int order = 1;
    search.setExplicitFocusOrder(order++);
    for (auto& b : sortButtons) b->setExplicitFocusOrder(order++);
    for (auto& b : scopeChips) b->setExplicitFocusOrder(order++);
    for (auto& b : typeChips) b->setExplicitFocusOrder(order++);
    for (auto& b : styleChips) b->setExplicitFocusOrder(order++);
    resetButton.setExplicitFocusOrder(order++);
    list->setExplicitFocusOrder(order++);
    favouriteButton.setExplicitFocusOrder(order++);
    previousButton.setExplicitFocusOrder(order++);
    nextButton.setExplicitFocusOrder(order++);
    loadButton.setExplicitFocusOrder(order++);
    deleteButton.setExplicitFocusOrder(order++);
    saveName.setExplicitFocusOrder(order++);
    saveButton.setExplicitFocusOrder(order++);
    closeButton.setExplicitFocusOrder(order++);

    updateSelectionWidgets();
    refresh();
}

PresetBrowser::~PresetBrowser()
{
    stopTimer();
    if (list != nullptr)
    {
        list->getVerticalScrollBar().removeListener(this);
        list->removeMouseListener(hoverTracker.get());
        list->setModel(nullptr);
    }
}

void PresetBrowser::paint(Graphics& g)
{
    g.fillAll(Colour(0xff0a0908));
}

void PresetBrowser::buildFilterChips()
{
    for (int i = 0; i < kScopeCount; ++i)
    {
        auto chip = std::make_unique<ChipButton>(kScopeNames[i], i == 1 ? amberBright : amber);
        chip->setRadioGroupId(7001);
        chip->setStarGlyph(i == 1);
        chip->setTitle(juce::String("Show ") + kScopeNames[i]);
        chip->setToggleState(i == scopeFilter, juce::dontSendNotification);
        chip->onClick = [this, i] { scopeFilter = i; rebuildFilter(); };
        addAndMakeVisible(*chip);
        scopeChips.push_back(std::move(chip));
    }
    for (int t = 0; t < typeCount; ++t)
    {
        auto chip = std::make_unique<ChipButton>(kTypes[t].label, typeColour(t));
        chip->setTitle(juce::String("Instrument type ") + kTypes[t].label);
        chip->onClick = [this, t, raw = chip.get()]
        {
            if (raw->getToggleState()) typeFilter |= 1u << (unsigned) t;
            else typeFilter &= ~(1u << (unsigned) t);
            rebuildFilter();
        };
        addChildComponent(*chip);
        typeChips.push_back(std::move(chip));
    }
    for (int s = 0; s < styleCount; ++s)
    {
        auto chip = std::make_unique<ChipButton>(kStyleNames[s], amber);
        chip->setTitle(juce::String("Style ") + kStyleNames[s]);
        chip->onClick = [this, s, raw = chip.get()]
        {
            if (raw->getToggleState()) styleFilter |= 1u << (unsigned) s;
            else styleFilter &= ~(1u << (unsigned) s);
            rebuildFilter();
        };
        addChildComponent(*chip);
        styleChips.push_back(std::move(chip));
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void PresetBrowser::resized()
{
    layoutAll();
}

void PresetBrowser::layoutAll()
{
    const int pw = getWidth(), ph = getHeight();
    if (pw <= 0 || ph <= 0)
        return;

    auto& L = layout;
    L.s = juce::jmin((float) pw / 1600.0f, (float) ph / 936.0f);
    uiScale = L.s;
    const float s = L.s;
    L.width = (float) pw / s;
    L.height = (float) ph / s;
    const float W = L.width, H = L.height;

    const auto px = [s](float x, float y, float w, float h)
    {
        return Rectangle<int>(juce::roundToInt(x * s), juce::roundToInt(y * s),
                              juce::roundToInt(w * s), juce::roundToInt(h * s));
    };
    const auto pxr = [&px](Rectangle<float> r) { return px(r.getX(), r.getY(), r.getWidth(), r.getHeight()); };

    L.face = { 44.0f, 14.0f, W - 88.0f, H - 28.0f };
    L.header = { 44.0f, 14.0f, W - 88.0f, 84.0f };
    const float top = 114.0f, bottom = H - 32.0f;
    L.leftPlate = { 66.0f, top, 300.0f, bottom - top };
    L.detail = { W - 66.0f - 420.0f, top, 420.0f, bottom - top };
    const float resultsX = L.leftPlate.getRight() + 14.0f;
    L.results = { resultsX, top, L.detail.getX() - 14.0f - resultsX, bottom - top };
    L.strip = { L.results.getX() + 16.0f, top + 18.0f, L.results.getWidth() - 32.0f, 28.0f };
    L.well = { L.results.getX() + 14.0f, top + 56.0f, L.results.getWidth() - 28.0f, bottom - top - 56.0f - 14.0f };

    const float closeW = 112.0f;
    const float closeX = W - 66.0f - closeW;
    L.sortStrip = { closeX - 28.0f - 5.0f * 62.0f, 40.0f, 5.0f * 62.0f, 36.0f };
    L.searchRect = { L.results.getX() + 14.0f, 32.0f, L.sortStrip.getX() - 30.0f - (L.results.getX() + 14.0f), 48.0f };

    // Header controls.
    closeButton.setUiScale(s);
    closeButton.setBounds(px(closeX, 36.0f, closeW, 44.0f));
    for (int i = 0; i < kSortCount; ++i)
    {
        sortButtons[(size_t) i]->setUiScale(s);
        sortButtons[(size_t) i]->setBounds(px(L.sortStrip.getX() + 62.0f * (float) i, L.sortStrip.getY(), 62.0f, L.sortStrip.getHeight()));
    }
    search.setUiScale(s);
    search.setBounds(pxr(L.searchRect));
    search.setFont(uiFont(16.0f * s));
    search.applyFontToAllText(uiFont(16.0f * s));
    search.setIndents(juce::roundToInt(44.0f * s), 0);
    search.setBorder(juce::BorderSize<int>(0, 0, 0, juce::roundToInt(44.0f * s)));
    clearSearch.setUiScale(s);
    clearSearch.setBounds(px(L.searchRect.getRight() - 42.0f, L.searchRect.getY() + 6.0f, 36.0f, L.searchRect.getHeight() - 12.0f));

    // Filter column. Chips are packed top to bottom; absent types and styles
    // collapse so the column never shows dead tabs.
    const float cx = L.leftPlate.getX() + 20.0f, cw = L.leftPlate.getWidth() - 40.0f;
    float y = L.leftPlate.getY() + 26.0f;
    L.scopeCaptionY = y;
    y += 26.0f;
    for (int i = 0; i < kScopeCount; ++i)
    {
        scopeChips[(size_t) i]->setUiScale(s);
        scopeChips[(size_t) i]->setBounds(px(cx, y, cw, 31.0f));
        y += 35.0f;
    }
    y += 12.0f;
    const float colW = (cw - 6.0f) / 2.0f;
    int shown = 0;
    L.typeCaptionY = 0.0f;
    for (int t = 0; t < typeCount; ++t)
        if (typePresent & (1u << (unsigned) t))
            ++shown;
    if (shown > 0)
    {
        L.typeCaptionY = y;
        y += 26.0f;
        int slot = 0;
        for (int t = 0; t < typeCount; ++t)
        {
            auto& chip = *typeChips[(size_t) t];
            const bool present = (typePresent & (1u << (unsigned) t)) != 0;
            chip.setVisible(present);
            if (! present)
                continue;
            chip.setUiScale(s);
            chip.setBounds(px(cx + (float) (slot % 2) * (colW + 6.0f), y + (float) (slot / 2) * 35.0f, colW, 31.0f));
            ++slot;
        }
        y += (float) ((slot + 1) / 2) * 35.0f + 12.0f;
    }
    else
        for (auto& chip : typeChips) chip->setVisible(false);

    shown = 0;
    L.styleCaptionY = 0.0f;
    for (int t = 0; t < styleCount; ++t)
        if (stylePresent & (1u << (unsigned) t))
            ++shown;
    if (shown > 0)
    {
        L.styleCaptionY = y;
        y += 26.0f;
        int slot = 0;
        for (int t = 0; t < styleCount; ++t)
        {
            auto& chip = *styleChips[(size_t) t];
            const bool present = (stylePresent & (1u << (unsigned) t)) != 0;
            chip.setVisible(present);
            if (! present)
                continue;
            chip.setUiScale(s);
            chip.setBounds(px(cx + (float) (slot % 2) * (colW + 6.0f), y + (float) (slot / 2) * 35.0f, colW, 31.0f));
            ++slot;
        }
    }
    else
        for (auto& chip : styleChips) chip->setVisible(false);
    L.chipsBottom = y + (float) ((shown + 1) / 2) * 35.0f;

    resetButton.setUiScale(s);
    resetButton.setBounds(px(cx, L.leftPlate.getBottom() - 20.0f - 38.0f, cw, 38.0f));

    // Results.
    const int rowHeight = juce::jmax(24, juce::roundToInt(66.0f * s));
    list->setRowHeight(rowHeight);
    const float railW = 14.0f;
    list->setBounds(px(L.well.getX() + 8.0f, L.well.getY() + 8.0f, L.well.getWidth() - 16.0f - railW - 4.0f, L.well.getHeight() - 16.0f));
    rail->uiScale = s;
    rail->setBounds(px(L.well.getRight() - 8.0f - railW, L.well.getY() + 14.0f, railW, L.well.getHeight() - 28.0f));
    emptyResetButton.setUiScale(s);
    emptyResetButton.setBounds(px(L.well.getCentreX() - 110.0f, L.well.getCentreY() + 18.0f, 220.0f, 40.0f));
    emptyResetButton.setVisible(filtered.empty());

    // Inspector controls.
    const float dx = L.detail.getX() + 22.0f, dy = L.detail.getY(), dw = L.detail.getWidth() - 44.0f;
    favouriteButton.setUiScale(s);
    favouriteButton.setBounds(px(dx + dw - 36.0f, dy + 10.0f, 36.0f, 34.0f));
    description.setBounds(px(dx, dy + 150.0f, dw, 58.0f));
    description.setFont(uiFont(12.5f * s));
    previousButton.setUiScale(s);
    previousButton.setBounds(px(dx, dy + 602.0f, 108.0f, 38.0f));
    nextButton.setUiScale(s);
    nextButton.setBounds(px(dx + dw - 108.0f, dy + 602.0f, 108.0f, 38.0f));
    loadButton.setUiScale(s);
    loadButton.setBounds(px(dx, dy + 650.0f, dw - 124.0f, 48.0f));
    deleteButton.setUiScale(s);
    deleteButton.setBounds(px(dx + dw - 112.0f, dy + 650.0f, 112.0f, 48.0f));
    saveName.setUiScale(s);
    saveName.setBounds(px(dx, dy + 724.0f, dw - 124.0f, 36.0f));
    saveName.setFont(uiFont(14.0f * s));
    saveName.applyFontToAllText(uiFont(14.0f * s));
    saveName.setIndents(juce::roundToInt(11.0f * s), 0);
    saveName.setBorder(juce::BorderSize<int>(0, 0, 0, 4));
    saveButton.setUiScale(s);
    saveButton.setBounds(px(dx + dw - 112.0f, dy + 722.0f, 112.0f, 40.0f));
    saveStatus.setBounds(px(dx, dy + 766.0f, dw, 26.0f));
    saveStatus.setFont(uiFont(10.5f * s));

    surface->setBounds(getLocalBounds());
    legends->setBounds(getLocalBounds());
    surface->repaint();
    legends->repaint();
    updateRail();
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------
void PresetBrowser::refresh()
{
    entries.clear();
    const auto& bank = processor.getFactoryPresets();
    entries.reserve(bank.size() + 32);

    // Factory signatures never change; compute them once per process.
    static const std::vector<std::shared_ptr<const PresetSignature>> factorySignatures = [&bank]
    {
        std::vector<std::shared_ptr<const PresetSignature>> all;
        all.reserve(bank.size());
        for (const auto& preset : bank)
            all.push_back(std::make_shared<const PresetSignature>(computeSignature(preset.values)));
        return all;
    }();

    const auto finish = [](Entry& e)
    {
        e.type = classifyType(e.isUser ? juce::String("User") : e.category, e.name);
        e.styles = e.signature != nullptr ? e.signature->styles : 0u;
        juce::String styleText;
        for (int t = 0; t < styleCount; ++t)
            if (e.styles & (1u << (unsigned) t))
                styleText << " " << kStyleNames[t];
        e.haystack = (e.name + " " + e.category + " " + kTypes[e.type].label + styleText + " " + e.description
                      + (e.isUser ? " user" : " factory")).toLowerCase();
    };

    for (int i = 0; i < (int) bank.size(); ++i)
    {
        const auto& preset = bank[(size_t) i];
        Entry e;
        e.name = juce::String(preset.name);
        e.category = juce::String(preset.category);
        e.description = juce::String(preset.description);
        e.factoryIndex = i;
        e.key = "F:" + e.name;
        e.slot = juce::String(i + 1).paddedLeft('0', 3);
        if ((size_t) i < factorySignatures.size())
            e.signature = factorySignatures[(size_t) i];
        finish(e);
        entries.push_back(std::move(e));
    }

    auto files = processor.getUserPresetFiles();
    files.sort();
    int userNumber = 0;
    for (const auto& file : files)
    {
        Entry e;
        e.name = file.getFileNameWithoutExtension();
        e.category = "User";
        e.description = "User patch stored in the CS-80 Reference preset folder.";
        e.userFile = file;
        e.isUser = true;
        e.key = "U:" + file.getFileName();
        e.slot = "U" + juce::String(++userNumber).paddedLeft('0', 2);
        e.modified = file.getLastModificationTime().toMilliseconds();
        e.signature = signatureForUserFile(file);
        finish(e);
        entries.push_back(std::move(e));
    }

    typePresent = 0;
    stylePresent = 0;
    for (const auto& e : entries)
    {
        typePresent |= 1u << (unsigned) e.type;
        stylePresent |= e.styles;
    }
    typeFilter &= typePresent;
    styleFilter &= stylePresent;
    for (int t = 0; t < typeCount; ++t)
        typeChips[(size_t) t]->setToggleState((typeFilter & (1u << (unsigned) t)) != 0, juce::dontSendNotification);
    for (int t = 0; t < styleCount; ++t)
        styleChips[(size_t) t]->setToggleState((styleFilter & (1u << (unsigned) t)) != 0, juce::dontSendNotification);

    layoutAll();
    rebuildFilter();
    selectCurrentPreset();
}

bool PresetBrowser::isFavourite(const Entry& e) const
{
    return favouriteKeys().contains(e.key);
}

int PresetBrowser::entryIndexForRow(int row) const
{
    return row >= 0 && row < (int) filtered.size() ? filtered[(size_t) row] : -1;
}

const PresetBrowser::Entry* PresetBrowser::selectedEntry() const
{
    const int index = entryIndexForRow(selectedFilteredRow);
    return index >= 0 ? &entries[(size_t) index] : nullptr;
}

bool PresetBrowser::anyFilterActive() const
{
    return scopeFilter != 0 || typeFilter != 0 || styleFilter != 0 || search.getText().trim().isNotEmpty();
}

void PresetBrowser::clearFilters()
{
    scopeFilter = 0;
    typeFilter = 0;
    styleFilter = 0;
    search.setText({}, false);
    rebuildFilter();
}

namespace
{
bool scopeAccepts(int scope, bool favourite, bool isUser, bool hardware)
{
    switch (scope)
    {
        case 1: return favourite;
        case 2: return ! isUser && ! hardware;
        case 3: return hardware;
        case 4: return isUser;
        default: return true;
    }
}
} // namespace

void PresetBrowser::rebuildFilter()
{
    const juce::String previousKey = selectedEntry() != nullptr ? selectedEntry()->key : juce::String();

    juce::StringArray tokens;
    tokens.addTokens(search.getText().trim().toLowerCase(), " ", "");
    tokens.removeEmptyStrings();

    // One predicate with overrides so the facet counts on the chips can ask
    // "what would I get if this chip were on" without a second code path.
    const auto accepts = [&](const Entry& e, int scope, std::uint32_t types, std::uint32_t styles)
    {
        if (! scopeAccepts(scope, isFavourite(e), e.isUser, ! e.isUser && e.category == "Hardware Studies"))
            return false;
        if (types != 0 && (types & (1u << (unsigned) e.type)) == 0)
            return false;
        if ((e.styles & styles) != styles)
            return false;
        for (const auto& token : tokens)
            if (! e.haystack.contains(token))
                return false;
        return true;
    };

    filtered.clear();
    for (int i = 0; i < (int) entries.size(); ++i)
        if (accepts(entries[(size_t) i], scopeFilter, typeFilter, styleFilter))
            filtered.push_back(i);

    const auto byName = [this](int a, int b)
    {
        return entries[(size_t) a].name.compareNatural(entries[(size_t) b].name) < 0;
    };
    switch (sortMode)
    {
        case 1: std::stable_sort(filtered.begin(), filtered.end(), byName); break;
        case 2: std::stable_sort(filtered.begin(), filtered.end(),
                                 [this](int a, int b) { return entries[(size_t) a].name.compareNatural(entries[(size_t) b].name) > 0; });
                break;
        case 3: std::stable_sort(filtered.begin(), filtered.end(), [&](int a, int b)
                {
                    const auto& x = entries[(size_t) a];
                    const auto& y = entries[(size_t) b];
                    return x.type != y.type ? x.type < y.type : x.name.compareNatural(y.name) < 0;
                });
                break;
        case 4: std::stable_sort(filtered.begin(), filtered.end(), [this](int a, int b)
                {
                    const auto& x = entries[(size_t) a];
                    const auto& y = entries[(size_t) b];
                    if (x.isUser != y.isUser) return x.isUser;
                    return x.isUser && x.modified > y.modified;
                });
                break;
        default: break;
    }

    // Facet counts: each chip shows how many patches it would give right now.
    for (int i = 0; i < kScopeCount; ++i)
    {
        int n = 0;
        for (const auto& e : entries)
            if (accepts(e, i, typeFilter, styleFilter))
                ++n;
        scopeChips[(size_t) i]->setCount(n);
        scopeChips[(size_t) i]->setToggleState(i == scopeFilter, juce::dontSendNotification);
    }
    for (int t = 0; t < typeCount; ++t)
    {
        int n = 0;
        for (const auto& e : entries)
            if (accepts(e, scopeFilter, 1u << (unsigned) t, styleFilter))
                ++n;
        typeChips[(size_t) t]->setCount(n);
        typeChips[(size_t) t]->setToggleState((typeFilter & (1u << (unsigned) t)) != 0, juce::dontSendNotification);
    }
    for (int t = 0; t < styleCount; ++t)
    {
        int n = 0;
        for (const auto& e : entries)
            if (accepts(e, scopeFilter, typeFilter, styleFilter | (1u << (unsigned) t)))
                ++n;
        styleChips[(size_t) t]->setCount(n);
        styleChips[(size_t) t]->setToggleState((styleFilter & (1u << (unsigned) t)) != 0, juce::dontSendNotification);
    }
    for (int i = 0; i < kSortCount; ++i)
        sortButtons[(size_t) i]->setToggleState(i == sortMode, juce::dontSendNotification);
    resetButton.setEnabled(anyFilterActive());
    clearSearch.setVisible(search.getText().isNotEmpty());
    emptyResetButton.setVisible(filtered.empty());

    // Keep the selection across filtering when the patch is still listed;
    // while searching, the top hit is selected so Enter loads it.
    restoringSelection = true;
    selectedFilteredRow = -1;
    hoveredRow = -1;
    list->updateContent();
    list->deselectAllRows();
    int restore = -1;
    for (int row = 0; row < (int) filtered.size() && previousKey.isNotEmpty(); ++row)
        if (entries[(size_t) filtered[(size_t) row]].key == previousKey)
        {
            restore = row;
            break;
        }
    if (restore < 0 && ! tokens.isEmpty() && ! filtered.empty())
        restore = 0;
    if (restore >= 0)
        list->selectRow(restore, false, true);
    else
        list->getViewport()->setViewPosition(0, 0);
    restoringSelection = false;
    selectedFilteredRow = list->getSelectedRow();

    list->repaint();
    updateSelectionWidgets();
    updateRail();
    legends->repaintFilters();
    legends->repaintStrip();
    legends->repaintWell();
}

void PresetBrowser::updateSelectionWidgets()
{
    const auto* e = selectedEntry();
    loadButton.setEnabled(e != nullptr);
    deleteButton.setEnabled(e != nullptr && e->isUser && ! deleteConfirmationPending);
    favouriteButton.setEnabled(e != nullptr);
    favouriteButton.setToggleState(e != nullptr && isFavourite(*e), juce::dontSendNotification);
    previousButton.setEnabled(selectedFilteredRow > 0);
    nextButton.setEnabled(selectedFilteredRow >= 0 && selectedFilteredRow + 1 < (int) filtered.size());
    if (e != nullptr)
        description.setText(e->description, juce::dontSendNotification);
    else
        description.setText("Select a patch to inspect it. Double-click or press LOAD to audition.",
                            juce::dontSendNotification);
    description.setVisible(e != nullptr);
    legends->repaintDetail();
}

void PresetBrowser::setStatus(const juce::String& text, int tone)
{
    saveStatus.setText(text, juce::dontSendNotification);
    saveStatus.setColour(juce::Label::textColourId,
                         tone > 0 ? Colour(0xff7fd99a) : (tone < 0 ? Colour(0xffff9d89) : ivoryDim));
}

// ---------------------------------------------------------------------------
// ListBoxModel
// ---------------------------------------------------------------------------
int PresetBrowser::getNumRows()
{
    return (int) filtered.size();
}

juce::String PresetBrowser::getNameForRow(int rowNumber)
{
    const int index = entryIndexForRow(rowNumber);
    if (index < 0)
        return {};
    const auto& e = entries[(size_t) index];
    return e.name + ", " + juce::String(kTypes[e.type].label).toLowerCase() + ", "
           + (e.isUser ? "user" : "factory") + (isFavourite(e) ? ", favourite" : "");
}

juce::Component* PresetBrowser::refreshComponentForRow(int rowNumber, bool, juce::Component* existing)
{
    if (rowNumber < 0 || rowNumber >= (int) filtered.size())
    {
        delete existing;
        return nullptr;
    }
    auto* cell = dynamic_cast<StarCell*>(existing);
    if (cell == nullptr)
    {
        delete existing;
        cell = new StarCell(*this);
    }
    cell->row = rowNumber;
    cell->repaint();
    return cell;
}

void PresetBrowser::paintListBoxItem(int rowNumber, Graphics& g, int width, int height, bool selected)
{
    const int index = entryIndexForRow(rowNumber);
    if (index < 0 || layout.s <= 0.0f)
        return;
    const auto& e = entries[(size_t) index];
    const float s = layout.s;
    g.addTransform(juce::AffineTransform::scale(s));
    const float W = (float) width / s, H = (float) height / s;
    const bool hover = rowNumber == hoveredRow;
    const auto tc = typeColour(e.type);

    // Raised glass card with a cast shadow; amber glow and lamp when selected.
    const Rectangle<float> card(6.0f, 4.0f, W - 12.0f, H - 8.0f);
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillRoundedRectangle(card.translated(0.0f, 3.0f).expanded(2.0f, 1.0f), 8.0f);
    g.setColour(juce::Colours::black.withAlpha(0.40f));
    g.fillRoundedRectangle(card.translated(0.0f, 1.8f).expanded(0.8f, 0.4f), 7.0f);
    if (selected)
    {
        g.setColour(amber.withAlpha(0.10f));
        g.fillRoundedRectangle(card.expanded(3.0f), 9.0f);
        g.setColour(amber.withAlpha(0.18f));
        g.fillRoundedRectangle(card.expanded(1.4f), 7.5f);
    }
    Colour top = selected ? Colour(0xff5d4622) : (hover ? Colour(0xff4a463f) : Colour(0xff3b3833));
    Colour bottom = selected ? Colour(0xff33260f) : (hover ? Colour(0xff2e2b27) : Colour(0xff252320));
    g.setGradientFill(juce::ColourGradient(top, 0.0f, card.getY(), bottom, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle(card, 6.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(selected ? 0.10f : 0.075f), card.getX(), card.getY(),
                                           juce::Colours::transparentWhite, card.getX(), card.getY() + card.getHeight() * 0.55f, false));
    g.fillRoundedRectangle(card, 6.0f);
    g.setColour(juce::Colours::white.withAlpha(selected ? 0.34f : 0.17f));
    g.drawLine(card.getX() + 6.0f, card.getY() + 0.8f, card.getRight() - 6.0f, card.getY() + 0.8f, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawLine(card.getX() + 0.8f, card.getY() + 6.0f, card.getX() + 0.8f, card.getBottom() - 6.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawLine(card.getX() + 6.0f, card.getBottom() - 0.8f, card.getRight() - 6.0f, card.getBottom() - 0.8f, 1.0f);
    g.setColour(selected ? amberBright.withAlpha(0.9f) : juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(card, 6.0f, selected ? 1.4f : 1.0f);

    // Selection lamp in a slot at the left edge.
    const Rectangle<float> lamp(card.getX() + 7.0f, card.getY() + 9.0f, 5.0f, card.getHeight() - 18.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(lamp.expanded(1.0f), 3.0f);
    if (selected)
    {
        g.setColour(amber.withAlpha(0.35f));
        g.fillRoundedRectangle(lamp.expanded(2.5f), 4.0f);
        g.setColour(amberBright);
        g.fillRoundedRectangle(lamp, 2.5f);
    }
    else
    {
        g.setColour(tc.withAlpha(hover ? 0.55f : 0.28f));
        g.fillRoundedRectangle(lamp, 2.5f);
    }

    // Slot number.
    g.setFont(monoFont(11.0f));
    g.setColour((selected ? amberBright : ivoryDim).withAlpha(selected ? 0.95f : 0.7f));
    g.drawText(e.slot, Rectangle<float>(card.getX() + 20.0f, card.getY(), 38.0f, card.getHeight()),
               juce::Justification::centredLeft, false);

    // Mini signature window.
    if (e.signature != nullptr)
        drawMiniSignature(g, { card.getX() + 62.0f, card.getY() + 9.0f, 100.0f, card.getHeight() - 18.0f }, *e.signature, selected);

    // Right cluster: type tag, source label; the star is a separate cell.
    const float pillH = 20.0f;
    const auto pillFont = tracked(uiFont(10.0f, juce::Font::bold), 0.1f);
    const float pillW = std::max(78.0f, textWidth(pillFont, kTypes[e.type].label) + 20.0f);
    const float pillX = card.getRight() - 52.0f - pillW;
    drawPill(g, pillX, card.getY() + 10.0f, pillH, kTypes[e.type].label, tc, selected || hover, pillW);
    legend(g, e.isUser ? "USER" : (e.category == "Hardware Studies" ? "HARDWARE" : "FACTORY"),
           { pillX, card.getY() + 33.0f, pillW, 14.0f }, tracked(uiFont(8.5f, juce::Font::bold), 0.14f),
           e.isUser ? teal.brighter(0.5f) : ivoryDim.withAlpha(0.8f), juce::Justification::centred);

    // Name and description.
    const float textX = card.getX() + 176.0f;
    const float textW = pillX - 14.0f - textX;
    if (textW > 20.0f)
    {
        legend(g, e.name, { textX, card.getY() + 7.0f, textW, 24.0f },
               fitFont(uiFont(17.0f, juce::Font::bold), e.name, textW),
               selected ? Colour(0xfffff1cc) : ivory, juce::Justification::centredLeft, true);
        g.setFont(uiFont(11.5f));
        g.setColour(ivoryDim.withAlpha(selected ? 0.95f : 0.8f));
        g.drawText(e.description, Rectangle<float>(textX, card.getY() + 31.0f, textW, 18.0f),
                   juce::Justification::centredLeft, true);
    }
}

void PresetBrowser::selectedRowsChanged(int lastRowSelected)
{
    selectedFilteredRow = lastRowSelected;
    if (const auto* entry = selectedEntry())
    {
        if (! restoringSelection)
            saveName.setText(entry->name, false);
    }
    updateSelectionWidgets();
    list->repaint();
}

void PresetBrowser::listBoxItemDoubleClicked(int, const juce::MouseEvent&)
{
    loadSelected();
}

void PresetBrowser::returnKeyPressed(int)
{
    loadSelected();
}

void PresetBrowser::deleteKeyPressed(int)
{
    deleteSelected();
}

void PresetBrowser::stepSelection(int delta)
{
    if (filtered.empty())
        return;
    const int next = juce::jlimit(0, (int) filtered.size() - 1,
                                  (selectedFilteredRow < 0 ? (delta > 0 ? -1 : (int) filtered.size()) : selectedFilteredRow) + delta);
    list->selectRow(next, false, true);
}

void PresetBrowser::toggleFavourite(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= (int) entries.size())
        return;
    const auto& key = entries[(size_t) entryIndex].key;
    if (favouriteKeys().contains(key))
        favouriteKeys().removeString(key);
    else
        favouriteKeys().add(key);
    rebuildFilter(); // counts, the favourites scope, and the card stars
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void PresetBrowser::loadSelected()
{
    const auto* entry = selectedEntry();
    if (entry == nullptr)
        return;

    if (entry->isUser)
        processor.loadUserPreset(entry->userFile);
    else if (entry->factoryIndex >= 0)
        processor.loadFactoryPreset(entry->factoryIndex);

    if (onPresetLoaded)
        onPresetLoaded();
}

void PresetBrowser::deleteSelected()
{
    const auto* entry = selectedEntry();
    if (entry == nullptr || ! entry->isUser || deleteConfirmationPending)
        return;

    const auto file = entry->userFile;
    const auto name = entry->name;
    deleteConfirmationPending = true;
    deleteButton.setEnabled(false);
    deleteDialog = juce::AlertWindow::showScopedAsync(
        juce::MessageBoxOptions::makeOptionsOkCancel(juce::MessageBoxIconType::WarningIcon,
            "Delete user preset?",
            "Delete \"" + name + "\"?\n\nThe preset file will be moved to the trash:\n\n"
                + file.getFullPathName(),
            "Delete", "Cancel", this),
        [safeThis = juce::Component::SafePointer<PresetBrowser>(this), file, name](int choice)
        {
            if (safeThis == nullptr)
                return;
            safeThis->deleteConfirmationPending = false;
            if (choice == 1)
            {
                const bool removed = file.moveToTrash() || file.deleteFile();
                safeThis->refresh();
                safeThis->setStatus(removed ? "Deleted \"" + name + "\"." : "Could not delete \"" + name + "\".",
                                    removed ? 1 : -1);
            }
            else
            {
                safeThis->updateSelectionWidgets();
                safeThis->setStatus("Delete cancelled. The preset is unchanged.", 0);
            }
        });
}

void PresetBrowser::saveCurrentPreset(const juce::String& name, bool overwrite)
{
    if (overwriteConfirmationPending)
        return;

    const auto result = processor.saveUserPreset(name, overwrite
        ? Cs80AudioProcessor::PresetOverwrite::yes : Cs80AudioProcessor::PresetOverwrite::no);
    saveStatus.setTooltip(result.file.getFullPathName());
    if (result.wasSaved())
    {
        refresh();
        const auto findRow = [&]
        {
            for (int row = 0; row < (int) filtered.size(); ++row)
                if (entries[(size_t) filtered[(size_t) row]].userFile == result.file)
                    return row;
            return -1;
        };
        int row = findRow();
        if (row < 0)
        {
            clearFilters(); // a filter was hiding the new patch
            row = findRow();
        }
        if (row >= 0)
        {
            list->selectRow(row, false, true);
            list->scrollToEnsureRowIsOnscreen(row);
        }
        // Keep the requested name, not a factory row with the same source index.
        saveName.setText(name, false);
        setStatus("Saved \"" + name + "\".", 1);
        if (onPresetLoaded)
            onPresetLoaded();
        return;
    }

    if (result.status == Cs80AudioProcessor::PresetSaveStatus::error)
    {
        setStatus("Not saved: " + result.message, -1);
        return;
    }

    overwriteConfirmationPending = true;
    saveButton.setEnabled(false);
    setStatus("This filename already exists. Confirm overwrite or choose another name.", -1);
    overwriteDialog = juce::AlertWindow::showScopedAsync(
        juce::MessageBoxOptions::makeOptionsOkCancel(juce::MessageBoxIconType::WarningIcon,
            "Overwrite user preset?",
            "Saving \"" + name + "\" will replace the existing preset file:\n\n"
                + result.file.getFullPathName()
                + "\n\nDifferent patch names can map to the same filename. Overwrite this file?",
            "Overwrite", "Cancel", this),
        [safeThis = juce::Component::SafePointer<PresetBrowser>(this), name](int choice)
        {
            if (safeThis == nullptr)
                return;
            safeThis->overwriteConfirmationPending = false;
            safeThis->saveButton.setEnabled(true);
            if (choice == 1)
                safeThis->saveCurrentPreset(name, true);
            else
                safeThis->setStatus("Save cancelled. Existing preset unchanged.", 0);
        });
}

void PresetBrowser::selectCurrentPreset()
{
    const int current = processor.getCurrentPresetIndex();
    const auto currentName = processor.getPresetName();
    for (int row = 0; row < (int) filtered.size(); ++row)
    {
        const auto& entry = entries[(size_t) filtered[(size_t) row]];
        if ((! entry.isUser && entry.factoryIndex == current)
            || entry.name.equalsIgnoreCase(currentName))
        {
            selectedFilteredRow = row;
            list->selectRow(row, false, true);
            list->scrollToEnsureRowIsOnscreen(row);
            updateSelectionWidgets();
            break;
        }
    }
}

void PresetBrowser::focusSaveField()
{
    saveName.grabKeyboardFocus();
    saveName.selectAll();
}

// ---------------------------------------------------------------------------
// Keyboard, wheel and scrolling
// ---------------------------------------------------------------------------
bool PresetBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('f', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress('f', juce::ModifierKeys::ctrlModifier, 0))
    {
        search.grabKeyboardFocus();
        search.selectAll();
        return true;
    }
    return false;
}

void PresetBrowser::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (list == nullptr)
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }
    const auto p = e.getEventRelativeTo(this).getPosition();
    if (! layout.well.transformedBy(juce::AffineTransform::scale(layout.s)).expanded(6.0f).contains(p.toFloat()))
    {
        juce::Component::mouseWheelMove(e, wheel);
        return;
    }
    auto* viewport = list->getViewport();
    const double maxY = std::max(0, viewport->getViewedComponent()->getHeight() - viewport->getMaximumVisibleHeight());
    const double current = (double) viewport->getViewPositionY();
    const double delta = (double) wheel.deltaY * 14.0 * 12.0; // about a card and a half per notch
    if (wheel.isSmooth)
    {
        stopTimer();
        viewport->setViewPosition(0, juce::roundToInt(juce::jlimit(0.0, maxY, current - delta)));
    }
    else
        smoothScrollTo((isTimerRunning() ? scrollTarget : current) - delta);
}

void PresetBrowser::smoothScrollTo(double y)
{
    auto* viewport = list->getViewport();
    const double maxY = std::max(0, viewport->getViewedComponent()->getHeight() - viewport->getMaximumVisibleHeight());
    scrollTarget = juce::jlimit(0.0, maxY, y);
    lastScrollSet = viewport->getViewPositionY();
    startTimerHz(60);
}

void PresetBrowser::timerCallback()
{
    auto* viewport = list->getViewport();
    const int current = viewport->getViewPositionY();
    if (lastScrollSet >= 0 && std::abs(current - lastScrollSet) > 1)
    {
        stopTimer(); // something else (keyboard, scrollbar drag) took over
        return;
    }
    const double remaining = scrollTarget - (double) current;
    int next;
    if (std::abs(remaining) <= 1.5)
    {
        next = juce::roundToInt(scrollTarget);
        stopTimer();
    }
    else
        next = current + juce::roundToInt(remaining * 0.26);
    viewport->setViewPosition(0, next);
    lastScrollSet = next;
}

void PresetBrowser::scrollBarMoved(juce::ScrollBar*, double)
{
    updateRail();
}

void PresetBrowser::updateRail()
{
    if (rail == nullptr || list == nullptr)
        return;
    auto& bar = list->getVerticalScrollBar();
    rail->setMetrics(bar.getMaximumRangeLimit(), bar.getCurrentRangeSize(), bar.getCurrentRangeStart());
}

} // namespace cs80::ui

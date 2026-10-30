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
    static constexpr int kTrace = 72;

    struct Channel
    {
        float attack = 0.0f, decay = 0.0f, sustain = 0.0f, release = 0.0f;
        float gain = 0.0f;       // loudness relative to the louder channel, 0.08 .. 1
        float hpf = 20.0f, lpf = 18000.0f, hpfRes = 0.0f, lpfRes = 0.0f;
        bool hpfOn = true, lpfOn = true;
        float saw = 0.0f, pulse = 0.0f, sine = 0.0f, noise = 0.0f;
        std::array<float, kEnvPoints> env {};   // amplitude on the shared time axis
        // End of attack, decay and hold, and end of release, as fractions of
        // the shared time axis (used for the labels under the envelope scope).
        float markA = 0.0f, markD = 0.0f, markH = 0.0f, markEnd = 1.0f;
    };

    std::array<Channel, 2> ch;
    // Bright, Resonant, Swell, Hold, Motion, Width; each 0..1.
    std::array<float, kRadarAxes> radar {};
    float cutoff = 0.0f, resonance = 0.0f, attack = 0.0f, release = 0.0f;
    float sustain = 0.0f, decay = 0.0f, chorus = 0.0f;
    int dominant = 0;
    std::uint32_t styles = 0;
    // Spectrum fingerprint, one trace per channel: each channel's oscillator mix
    // pushed through its own filters, harmonics 1..72 of 220 Hz (drawn on a log
    // frequency axis), 0..1 on a 42 dB window. Both traces share one reference,
    // the loudest harmonic of the patch, so the channels keep their balance and
    // every patch fills its own picture. This is the thumbnail on every result
    // row, so patches that share an envelope still look different.
    std::array<std::array<float, kTrace>, 2> trace {};
    bool traceIdentical = false;
    float layerOne = 0.5f;                  // share of the output carried by channel I
    float detune = 0.0f, lfo = 0.0f, pwm = 0.0f, ring = 0.0f;
    bool arp = false;
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

// Style tags carry no colour: colour belongs to the instrument (round dots) and
// nothing else in the library, so the two codes can never be confused. Slim rows
// show short codes; the tooltip and the card/inspector tags spell them out.
const char* const kStyleCodes[styleCount] = { "SWL", "PERC", "BRT", "WRM", "RES", "MOV", "WID", "TAIL" };

float unit01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

float logNorm(float t) // 1 ms .. 20 s onto 0..1
{
    return unit01(std::log10(std::max(t, 0.001f) / 0.001f) / std::log10(20000.0f));
}

float filterMagnitudeDb(const PresetSignature::Channel& ch, float hz);

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
        ch.markA = segA[c] / axis;
        ch.markD = (segA[c] + segD[c]) / axis;
        ch.markH = (segA[c] + segD[c] + hold) / axis;
        ch.markEnd = total[c] / axis;
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
    s.layerOne = weight[0];
    s.detune = v[P_detune];
    s.lfo = lfo;
    s.pwm = pwm;
    s.ring = ring;
    s.arp = v[P_arpOn] >= 0.5f;

    {
        constexpr float f0 = 220.0f;
        constexpr int n = PresetSignature::kTrace;
        std::array<std::array<float, n>, 2> level {};
        float peak = 1.0e-6f;
        for (int c = 0; c < 2; ++c)
        {
            const auto& ch = s.ch[(size_t) c];
            const float pw = clampValue(v[chParam(c, CP_pw)], 0.06f, 0.94f);
            for (int k = 1; k <= n; ++k)
            {
                const float kf = (float) k;
                float amp = ch.saw / kf + ch.pulse * std::abs(std::sin(juce::MathConstants<float>::pi * kf * pw)) / kf
                            + ch.noise * 0.03f;
                if (k == 1)
                    amp += ch.sine;
                const float magnitude = std::pow(10.0f, filterMagnitudeDb(ch, f0 * kf) / 20.0f);
                const float value = ch.gain * amp * magnitude;
                level[(size_t) c][(size_t) k - 1] = value;
                peak = std::max(peak, value);
            }
        }
        float apart = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < n; ++k)
                s.trace[(size_t) c][(size_t) k] = unit01(1.0f + 20.0f * std::log10(std::max(level[(size_t) c][(size_t) k] / peak, 1.0e-4f)) / 42.0f);
        for (int k = 0; k < n; ++k)
            apart = std::max(apart, std::abs(s.trace[0][(size_t) k] - s.trace[1][(size_t) k]));
        s.traceIdentical = apart < 0.03f;
    }

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

const std::unordered_map<std::string, int>& parameterIdMap()
{
    static const std::unordered_map<std::string, int> ids = []
    {
        std::unordered_map<std::string, int> map;
        for (int i = 0; i < cs80::P_numParams; ++i)
            map.emplace(cs80::paramId(i), i);
        return map;
    }();
    return ids;
}

// The values stored in a saved user patch; `used[i]` says whether the file names parameter i.
void readUserFileValues(const juce::File& file, cs80::ParamValues& values, std::vector<char>& used)
{
    used.assign((size_t) cs80::P_numParams, 0);
    const auto& ids = parameterIdMap();
    if (auto xml = juce::XmlDocument::parse(file))
        for (auto* child : xml->getChildIterator())
        {
            if (! child->hasAttribute("id") || ! child->hasAttribute("value"))
                continue;
            const auto found = ids.find(child->getStringAttribute("id").toStdString());
            if (found == ids.end())
                continue;
            const auto& def = cs80::paramDef(found->second);
            values[found->second] = cs80::clampValue((float) child->getDoubleAttribute("value"), def.min, def.max);
            used[(size_t) found->second] = 1;
        }
}

// Reads the parameter values of a saved user patch (an APVTS state document)
// without touching the processor. Unknown or missing entries keep defaults.
std::shared_ptr<const PresetSignature> signatureForUserFile(const juce::File& file)
{
    const auto& ids = parameterIdMap();

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
// Arrow on the lit sort segment: +1 ascending (or oldest first), -1 descending.
constexpr int kSortDirection[] = { 1, 1, -1, 1, -1 };
constexpr int kScopeCount = 5;
constexpr int kSortCount = 5;

// Favourites, shared by every library window of this process. Keys are "F:" +
// factory name or "U:" + user file name. They are kept in a plain text file,
// one key per line, next to the user presets (CS80Reference/favourites.txt in
// the application-data folder; CS80_FAVOURITES_FILE overrides the path for
// portable installs and tests). The file is only read when the library opens
// and only written when a star is toggled, so the UI self-test never writes.
// Every file operation is best effort: a read-only or missing folder just
// means favourites last for the session. A "#view=compact" or "#view=cards"
// line in the same file remembers the row density the same way.
bool& compactPreference()
{
    static bool compact = false;
    return compact;
}

juce::StringArray& favouriteKeys()
{
    static juce::StringArray keys;
    return keys;
}

juce::File favouritesFile()
{
    const auto override = juce::SystemStats::getEnvironmentVariable("CS80_FAVOURITES_FILE", {});
    if (override.isNotEmpty())
        return juce::File(override);
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CS80Reference").getChildFile("favourites.txt");
}

// Remembers the file state we last saw, so another plugin instance's edits are
// picked up on the next open but our own writes are not re-read.
struct FavouritesStamp
{
    juce::String path;
    juce::Time modified;
};

FavouritesStamp& favouritesStamp()
{
    static FavouritesStamp stamp;
    return stamp;
}

void loadFavourites()
{
    const auto file = favouritesFile();
    if (! file.existsAsFile())
        return;
    auto& stamp = favouritesStamp();
    const auto modified = file.getLastModificationTime();
    if (stamp.path == file.getFullPathName() && stamp.modified == modified)
        return;
    stamp.path = file.getFullPathName();
    stamp.modified = modified;
    juce::StringArray lines;
    lines.addLines(file.loadFileAsString());
    favouriteKeys().clear();
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();
        if (trimmed.startsWith("#"))
        {
            if (trimmed == "#view=compact") compactPreference() = true;
            else if (trimmed == "#view=cards") compactPreference() = false;
        }
        else if (trimmed.isNotEmpty())
            favouriteKeys().addIfNotAlreadyThere(trimmed);
    }
}

void saveFavourites()
{
    const auto file = favouritesFile();
    const auto folder = file.getParentDirectory();
    if (! folder.exists() && folder.createDirectory().failed())
        return;
    const auto text = favouriteKeys().joinIntoString("\n") + (favouriteKeys().isEmpty() ? "" : "\n")
                      + (compactPreference() ? "#view=compact\n" : "#view=cards\n");
    if (file.replaceWithText(text))
    {
        auto& stamp = favouritesStamp();
        stamp.path = file.getFullPathName();
        stamp.modified = file.getLastModificationTime();
    }
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

// Squeezes a label sideways to fit, never smaller and never by more than
// `minScale`. Callers that can still run out of room draw with an ellipsis: a
// clipped label beats an unreadable one. Small labels pass a high floor so they
// are cut, not squeezed.
juce::Font fitFont(juce::Font font, const juce::String& text, float maxWidth, float minScale = 0.78f)
{
    const float w = textWidth(font, text);
    if (w <= maxWidth || w <= 0.0f)
        return font;
    return font.withHorizontalScale(std::max(minScale, maxWidth / w));
}

// Smallest type anywhere in the library, in design pixels. The editor's
// smallest window scales the panel to 0.7, so this is 8.75 screen pixels
// (cap height 5.5) there. Where a label does not fit at this size it is cut,
// never set smaller.
constexpr float kMinText = 12.5f;

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
    legend(g, text, area, tracked(uiFont(kMinText, juce::Font::bold), 0.14f), gold.withAlpha(alpha),
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

// Short forms for the result-card stat line: "850Hz", "3.6kHz", "250ms", "1.2s".
// The unit is never dropped, so every cutoff on every row reads the same way.
juce::String shortHertz(float hz)
{
    if (hz >= 10000.0f)
        return juce::String(juce::roundToInt(hz / 1000.0f)) + "kHz";
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, 1) + "kHz";
    return juce::String(juce::roundToInt(hz)) + "Hz";
}

juce::String shortTime(float seconds)
{
    if (seconds < 0.995f)
        return juce::String(juce::roundToInt(seconds * 1000.0f)) + "ms";
    return juce::String(seconds, seconds < 9.95f ? 1 : 0) + "s";
}

juce::String waveSummary(const PresetSignature::Channel& ch, bool shortForm)
{
    juce::StringArray waves;
    if (ch.saw > 0.5f) waves.add("SAW");
    if (ch.pulse > 0.5f) waves.add(shortForm ? "PUL" : "PULSE");
    if (ch.sine > 0.05f) waves.add(shortForm ? "SIN" : "SINE");
    if (ch.noise > 0.01f) waves.add(shortForm ? "NOI" : "NOISE");
    return waves.isEmpty() ? juce::String("--") : waves.joinIntoString(shortForm ? "+" : " + ");
}

// "850 Hz", "7.2 kHz", "12 kHz": the unit always travels with the number.
juce::String unitHertz(float hz)
{
    if (hz >= 10000.0f)
        return juce::String(juce::roundToInt(hz / 1000.0f)) + " kHz";
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, 1) + " kHz";
    return juce::String(juce::roundToInt(hz)) + " Hz";
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
    const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.18f);
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

// Instrument tag: a neutral capsule with a round dot in the instrument's colour.
// Round dots mean "instrument" everywhere in the library (chips, rows, inspector).
float drawTypePill(Graphics& g, float x, float y, float height, const juce::String& text, Colour colour,
                   bool lit, float minWidth = 0.0f)
{
    const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
    const float tw = textWidth(font, text);
    const float w = std::max(minWidth, tw + 30.0f);
    const Rectangle<float> r(x, y, w, height);
    g.setColour(juce::Colours::black.withAlpha(0.30f));
    g.fillRoundedRectangle(r, height * 0.5f);
    g.setColour(ivory.withAlpha(lit ? 0.78f : 0.5f));
    g.drawRoundedRectangle(r.reduced(0.5f), height * 0.5f, 1.0f);
    const float content = 10.0f + 7.0f + tw;
    const float cx = r.getX() + (w - content) * 0.5f, cy = r.getCentreY();
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillEllipse(cx - 1.0f, cy - 6.0f, 12.0f, 12.0f);
    g.setColour(colour);
    g.fillEllipse(cx, cy - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.fillEllipse(cx + 2.0f, cy - 4.0f, 4.0f, 3.0f);
    g.setFont(font);
    g.setColour(ivory.withAlpha(0.95f));
    g.drawText(text, Rectangle<float>(cx + 17.0f, y, tw + 6.0f, height), juce::Justification::centredLeft, false);
    return w;
}

juce::Font styleTagFont() { return tracked(uiFont(kMinText, juce::Font::bold), 0.06f); }
float styleTagWidth(const juce::String& text) { return textWidth(styleTagFont(), text) + 14.0f; }

// Style tag: a square-cornered neutral outline, never coloured.
float drawStyleTag(Graphics& g, float x, float y, float height, const juce::String& text, bool lit)
{
    const float w = styleTagWidth(text);
    const Rectangle<float> r(x, y, w, height);
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillRect(r);
    g.setColour(ivory.withAlpha(lit ? 0.72f : 0.5f));
    g.drawRect(r.reduced(0.5f), 1.0f);
    g.setColour(ivory.withAlpha(lit ? 1.0f : 0.92f));
    g.setFont(styleTagFont());
    g.drawText(text, r, juce::Justification::centred, false);
    return w;
}

// State tag (LOADED green, EDITED amber) on the row that is sounding.
float drawStatusTag(Graphics& g, float x, float y, const juce::String& text, Colour colour)
{
    const auto font = styleTagFont();
    const float w = textWidth(font, text) + 14.0f;
    const Rectangle<float> r(x, y, w, 18.0f);
    g.setColour(colour.withAlpha(0.26f));
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(colour.withAlpha(0.9f));
    g.drawRoundedRectangle(r.reduced(0.5f), 2.5f, 1.0f);
    g.setColour(colour.brighter(0.5f));
    g.setFont(font);
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

const Colour kChannelOne(0xffffc24d);
const Colour kChannelTwo(0xffe9dfc2);

// The same embossed micro-relief the main panel and the advanced pages use for
// the crinkle finish: white and black facets whose alpha is the relief
// strength. One texel lands on one device pixel; callers set the opacity.
juce::Image makeRelief(int n, int seed, float gain)
{
    juce::Random rng(seed);
    auto layer = [&](int cell)
    {
        const int cells = n / cell;
        std::vector<float> lattice((size_t) (cells * cells));
        for (auto& v : lattice) v = rng.nextFloat();
        std::vector<float> out((size_t) (n * n));
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const float fx = (float) x / (float) cell, fy = (float) y / (float) cell;
                const int x0 = (int) fx, y0 = (int) fy;
                float tx = fx - (float) x0, ty = fy - (float) y0;
                tx = tx * tx * (3.0f - 2.0f * tx);
                ty = ty * ty * (3.0f - 2.0f * ty);
                auto at = [&](int xx, int yy) { return lattice[(size_t) ((yy % cells) * cells + (xx % cells))]; };
                const float top = at(x0, y0) + (at(x0 + 1, y0) - at(x0, y0)) * tx;
                const float bot = at(x0, y0 + 1) + (at(x0 + 1, y0 + 1) - at(x0, y0 + 1)) * tx;
                out[(size_t) (y * n + x)] = top + (bot - top) * ty;
            }
        return out;
    };
    const auto coarse = layer(8), mid = layer(4), fine = layer(2);
    std::vector<float> height((size_t) (n * n));
    for (size_t i = 0; i < height.size(); ++i)
        height[i] = 0.40f * coarse[i] + 0.38f * mid[i] + 0.34f * fine[i] + 0.28f * rng.nextFloat();
    juce::Image image(juce::Image::ARGB, n, n, true);
    juce::Image::BitmapData data(image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
        {
            const float here = height[(size_t) (y * n + x)];
            const float diag = height[(size_t) (((y + 1) % n) * n + ((x + 1) % n))];
            const float e = juce::jlimit(-1.0f, 1.0f, (here - diag) * gain);
            data.setPixelColour(x, y, e > 0.0f ? juce::Colours::white.withAlpha(e)
                                               : juce::Colours::black.withAlpha(-e));
        }
    return image;
}

const juce::Image& crinkleTile()
{
    static const juce::Image tile = makeRelief(192, 5, 1.3f);
    return tile;
}

// Tile the relief over an area of a design-space context (`deviceScale` device
// pixels per design unit), so the grain stays one pixel fine at every size.
void fillRelief(Graphics& g, const juce::Image& tile, Rectangle<float> area, float deviceScale, float opacity,
                float shiftX = 0.0f, float shiftY = 0.0f)
{
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(area.toNearestIntEdges());
    juce::FillType fill(tile, juce::AffineTransform::scale(1.0f / std::max(0.25f, deviceScale)).translated(shiftX, shiftY));
    fill.setOpacity(opacity);
    g.setFillType(fill);
    g.fillRect(area);
}

// Result-row thumbnail: a tiny backlit window with the patch's spectrum, one
// trace per channel (I gold, II ivory) on a log-frequency axis with octave
// gridlines, normalised to the patch's own loudest harmonic. Beneath it, when
// there is room, a legend in the same two colours. Patches that share an envelope
// still differ in where their filters bend and how the two layers differ.
void drawThumbnail(Graphics& g, Rectangle<float> r, const PresetSignature& sig, bool lit, bool withLegend)
{
    g.setColour(Colour(0xff080a09));
    g.fillRoundedRectangle(r, 2.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.75f), r.getX(), r.getY(),
                                           juce::Colours::transparentBlack, r.getX(), r.getY() + 12.0f, false));
    g.fillRoundedRectangle(r.reduced(0.5f), 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.drawLine(r.getX() + 3.0f, r.getBottom() + 0.5f, r.getRight() - 3.0f, r.getBottom() + 0.5f, 1.0f);

    constexpr int n = PresetSignature::kTrace;
    const auto inner = r.reduced(6.0f, withLegend ? 5.0f : 4.0f);
    const auto plot = withLegend ? inner.withTrimmedBottom(17.0f) : inner;
    const auto xFor = [&](int k) { return plot.getX() + plot.getWidth() * std::log((float) k) / std::log((float) n); };
    const auto yFor = [&](float v) { return plot.getBottom() - 1.0f - v * (plot.getHeight() - 2.0f); };
    g.setColour(amber.withAlpha(0.10f));
    for (int octave = 2; octave < n; octave *= 2)
        g.drawVerticalLine(juce::roundToInt(xFor(octave)), plot.getY(), plot.getBottom());
    g.setColour(amber.withAlpha(0.25f));
    g.drawHorizontalLine(juce::roundToInt(plot.getBottom()), plot.getX(), plot.getRight());

    for (int pass = 0; pass < 2; ++pass)
    {
        const int c = pass == 0 ? 1 : 0;                        // II first, so I stays readable on top
        if (c == 1 && sig.traceIdentical)
            continue;
        juce::Path line;
        for (int k = 1; k <= n; ++k)
        {
            const float x = xFor(k), y = yFor(sig.trace[(size_t) c][(size_t) k - 1]);
            if (k == 1) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        if (c == 0)
        {
            juce::Path area(line);
            area.lineTo(plot.getRight(), plot.getBottom());
            area.lineTo(plot.getX(), plot.getBottom());
            area.closeSubPath();
            g.setGradientFill(juce::ColourGradient(kChannelOne.withAlpha(lit ? 0.34f : 0.26f), 0.0f, plot.getY(),
                                                   kChannelOne.withAlpha(0.03f), 0.0f, plot.getBottom(), false));
            g.fillPath(area);
        }
        g.setColour((c == 0 ? kChannelOne : kChannelTwo).withAlpha(c == 0 ? 1.0f : 0.85f));
        g.strokePath(line, juce::PathStrokeType(c == 0 ? 1.5f : 1.2f, juce::PathStrokeType::curved));
    }

    if (withLegend)
    {
        // Legend: what each colour is, in the colours the traces use.
        const float y = inner.getBottom() - 13.0f;
        const auto labelFont = monoFont(kMinText);
        g.setFont(labelFont);
        float x = inner.getX() + 1.0f;
        const auto key = [&](const juce::String& text, Colour c, bool swatch)
        {
            const float tw = textWidth(labelFont, text) + 2.0f;
            g.setColour(c.withAlpha(0.95f));
            g.drawText(text, Rectangle<float>(x, y, tw + 2.0f, 13.0f), juce::Justification::centredLeft, false);
            x += tw + 2.0f;
            if (swatch)
            {
                g.fillRoundedRectangle(x, y + 5.5f, 12.0f, 2.4f, 1.2f);
                x += 12.0f + 8.0f;
            }
        };
        if (sig.traceIdentical)
        {
            key("I", kChannelOne, true);
            key("= II", kChannelTwo, false);
        }
        else
        {
            key("I", kChannelOne, true);
            key("II", kChannelTwo, true);
        }
    }

    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawRoundedRectangle(r, 2.0f, 1.0f);
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

    if (kind == Kind::segment)
    {
        const auto area = bounds.reduced(1.5f * u, 1.5f * u);
        const float radius = 2.2f * u;
        if (on)
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRoundedRectangle(area.translated(0.0f, 1.2f * u), radius);
            g.setGradientFill(juce::ColourGradient(Colour(0xfff0bc54), area.getX(), area.getY(),
                                                   Colour(0xffb97c20), area.getX(), area.getBottom(), false));
            g.fillRoundedRectangle(area, radius);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.drawLine(area.getX() + 3.0f * u, area.getY() + 0.8f * u, area.getRight() - 3.0f * u,
                       area.getY() + 0.8f * u, 1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawRoundedRectangle(area, radius, 1.0f);
        }
        else if ((hover || down) && enabled)
        {
            g.setColour(ivory.withAlpha(0.10f));
            g.fillRoundedRectangle(area, radius);
        }
        const auto font = tracked(uiFont(kMinText * u, juce::Font::bold), 0.1f);
        g.setFont(font);
        g.setColour(! enabled ? ivory.withAlpha(0.46f) : (on ? Colour(0xff2b1b05) : ivory.withAlpha(hover ? 1.0f : 0.82f)));
        if (on && arrow != 0)
        {
            // Sort direction: a small triangle after the label of the lit segment.
            const float tw = textWidth(font, getButtonText());
            const float gap = 5.0f * u, aw = 7.0f * u;
            const float x0 = area.getCentreX() - (tw + gap + aw) * 0.5f;
            g.drawText(getButtonText(), Rectangle<float>(x0, area.getY(), tw + 2.0f * u, area.getHeight()),
                       juce::Justification::centredLeft, false);
            const float ax = x0 + tw + gap, ay = area.getCentreY();
            juce::Path tri;
            if (arrow > 0) tri.addTriangle(ax, ay + 3.0f * u, ax + aw, ay + 3.0f * u, ax + aw * 0.5f, ay - 3.5f * u);
            else           tri.addTriangle(ax, ay - 3.0f * u, ax + aw, ay - 3.0f * u, ax + aw * 0.5f, ay + 3.5f * u);
            g.fillPath(tri);
        }
        else
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
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.6f), false, 1.6f * u);
            fillStar(g, c, radius, hover ? amberBright : ivory.withAlpha(0.72f), false, 1.6f * u);
        }
        return;
    }

    if (kind == Kind::clear)
    {
        const auto c = bounds.getCentre();
        const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.30f;
        g.setColour(juce::Colours::black.withAlpha(enabled ? 0.55f : 0.30f));
        g.fillEllipse(c.x - radius, c.y - radius + u, radius * 2.0f, radius * 2.0f);
        g.setGradientFill(juce::ColourGradient(Colour(! enabled ? 0xff2c2a26 : (hover ? 0xff5c564b : 0xff45413a)), c.x, c.y - radius,
                                               Colour(enabled ? 0xff201e1b : 0xff1a1917), c.x, c.y + radius, false));
        g.fillEllipse(c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
        const float k = radius * 0.42f;
        g.setColour(! enabled ? ivory.withAlpha(0.28f) : (hover ? amberBright : ivory.withAlpha(0.8f)));
        g.drawLine(c.x - k, c.y - k, c.x + k, c.y + k, 1.6f * u);
        g.drawLine(c.x - k, c.y + k, c.x + k, c.y - k, 1.6f * u);
        return;
    }

    // The keys are the toolbar's own plates (satin black, or gold for the
    // primary action), so the library reads as part of the same instrument.
    const auto face = bounds.reduced(1.8f, 1.2f).withTrimmedBottom(1.8f).translated(0.0f, down && enabled ? 1.0f : 0.0f);
    const auto font = tracked(uiFont(kMinText * u, juce::Font::bold), 0.14f);
    Colour ink = ivory;
    if (kind == Kind::primary) ink = Colour(0xff2b1b05);
    else if (kind == Kind::danger) ink = Colour(0xffff9d89);

    if (kind == Kind::danger && armed && enabled)
    {
        // Armed: the next press deletes. A red plate with white type, unmistakable beside the grey keys.
        const auto lit = face.translated(0.0f, down ? 1.0f : 0.0f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(lit.translated(0.0f, 1.8f * u), 3.0f);
        g.setGradientFill(juce::ColourGradient(Colour(0xffd0472f), lit.getX(), lit.getY(),
                                               Colour(0xff8f2616), lit.getX(), lit.getBottom(), false));
        g.fillRoundedRectangle(lit, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.32f));
        g.drawLine(lit.getX() + 3.0f, lit.getY() + 1.0f, lit.getRight() - 3.0f, lit.getY() + 1.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.drawRoundedRectangle(lit, 3.0f, 1.0f);
        g.setFont(font);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawText(getButtonText(), lit.translated(0.0f, u), juce::Justification::centred, false);
        g.setColour(juce::Colours::white);
        g.drawText(getButtonText(), lit, juce::Justification::centred, false);
        return;
    }

    if (! enabled)
    {
        // A disabled key sits flush with the plate instead of standing proud.
        // Dimmer than a live key and flush with the plate, but still legible (3:1 or better):
        // a tooltip on every disabled key says why it is off.
        g.setColour(juce::Colours::black.withAlpha(0.32f));
        g.fillRoundedRectangle(face, 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.13f));
        g.drawRoundedRectangle(face, 3.0f, 1.0f);
        g.setFont(font);
        g.setColour(ivory.withAlpha(0.46f));
        g.drawText(getButtonText(), face, juce::Justification::centred, false);
        return;
    }

    if (kind == Kind::primary)
        drawAccentPlate(g, bounds, down, hover);
    else
        drawSatinPlate(g, bounds, false, down, hover, false);

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
    g.setFont(font);
    if (kind == Kind::primary)
    {
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawText(getButtonText(), textArea.translated(0.0f, u), juce::Justification::centred, false);
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
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
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 2.5f * scale, 1.5f);
    }
}

// A square keycap like the tone-selector tabs: a flat satin face on a dark
// wall, a square lamp window for the colour code, and a lit gold face with a
// tick when selected.
void ChipButton::paintButton(Graphics& g, bool hover, bool down)
{
    const float u = juce::jmax(0.5f, scale);
    const bool on = getToggleState();
    const bool faded = count == 0 && ! on;
    const float radius = 2.0f * u;
    const float wall = 2.0f * u;
    const auto cap = getLocalBounds().toFloat().reduced(0.5f * u, 0.5f * u).withTrimmedBottom(wall);
    const auto b = down ? cap.translated(0.0f, wall * 0.5f) : cap;

    if (on)
    {
        g.setColour(Colour(0xffffc24d).withAlpha(0.20f));
        g.drawRoundedRectangle(b.expanded(1.3f * u), radius + u, 1.5f * u);
    }
    g.setColour(on ? Colour(0xff6a4811) : Colour(0xff0c0b0a));
    g.fillRoundedRectangle(b.translated(0.0f, wall), radius);

    Colour top(0xff403d38), bottom(0xff2c2a27);
    if (on) { top = Colour(0xfff1c35e); bottom = Colour(0xffbd8626); }
    else if (hover || down) { top = top.brighter(0.18f); bottom = bottom.brighter(0.14f); }
    g.setGradientFill(juce::ColourGradient(top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle(b, radius);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.45f : (faded ? 0.07f : 0.17f)));
    g.drawLine(b.getX() + radius, b.getY() + 0.9f * u, b.getRight() - radius, b.getY() + 0.9f * u, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRoundedRectangle(b, radius, 1.0f);

    const float lampX = b.getX() + 12.5f * u, lampY = b.getCentreY();
    const Colour darkInk(0xff2b1b05);
    if (on && ! starGlyph && lampShape != Lamp::round)
    {
        juce::Path tick;
        tick.startNewSubPath(lampX - 4.6f * u, lampY + 0.2f * u);
        tick.lineTo(lampX - 1.4f * u, lampY + 3.6f * u);
        tick.lineTo(lampX + 4.8f * u, lampY - 3.8f * u);
        g.setColour(darkInk);
        g.strokePath(tick, juce::PathStrokeType(2.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (starGlyph)
    {
        fillStar(g, { lampX, lampY }, 6.4f * u, on ? darkInk : accentColour.withAlpha(faded ? 0.4f : 0.85f), on, 1.3f * u);
    }
    else if (lampShape == Lamp::round)
    {
        // Instrument: a round dot in the instrument's colour.
        const float rr = 5.2f * u;
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillEllipse(lampX - rr - u, lampY - rr - u, (rr + u) * 2.0f, (rr + u) * 2.0f);
        g.setColour(accentColour.withAlpha(faded ? 0.38f : 1.0f));
        g.fillEllipse(lampX - rr, lampY - rr, rr * 2.0f, rr * 2.0f);
        if (! faded)
        {
            g.setColour(juce::Colours::white.withAlpha(0.38f));
            g.fillEllipse(lampX - rr * 0.45f, lampY - rr * 0.75f, rr * 0.9f, rr * 0.6f);
        }
    }
    else if (lampShape == Lamp::hollow)
    {
        // Style: a square neutral outline, never coloured.
        const Rectangle<float> window(lampX - 4.8f * u, lampY - 4.8f * u, 9.6f * u, 9.6f * u);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRect(window);
        g.setColour(ivory.withAlpha(faded ? 0.5f : 0.9f));
        g.drawRect(window.reduced(0.6f * u), 1.3f * u);
    }
    else
    {
        // Library scope: a plain neutral square.
        const Rectangle<float> window(lampX - 4.8f * u, lampY - 4.8f * u, 9.6f * u, 9.6f * u);
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRoundedRectangle(window, 1.2f * u);
        g.setColour(accentColour.withAlpha(faded ? 0.4f : 1.0f));
        g.fillRect(window.reduced(1.5f * u));
    }

    auto text = b.withTrimmedLeft(25.0f * u).withTrimmedRight(6.0f * u);
    const auto countText = juce::String(count);
    const auto countFont = monoFont(kMinText * u, juce::Font::bold);
    const float countWidth = textWidth(countFont, countText);
    g.setFont(countFont);
    // An idle chip stays at least 4:1 against its face: it is dimmer than a live one, not hidden.
    g.setColour(on ? Colour(0xff3d2806) : ivory.withAlpha(faded ? 0.68f : 0.88f));
    g.drawText(countText, text.removeFromRight(countWidth + 2.0f * u), juce::Justification::centredRight, false);
    text.removeFromRight(3.0f * u);
    const auto label = getButtonText();
    const auto font = fitFont(tracked(uiFont(kMinText * u, juce::Font::bold), 0.05f), label, text.getWidth(), 0.9f);
    g.setFont(font);
    if (! on)
    {
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.drawText(label, text.translated(0.0f, 1.0f), juce::Justification::centredLeft, true);
    }
    g.setColour(on ? darkInk : ivory.withAlpha(faded ? 0.76f : 0.94f));
    g.drawText(label, text, juce::Justification::centredLeft, true);
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

    // Soft fades at the top and bottom edge say "more above / more below"
    // without a cut-off card. Two small gradients, only while scrollable.
    void paintOverChildren(Graphics& g) override
    {
        juce::ListBox::paintOverChildren(g);
        auto* viewport = getViewport();
        if (viewport == nullptr || viewport->getViewedComponent() == nullptr)
            return;
        const int position = viewport->getViewPositionY();
        const int total = viewport->getViewedComponent()->getHeight();
        const int visible = viewport->getMaximumVisibleHeight();
        const float fade = (float) std::max(10, getRowHeight() / 5);
        const Colour well(0xff0b0a09);
        if (position > 0)
        {
            g.setGradientFill(juce::ColourGradient(well.withAlpha(0.92f), 0.0f, 0.0f, well.withAlpha(0.0f), 0.0f, fade, false));
            g.fillRect(0.0f, 0.0f, (float) getWidth(), fade);
        }
        if (position + visible < total - 1)
        {
            g.setGradientFill(juce::ColourGradient(well.withAlpha(0.0f), 0.0f, (float) getHeight() - fade,
                                                   well.withAlpha(0.92f), 0.0f, (float) getHeight(), false));
            g.fillRect(0.0f, (float) getHeight() - fade, (float) getWidth(), fade);
        }
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
        // Transparent outside the panel face: the cheek columns show the instrument's own
        // walnut (the Backdrop), exactly as they do beside the memory lid.
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
        setBufferedToImage(true);
        setAccessible(false);
    }

    void paint(Graphics& g) override
    {
        const auto& L = owner.layout;
        g.addTransform(juce::AffineTransform::scale(L.s));
        const float W = L.width, H = L.height;

        // Where the face meets the walnut: a cast shadow on the wood and a gold hairline on the panel edge.
        for (const bool left : { true, false })
        {
            const float edge = left ? L.face.getX() : L.face.getRight();
            g.setGradientFill(juce::ColourGradient(juce::Colours::black.withAlpha(0.55f), edge, 0.0f,
                                                   juce::Colours::transparentBlack, edge + (left ? -9.0f : 9.0f), 0.0f, false));
            g.fillRect(left ? edge - 9.0f : edge, 0.0f, 9.0f, H);
        }

        // The panel face carries the same crinkle finish as the instrument body.
        fillPanelFace(g, L.face);
        fillRelief(g, crinkleTile(), L.face, L.s, 0.30f, 11.0f, 7.0f);
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
        g.setColour(Colour(0xffcdb878).withAlpha(0.34f));
        g.drawLine(L.face.getX() + 0.5f, 0.0f, L.face.getX() + 0.5f, H, 1.0f);
        g.drawLine(L.face.getRight() - 0.5f, 0.0f, L.face.getRight() - 0.5f, H, 1.0f);

        // Header band: a darker strip with a double etched rule beneath it.
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.fillRect(L.header);
        etchLine(g, L.face.getX() + 22.0f, L.header.getBottom(), L.face.getRight() - 22.0f, L.header.getBottom(), 0.8f);
        etchLine(g, L.face.getX() + 22.0f, L.header.getBottom() + 4.0f, L.face.getRight() - 22.0f,
                 L.header.getBottom() + 4.0f, 0.3f);

        auto title = tracked(uiFont(24.0f, juce::Font::bold), 0.14f);
        legend(g, "PRESET LIBRARY", { 78.0f, 38.0f, 300.0f, 40.0f }, title, ivory);
        caption(g, "SORT BY", { L.sortStrip.getX() + 4.0f, 21.0f, 120.0f, 16.0f });
        caption(g, "ROWS", { L.viewStrip.getX() + 4.0f, 21.0f, 120.0f, 16.0f });

        const float cornerX = L.face.getX() + 13.0f, cornerY = 27.0f;
        drawScrew(g, { cornerX, cornerY }, 4.6f, 0.4f);
        drawScrew(g, { L.face.getRight() - 13.0f, cornerY }, 4.6f, 1.1f);
        drawScrew(g, { cornerX, H - 27.0f }, 4.6f, 2.0f);
        drawScrew(g, { L.face.getRight() - 13.0f, H - 27.0f }, 4.6f, 0.9f);

        // Sort and view strip troughs.
        drawWell(g, L.sortStrip.expanded(2.0f), 6.0f);
        drawWell(g, L.viewStrip.expanded(2.0f), 6.0f);

        // The three etched groups.
        etchFrame(g, L.leftPlate, 6.0f, "FILTERS");
        etchFrame(g, L.results, 6.0f, "PATCHES");
        etchFrame(g, L.detail, 6.0f, "INSPECTOR");
        for (auto plate : { L.leftPlate, L.detail })
        {
            g.setColour(juce::Colours::black.withAlpha(0.10f));
            g.fillRoundedRectangle(plate.reduced(2.0f), 5.0f);
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawLine(plate.getX() + 8.0f, plate.getY() + 3.0f, plate.getRight() - 8.0f, plate.getY() + 3.0f, 1.0f);
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

namespace
{
// Vertical plan of the inspector in design units, measured from its top edge.
// The scopes take whatever the description and the window height leave over, so
// the bottom stack (PREV/NEXT, LOAD, SAVE) never moves while browsing.
struct InspectorPlan
{
    float desc = 0.0f, origin = 0.0f, rule = 0.0f, env = 0.0f, envH = 0.0f, filter = 0.0f, filterH = 0.0f;
    float radar = 0.0f, radarH = 0.0f;
    float prevNext = 0.0f, load = 0.0f, saveCaption = 0.0f, saveField = 0.0f, saveButton = 0.0f, status = 0.0f;
};

InspectorPlan planInspector(float detailHeight, bool hasDescription)
{
    InspectorPlan p;
    const float extra = detailHeight - 790.0f;
    p.prevNext = 596.0f + extra;
    p.load = 638.0f + extra;
    p.saveCaption = 692.0f + extra;
    p.saveField = 710.0f + extra;
    p.saveButton = 707.0f + extra;
    p.status = 750.0f + extra;
    p.desc = 124.0f;
    p.origin = p.desc + (hasDescription ? 40.0f : 0.0f);
    p.rule = p.origin + 52.0f;
    p.env = p.rule + 12.0f;
    p.radarH = 164.0f;
    p.radar = p.prevNext - 10.0f - p.radarH;
    const float avail = p.radar - 8.0f - p.env - 8.0f;
    p.envH = std::floor(avail * 0.56f);
    p.filter = p.env + p.envH + 8.0f;
    p.filterH = p.radar - 8.0f - p.filter;
    return p;
}

// A small engraved keycap for the key hints above the list.
float drawKeycap(Graphics& g, float x, float y, float height, const juce::String& text)
{
    const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.06f);
    const bool arrows = text == "^v";
    const float w = arrows ? 34.0f : textWidth(font, text) + 14.0f;
    const Rectangle<float> cap(x, y, w, height - 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRoundedRectangle(cap.translated(0.0f, 2.0f), 2.5f);
    g.setGradientFill(juce::ColourGradient(Colour(0xff4e4a43), cap.getX(), cap.getY(), Colour(0xff35322e), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 2.5f);
    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.drawLine(cap.getX() + 2.5f, cap.getY() + 0.8f, cap.getRight() - 2.5f, cap.getY() + 0.8f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);
    g.setColour(ivory.withAlpha(0.95f));
    if (arrows)
    {
        // Up and down triangles side by side.
        const auto c = cap.getCentre();
        juce::Path both;
        both.addTriangle(c.x - 10.0f, c.y + 3.5f, c.x - 2.0f, c.y + 3.5f, c.x - 6.0f, c.y - 4.0f);
        both.addTriangle(c.x + 2.0f, c.y - 3.5f, c.x + 10.0f, c.y - 3.5f, c.x + 6.0f, c.y + 4.0f);
        g.fillPath(both);
    }
    else
    {
        g.setFont(font);
        g.drawText(text, cap, juce::Justification::centred, false);
    }
    return w;
}

} // namespace

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

    void paintStrip(Graphics& g)
    {
        const auto& L = owner.layout;
        const Rectangle<float> window(L.strip.getX(), L.strip.getY() + 1.0f, 144.0f, L.strip.getHeight() - 2.0f);
        drawWell(g, window, 4.0f);
        const auto shown = juce::String((int) owner.filtered.size()).paddedLeft('0', 3);
        const auto total = juce::String((int) owner.entries.size()).paddedLeft('0', 3);
        const auto readout = window.withTrimmedRight(10.0f);
        g.setFont(monoFont(14.0f));
        g.setColour(amber.withAlpha(0.20f));
        g.drawText(shown + "/" + total, readout.translated(0.0f, 0.5f), juce::Justification::centredRight, false);
        g.setColour(amberBright);
        g.drawText(shown + "/" + total, readout, juce::Justification::centredRight, false);
        caption(g, "SHOWN", window.withTrimmedLeft(10.0f).withWidth(66.0f));

        // Key hints as keycaps, packed against the right edge. The last hints
        // drop away first when the strip is narrow.
        struct Hint { juce::String key, action; };
        const Hint hints[] = { { "^v", "BROWSE" },
                               { "ENTER", "LOAD" }, { "SPACE", "STAR" }, { "DEL", "DELETE" } };
        const auto keyFont = tracked(uiFont(kMinText, juce::Font::bold), 0.06f);
        const auto actionFont = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
        const float minX = window.getRight() + 24.0f;
        int count = 4;
        const auto widthOf = [&](int n)
        {
            float w = 0.0f;
            for (int i = 0; i < n; ++i)
                w += (hints[i].key == "^v" ? 34.0f : textWidth(keyFont, hints[i].key) + 14.0f) + 6.0f + textWidth(actionFont, hints[i].action) + (i + 1 < n ? 18.0f : 0.0f);
            return w;
        };
        while (count > 1 && L.strip.getRight() - widthOf(count) < minX)
            --count;
        float x = L.strip.getRight() - widthOf(count);
        const float y = window.getY() + (window.getHeight() - 18.0f) * 0.5f;
        for (int i = 0; i < count; ++i)
        {
            x += drawKeycap(g, x, y, 18.0f, hints[i].key) + 6.0f;
            const float aw = textWidth(actionFont, hints[i].action);
            legend(g, hints[i].action, { x, window.getY(), aw + 2.0f, window.getHeight() }, actionFont,
                   ivory.withAlpha(0.78f), juce::Justification::centredLeft);
            x += aw + 18.0f;
        }
    }

    void paintFilterCaptions(Graphics& g)
    {
        const auto& L = owner.layout;
        const float x = L.leftPlate.getX() + 20.0f, w = L.leftPlate.getWidth() - 40.0f;
        const auto head = [&](const juce::String& text, const juce::String& hint, float y)
        {
            caption(g, text, { x, y, 160.0f, 16.0f });
            if (hint.isNotEmpty())
                legend(g, hint, { x + w - 96.0f, y, 96.0f, 16.0f },
                       tracked(uiFont(kMinText, juce::Font::bold), 0.1f), ivoryDim.withAlpha(0.95f),
                       juce::Justification::centredRight);
            etchLine(g, x, y + 19.0f, x + w, y + 19.0f, 0.35f);
        };
        head("LIBRARY", "PICK ONE", L.scopeCaptionY);
        if (L.typeCaptionY > 0.0f)
            head("INSTRUMENT", "ANY OF", L.typeCaptionY);
        if (L.styleCaptionY > 0.0f)
            head("STYLE", "ALL OF", L.styleCaptionY);
        paintFilterRow(g);
    }

    // The status line at the foot of the column: how many filters narrow the
    // list and how many patches that leaves. The real CLEAR ALL key sits at its
    // right end and is a ghost while nothing is filtered.
    void paintFilterRow(Graphics& g)
    {
        const auto& L = owner.layout;
        if (L.filterRow.isEmpty())
            return;
        const int n = owner.activeFilterCount();
        const auto row = L.filterRow;
        etchLine(g, row.getX(), row.getY() - 8.0f, row.getRight(), row.getY() - 8.0f, 0.4f);
        const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
        const auto text = row.withTrimmedRight(124.0f);
        const int shown = (int) owner.filtered.size(), total = (int) owner.entries.size();
        legend(g, n == 0 ? juce::String("NO FILTERS") : juce::String(n) + (n == 1 ? " FILTER ACTIVE" : " FILTERS ACTIVE"),
               text.withHeight(17.0f), font, n == 0 ? ivoryDim.withAlpha(0.95f) : amberBright,
               juce::Justification::centredLeft, true);
        legend(g, n == 0 ? juce::String(total) + " SHOWN" : juce::String(shown) + " OF " + juce::String(total) + " SHOWN",
               text.withTrimmedTop(17.0f), font, ivoryDim.withAlpha(0.95f), juce::Justification::centredLeft, true);
    }

    void paintEmpty(Graphics& g)
    {
        const auto& L = owner.layout;
        const auto c = L.well.getCentre();
        const auto font = tracked(uiFont(22.0f, juce::Font::bold), 0.16f);
        legend(g, "NO PATCHES MATCH", { L.well.getX(), c.y - 56.0f, L.well.getWidth(), 34.0f }, font, ivory.withAlpha(0.9f),
               juce::Justification::centred);
        legend(g, "Loosen the search words or switch off a filter.",
               { L.well.getX(), c.y - 20.0f, L.well.getWidth(), 20.0f }, uiFont(13.0f), ivoryDim,
               juce::Justification::centred);
    }

    // ---- inspector ---------------------------------------------------------
    void paintDetail(Graphics& g)
    {
        const auto& L = owner.layout;
        const auto& D = L.detail;
        const float x = D.getX() + 22.0f, w = D.getWidth() - 44.0f;
        caption(g, "SELECTED PATCH", { x, D.getY() + 20.0f, 200.0f, 16.0f });

        const auto* e = owner.selectedEntry();
        if (e == nullptr)
        {
            const auto plan = planInspector(D.getHeight(), false);
            const auto c = D.getCentre();
            legend(g, "SELECT A PATCH", { D.getX(), c.y - 40.0f, D.getWidth(), 30.0f },
                   tracked(uiFont(20.0f, juce::Font::bold), 0.16f), ivory.withAlpha(0.85f), juce::Justification::centred);
            paragraph(g, "Click a row or use the arrow keys to inspect its envelope, filter and character.",
                      { D.getX() + 50.0f, c.y - 2.0f, D.getWidth() - 100.0f, 44.0f }, uiFont(13.0f), ivoryDim,
                      juce::Justification::centredTop, 3);
            caption(g, "SAVE CURRENT SOUND AS", { x, D.getY() + plan.saveCaption, w, 16.0f });
            return;
        }
        const auto plan = planInspector(D.getHeight(), e->ownDescription);

        // Loaded-now badge: says the marked row and this patch are the sound that is playing,
        // and turns amber when that sound has been edited since it was loaded.
        if (owner.isLoaded(*e))
        {
            const bool edited = owner.loadedEdited;
            const juce::String label = edited ? "LOADED / EDITED" : "LOADED";
            const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.12f);
            const float bw = textWidth(font, label) + 2.0f;
            const float bx = x + w - 52.0f - bw;
            drawJewelLamp(g, { bx - 11.0f, D.getY() + 28.0f }, 4.0f, edited ? amber : Colour(0xff58d27c), true);
            legend(g, label, { bx, D.getY() + 20.0f, bw + 4.0f, 16.0f }, font,
                   edited ? amberBright : Colour(0xff8fe6a9), juce::Justification::centredLeft);
        }

        // Name
        const auto nameFont = fitFont(tracked(uiFont(30.0f, juce::Font::bold), 0.02f), e->name, w - 46.0f);
        legend(g, e->name, { x, D.getY() + 38.0f, w - 46.0f, 44.0f }, nameFont, ivory, juce::Justification::centredLeft, true);

        // Type pill, then one pill per style tag. A patch with no tags simply has none.
        float cursor = x;
        const float pillY = D.getY() + 90.0f;
        cursor += drawTypePill(g, cursor, pillY, 22.0f, kTypes[e->type].label, typeColour(e->type), true) + 8.0f;
        int hidden = 0;
        for (int t = 0; t < styleCount; ++t)
        {
            if ((e->styles & (1u << (unsigned) t)) == 0)
                continue;
            const float pw = styleTagWidth(kStyleNames[t]);
            if (cursor + pw > x + w - 36.0f)
            {
                ++hidden;
                continue;
            }
            cursor += drawStyleTag(g, cursor, pillY, 22.0f, kStyleNames[t], true) + 8.0f;
        }
        if (hidden > 0)
            legend(g, "+" + juce::String(hidden), { cursor, pillY, 34.0f, 22.0f }, uiFont(kMinText, juce::Font::bold), ivoryDim);

        paintOrigin(g, { x, D.getY() + plan.origin, w, 36.0f }, *e);

        // Signature
        const auto sigFont = tracked(uiFont(kMinText, juce::Font::bold), 0.14f);
        const float sigW = textWidth(sigFont, "SIGNATURE");
        caption(g, "SIGNATURE", { x, D.getY() + plan.rule - 8.0f, sigW + 6.0f, 16.0f });
        etchLine(g, x + sigW + 14.0f, D.getY() + plan.rule, x + w, D.getY() + plan.rule, 0.5f);

        if (e->signature == nullptr)
            return;
        const auto& sig = *e->signature;
        paintEnvelopeScope(g, { x, D.getY() + plan.env, w, plan.envH }, sig);
        paintFilterScope(g, { x, D.getY() + plan.filter, w, plan.filterH }, sig);
        const float radarW = 216.0f;
        paintRadar(g, { x, D.getY() + plan.radar, radarW, plan.radarH }, sig);
        paintFacts(g, { x + radarW + 10.0f, D.getY() + plan.radar, w - radarW - 10.0f, plan.radarH }, sig);

        caption(g, "SAVE CURRENT SOUND AS", { x, D.getY() + plan.saveCaption, w, 16.0f });
    }

    // Three label/value cells: slot, origin, and either the save date or the layer balance.
    static void paintOrigin(Graphics& g, Rectangle<float> r, const Entry& e)
    {
        const juce::String origin = e.isUser ? "USER FILE" : (e.category == "Hardware Studies" ? "HARDWARE STUDY" : "FACTORY PROGRAM");
        juce::String thirdName = "LAYERS", thirdValue = "--";
        if (e.isUser && e.modified > 0)
        {
            thirdName = "SAVED";
            thirdValue = juce::Time(e.modified).toString(true, false).toUpperCase();
        }
        else if (e.signature != nullptr)
        {
            const int one = juce::roundToInt(e.signature->layerOne * 100.0f);
            thirdValue = one >= 97 ? juce::String("I ONLY") : (one <= 3 ? juce::String("II ONLY")
                                                                       : "I " + juce::String(one) + "  II " + juce::String(100 - one));
        }
        const struct { juce::String name, value; float width; } cells[] = {
            { "SLOT", e.slot, 70.0f }, { "ORIGIN", origin, 156.0f }, { thirdName, thirdValue, r.getWidth() - 226.0f } };
        float x = r.getX();
        for (const auto& cell : cells)
        {
            caption(g, cell.name, { x, r.getY(), cell.width, 16.0f }, juce::Justification::centredLeft, 0.85f);
            legend(g, cell.value, { x, r.getY() + 17.0f, cell.width - 6.0f, 19.0f }, monoFont(kMinText), ivory,
                   juce::Justification::centredLeft, true);
            x += cell.width;
        }
    }

    static void scopeFrame(Graphics& g, Rectangle<float> r, const juce::String& title, const PresetSignature& sig,
                           bool identical)
    {
        drawWell(g, r, 5.0f);
        caption(g, title, { r.getX() + 11.0f, r.getY() + 5.0f, 150.0f, 16.0f }, juce::Justification::centredLeft, 0.9f);
        float lx = r.getRight() - 12.0f;
        const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
        const auto key = [&](const juce::String& text, Colour c)
        {
            const float tw = textWidth(font, text);
            lx -= tw;
            legend(g, text, { lx, r.getY() + 5.0f, tw + 2.0f, 16.0f }, font, c);
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
        const auto p = Rectangle<float>(r.getX() + 12.0f, r.getY() + 26.0f, r.getWidth() - 24.0f, r.getHeight() - 26.0f - 24.0f);
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

        // Time axis for the louder channel: A, D, S and R with their settings. The axis
        // is compressed (log-like), so ticks sit at the segment boundaries rather than
        // at even seconds. Each label sits under its own segment when it can; a short
        // stage (a 3 ms attack) pushes its neighbours aside instead of being left out.
        const auto& ch = sig.ch[(size_t) sig.dominant];
        const float edges[5] = { 0.0f, ch.markA, ch.markD, ch.markH, ch.markEnd };
        const juce::String names[4] = { "A " + formatTime(ch.attack), "D " + formatTime(ch.decay),
                                        "S " + juce::String(juce::roundToInt(ch.sustain * 100.0f)) + " %",
                                        "R " + formatTime(ch.release) };
        const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.02f);
        const float axisY = p.getBottom() + 3.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float tx = p.getX() + p.getWidth() * edges[i];
            g.setColour(amber.withAlpha(0.55f));
            g.drawVerticalLine(juce::roundToInt(tx), p.getBottom(), p.getBottom() + 4.0f);
        }
        float width[4], left[4];
        float cursor = p.getX();
        for (int i = 0; i < 4; ++i)
        {
            width[i] = textWidth(font, names[i]);
            const float centre = p.getX() + p.getWidth() * (edges[i] + edges[i + 1]) * 0.5f;
            left[i] = std::max(cursor, centre - width[i] * 0.5f);
            cursor = left[i] + width[i] + 12.0f;
        }
        float limit = p.getRight();
        for (int i = 3; i >= 0; --i)
        {
            left[i] = std::min(left[i], limit - width[i]);
            limit = left[i] - 12.0f;
        }
        for (int i = 0; i < 4; ++i)
            legend(g, names[i], { left[i] - 2.0f, axisY + 3.0f, width[i] + 6.0f, 16.0f }, font, ivoryDim.withAlpha(0.98f),
                   juce::Justification::centred);
    }

    static void paintFilterScope(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        constexpr int points = 90;
        constexpr float dbTop = 18.0f, dbBottom = -36.0f;
        bool identical = sig.ch[0].lpf == sig.ch[1].lpf && sig.ch[0].hpf == sig.ch[1].hpf
                         && sig.ch[0].lpfRes == sig.ch[1].lpfRes && sig.ch[0].hpfRes == sig.ch[1].hpfRes
                         && sig.ch[0].lpfOn == sig.ch[1].lpfOn && sig.ch[0].hpfOn == sig.ch[1].hpfOn;
        scopeFrame(g, r, "VCF RESPONSE", sig, identical);
        const auto p = Rectangle<float>(r.getX() + 12.0f, r.getY() + 26.0f, r.getWidth() - 24.0f, r.getHeight() - 26.0f - 18.0f);
        const auto xForHz = [&](float hz) { return p.getX() + p.getWidth() * std::log(hz / 20.0f) / std::log(1000.0f); };
        const auto yForDb = [&](float db)
        {
            return p.getY() + p.getHeight() * (dbTop - juce::jlimit(dbBottom, dbTop, db)) / (dbTop - dbBottom);
        };
        const auto labelFont = tracked(uiFont(kMinText, juce::Font::bold), 0.06f);
        const float markHz[3] = { 100.0f, 1000.0f, 10000.0f };
        const char* const markText[3] = { "100 Hz", "1 kHz", "10 kHz" };
        for (int m = 0; m < 3; ++m)
        {
            const float mx = xForHz(markHz[m]);
            g.setColour(amber.withAlpha(0.09f));
            g.drawVerticalLine(juce::roundToInt(mx), p.getY(), p.getBottom());
            legend(g, markText[m], { mx - 36.0f, p.getBottom() + 1.0f, 72.0f, 16.0f }, labelFont,
                   ivoryDim.withAlpha(0.95f), juce::Justification::centred);
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

    // Six-axis character plot: rings at 25, 50, 75 and 100 percent, labels set
    // at the rim in plain type so the polygon stays the heaviest thing in the box.
    static void paintRadar(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        drawWell(g, r, 5.0f);
        constexpr int axes = PresetSignature::kRadarAxes;
        const char* const names[axes] = { "BRIGHT", "RESO", "SWELL", "HOLD", "MOTION", "WIDE" };
        caption(g, "CHARACTER", { r.getX() + 10.0f, r.getY() + 5.0f, 110.0f, 16.0f }, juce::Justification::centredLeft, 0.9f);
        const juce::Point<float> c(r.getCentreX(), r.getY() + 91.0f);
        const float radius = 52.0f;
        const auto pointAt = [&](int axis, float amount, float reach = 0.0f)
        {
            const float a = -juce::MathConstants<float>::halfPi + (float) axis * juce::MathConstants<float>::twoPi / (float) axes;
            return juce::Point<float>(c.x + std::cos(a) * (radius * amount + reach), c.y + std::sin(a) * (radius * amount + reach));
        };
        for (int ring = 1; ring <= 4; ++ring)
        {
            juce::Path hex;
            for (int a = 0; a < axes; ++a)
            {
                const auto pt = pointAt(a, (float) ring / 4.0f);
                if (a == 0) hex.startNewSubPath(pt); else hex.lineTo(pt);
            }
            hex.closeSubPath();
            g.setColour(amber.withAlpha(ring == 4 ? 0.38f : 0.15f));
            g.strokePath(hex, juce::PathStrokeType(1.0f));
        }
        g.setColour(amber.withAlpha(0.16f));
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
        g.strokePath(shape, juce::PathStrokeType(1.8f, juce::PathStrokeType::mitered));
        for (int a = 0; a < axes; ++a)
        {
            const auto pt = pointAt(a, std::max(0.07f, sig.radar[(size_t) a]));
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.fillEllipse(pt.x - 3.6f, pt.y - 3.0f, 7.2f, 7.2f);
            g.setColour(amberBright.brighter(0.4f));
            g.fillEllipse(pt.x - 3.0f, pt.y - 3.4f, 6.0f, 6.0f);
        }
        const auto font = tracked(uiFont(kMinText, juce::Font::plain), 0.06f);
        for (int a = 0; a < axes; ++a)
        {
            const auto pt = pointAt(a, 1.0f);
            const float dx = pt.x - c.x;
            Rectangle<float> box;
            auto justification = juce::Justification::centred;
            if (dx > 4.0f)       { box = { pt.x + 6.0f, pt.y - 8.0f, 56.0f, 16.0f }; justification = juce::Justification::centredLeft; }
            else if (dx < -4.0f) { box = { pt.x - 62.0f, pt.y - 8.0f, 56.0f, 16.0f }; justification = juce::Justification::centredRight; }
            else                 box = { pt.x - 30.0f, pt.y + (pt.y < c.y ? -19.0f : 3.0f), 60.0f, 16.0f };
            legend(g, names[a], box, font, ivory.withAlpha(0.88f), justification);
        }
    }

    // Numbers that the radar cannot show: oscillator mix, filter points, detune,
    // chorus and which modulation is active (the envelope times sit under the envelope scope).
    static void paintFacts(Graphics& g, Rectangle<float> r, const PresetSignature& sig)
    {
        const auto& lead = sig.ch[(size_t) sig.dominant];
        juce::StringArray motion;
        if (sig.lfo > 0.05f) motion.add("LFO");
        if (sig.pwm > 0.05f) motion.add("PWM");
        if (sig.ring > 0.05f) motion.add("RING");
        if (sig.arp) motion.add("ARP");
        const struct { const char* name; juce::String value; } rows[] = {
            { "WAVES", waveSummary(lead, true) },
            { "LOW-PASS", lead.lpfOn ? unitHertz(lead.lpf) : juce::String("OFF") },
            { "HIGH-PASS", lead.hpfOn ? unitHertz(lead.hpf) : juce::String("OFF") },
            { "DETUNE", juce::String(juce::roundToInt(sig.detune)) + " ct" },
            { "CHORUS", sig.chorus > 0.0f ? juce::String(juce::roundToInt(sig.chorus * 100.0f)) + " %" : juce::String("OFF") },
            { "MOTION", motion.isEmpty() ? juce::String("STILL") : motion.joinIntoString(" ") } };
        const float pitch = r.getHeight() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            const Rectangle<float> row(r.getX(), r.getY() + pitch * (float) i, r.getWidth(), pitch);
            if (i > 0)
                etchLine(g, row.getX(), row.getY(), row.getRight(), row.getY(), 0.16f);
            caption(g, rows[i].name, row.withTrimmedRight(row.getWidth() * 0.5f), juce::Justification::centredLeft, 0.9f);
            const auto valueArea = row.withTrimmedLeft(row.getWidth() * 0.40f);
            legend(g, rows[i].value, valueArea,
                   fitFont(uiFont(kMinText, juce::Font::bold), rows[i].value, valueArea.getWidth(), 0.92f),
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
        const float w = std::min(12.0f * std::max(0.6f, uiScale), b.getWidth());
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

    void mouseUp(const juce::MouseEvent&) override
    {
        dragging = false;
        repaint();
        if (onRelease != nullptr)
            onRelease();
    }

    std::function<void()> onRelease;
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
        const float radius = 10.5f * u;
        // A 28 px socket marks the click target even before the pointer arrives.
        const float socket = 14.0f * u;
        g.setColour(juce::Colours::black.withAlpha(over ? 0.55f : 0.34f));
        g.fillEllipse(c.x - socket, c.y - socket, socket * 2.0f, socket * 2.0f);
        g.setColour((over ? amberBright : ivory).withAlpha(over ? 0.8f : 0.30f));
        g.drawEllipse(c.x - socket, c.y - socket, socket * 2.0f, socket * 2.0f, 1.0f);
        if (fav)
        {
            g.setColour(amber.withAlpha(0.18f));
            g.fillEllipse(c.x - radius * 1.6f, c.y - radius * 1.6f, radius * 3.2f, radius * 3.2f);
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.55f), true, 1.0f);
            fillStar(g, c, radius, over ? amberBright.brighter(0.3f) : amberBright, true, 1.0f);
        }
        else
        {
            // An outline star needs 3:1 against the plate even when idle.
            const float a = over ? 1.0f : (selected || owner.hoveredRow == row ? 0.86f : 0.68f);
            fillStar(g, c.translated(0.0f, u), radius, juce::Colours::black.withAlpha(0.5f), false, 1.5f * u);
            fillStar(g, c, radius, over ? amberBright : ivory.withAlpha(a), false, 1.5f * u);
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
        return Rectangle<float>((float) getWidth() - 48.0f * u, 0.0f, 44.0f * u, (float) getHeight()).toNearestInt();
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

// Polls which patch is loaded while the library is open, so the marker in the
// list and the REVERT key follow changes made elsewhere (the toolbar arrows).
class PresetBrowser::Watcher final : public juce::Timer
{
public:
    explicit Watcher(PresetBrowser& o) : owner(o) {}
    void timerCallback() override { owner.pollLoaded(); }

private:
    PresetBrowser& owner;
};

// ===========================================================================
// PresetBrowser
// ===========================================================================
PresetBrowser::PresetBrowser(Cs80AudioProcessor& p) : processor(p)
{
    setOpaque(false);
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
    search.setTextToShowWhenEmpty("Search patches, instruments, character...", ivoryDim.withAlpha(0.8f));
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
    addChildComponent(clearSearch);         // shown only while there is text to clear

    for (int i = 0; i < kSortCount; ++i)
    {
        auto button = std::make_unique<PlateButton>(kSortNames[i], PlateButton::Kind::segment);
        button->setRadioGroupId(7002);
        button->setTitle(kSortTitles[i]);
        button->setTooltip(juce::String(kSortTitles[i]) + " (click again to reverse)");
        button->setToggleState(i == sortMode, juce::dontSendNotification);
        button->onClick = [this, i]
        {
            if (sortMode == i)
                sortReverse = ! sortReverse;
            else
            {
                sortMode = i;
                sortReverse = false;
            }
            rebuildFilter();
        };
        addAndMakeVisible(*button);
        sortButtons.push_back(std::move(button));
    }

    {
        const char* const names[] = { "FULL", "SLIM" };
        const char* const titles[] = { "Show full result rows", "Show slim result rows" };
        for (int i = 0; i < 2; ++i)
        {
            auto button = std::make_unique<PlateButton>(names[i], PlateButton::Kind::segment);
            button->setRadioGroupId(7003);
            button->setTitle(titles[i]);
            button->setTooltip(i == 0 ? "Full rows: spectrum, stats and style tags"
                                      : "Slim rows: more patches per screen (remembered with your favourites)");
            button->setToggleState(i == (compactRows ? 1 : 0), juce::dontSendNotification);
            button->onClick = [this, i] { setCompactRows(i == 1, true); };
            addAndMakeVisible(*button);
            viewButtons.push_back(std::move(button));
        }
    }

    closeButton.setTitle("CLOSE");
    closeButton.setTooltip("Close the library (Esc)");
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    buildFilterChips();

    resetButton.setTitle("Clear all filters");
    resetButton.onClick = [this]
    {
        const bool hadFocus = resetButton.hasKeyboardFocus(true);
        clearFilters();               // dims this key, so hand its focus to the list
        if (hadFocus)
            list->grabKeyboardFocus();
    };
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
    rail->onRelease = [this]
    {
        smoothScrollTo((double) list->getViewport()->getViewPositionY()); // settle on a whole card
    };
    rail->onPage = [this](double delta)
    {
        smoothScrollTo((isTimerRunning() ? scrollTarget : (double) list->getViewport()->getViewPositionY()) + delta);
    };
    addChildComponent(*rail);

    // Inspector.
    favouriteButton.setTitle("Favourite");
    favouriteButton.setTooltip("Add or remove this patch from your favourites (kept between sessions). Space toggles it in the list.");
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
    description.setColour(juce::Label::textColourId, ivory.withAlpha(0.82f));
    description.setMinimumHorizontalScale(0.85f);
    description.setBorderSize({ 0, 0, 0, 0 });
    description.setInterceptsMouseClicks(false, false);
    description.setTitle("Patch description");
    addAndMakeVisible(description);

    loadButton.setTitle("LOAD");
    loadButton.setTooltip("Load the selected patch, replacing the current sound (Enter or double-click)");
    loadButton.onClick = [this] { loadSelected(); };
    addAndMakeVisible(loadButton);
    revertButton.setTitle("REVERT");
    revertButton.setTooltip("Return to the sound that was loaded when the library opened");
    revertButton.onClick = [this] { revertToOpening(); };
    addAndMakeVisible(revertButton);
    deleteButton.setTitle("DELETE");
    deleteButton.setTooltip("Move the selected user patch to the trash (Delete)");
    deleteButton.onClick = [this] { deleteSelected(); };
    addAndMakeVisible(deleteButton);

    saveName.setTextToShowWhenEmpty("New user patch name", ivoryDim.withAlpha(0.8f));
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
    for (auto& b : viewButtons) b->setExplicitFocusOrder(order++);
    for (auto& b : scopeChips) b->setExplicitFocusOrder(order++);
    for (auto& b : typeChips) b->setExplicitFocusOrder(order++);
    for (auto& b : styleChips) b->setExplicitFocusOrder(order++);
    resetButton.setExplicitFocusOrder(order++);
    list->setExplicitFocusOrder(order++);
    favouriteButton.setExplicitFocusOrder(order++);
    previousButton.setExplicitFocusOrder(order++);
    nextButton.setExplicitFocusOrder(order++);
    loadButton.setExplicitFocusOrder(order++);
    revertButton.setExplicitFocusOrder(order++);
    deleteButton.setExplicitFocusOrder(order++);
    saveName.setExplicitFocusOrder(order++);
    saveButton.setExplicitFocusOrder(order++);
    closeButton.setExplicitFocusOrder(order++);

    watcher = std::make_unique<Watcher>(*this);
    captureOpening();
    updateSelectionWidgets();
    refresh();
}

PresetBrowser::~PresetBrowser()
{
    stopTimer();
    if (watcher != nullptr)
        watcher->stopTimer();
    if (list != nullptr)
    {
        list->getVerticalScrollBar().removeListener(this);
        list->removeMouseListener(hoverTracker.get());
        list->setModel(nullptr);
    }
}

void PresetBrowser::paint(Graphics&)
{
}

void PresetBrowser::buildFilterChips()
{
    for (int i = 0; i < kScopeCount; ++i)
    {
        auto chip = std::make_unique<ChipButton>(kScopeNames[i], i == 1 ? amberBright : ivory.withAlpha(0.8f));
        chip->setRadioGroupId(7001);
        chip->setStarGlyph(i == 1);
        chip->setTitle(juce::String("Show ") + kScopeNames[i]);
        if (i == 3)
            chip->setTooltip("Original hardware tab identities. Provisional voicings, not measured factory presets.");
        chip->setToggleState(i == scopeFilter, juce::dontSendNotification);
        chip->onClick = [this, i] { scopeFilter = i; rebuildFilter(); };
        addAndMakeVisible(*chip);
        scopeChips.push_back(std::move(chip));
    }
    for (int t = 0; t < typeCount; ++t)
    {
        auto chip = std::make_unique<ChipButton>(kTypes[t].label, typeColour(t));
        chip->setLamp(ChipButton::Lamp::round);
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
        auto chip = std::make_unique<ChipButton>(kStyleNames[s], ivory);
        chip->setLamp(ChipButton::Lamp::hollow);
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

    L.face = { 44.0f, 0.0f, W - 88.0f, H };
    L.header = { 44.0f, 0.0f, W - 88.0f, 98.0f };
    const float top = 114.0f, bottom = H - 32.0f;
    L.leftPlate = { 66.0f, top, 326.0f, bottom - top };
    L.detail = { W - 66.0f - 440.0f, top, 440.0f, bottom - top };
    const float resultsX = L.leftPlate.getRight() + 14.0f;
    L.results = { resultsX, top, L.detail.getX() - 14.0f - resultsX, bottom - top };
    L.strip = { L.results.getX() + 16.0f, top + 18.0f, L.results.getWidth() - 32.0f, 28.0f };
    L.well = { L.results.getX() + 14.0f, top + 56.0f, L.results.getWidth() - 28.0f, bottom - top - 56.0f - 14.0f };

    const float closeW = 112.0f;
    const float closeX = W - 66.0f - closeW;
    L.sortStrip = { closeX - 28.0f - 5.0f * 62.0f, 40.0f, 5.0f * 62.0f, 36.0f };
    L.viewStrip = { L.sortStrip.getX() - 24.0f - 2.0f * 56.0f, 40.0f, 2.0f * 56.0f, 36.0f };
    L.searchRect = { L.results.getX() + 14.0f, 32.0f, L.viewStrip.getX() - 28.0f - (L.results.getX() + 14.0f), 48.0f };

    // Header controls.
    closeButton.setUiScale(s);
    closeButton.setBounds(px(closeX, 36.0f, closeW, 44.0f));
    for (int i = 0; i < kSortCount; ++i)
    {
        sortButtons[(size_t) i]->setUiScale(s);
        sortButtons[(size_t) i]->setBounds(px(L.sortStrip.getX() + 62.0f * (float) i, L.sortStrip.getY(), 62.0f, L.sortStrip.getHeight()));
    }
    for (size_t i = 0; i < viewButtons.size(); ++i)
    {
        viewButtons[i]->setUiScale(s);
        viewButtons[i]->setBounds(px(L.viewStrip.getX() + 56.0f * (float) i, L.viewStrip.getY(), 56.0f, L.viewStrip.getHeight()));
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
    constexpr float chipH = 34.0f, chipPitch = 39.0f;
    float y = L.leftPlate.getY() + 26.0f;
    L.scopeCaptionY = y;
    y += 28.0f;
    for (int i = 0; i < kScopeCount; ++i)
    {
        scopeChips[(size_t) i]->setUiScale(s);
        scopeChips[(size_t) i]->setBounds(px(cx, y, cw, chipH));
        y += chipPitch;
    }
    y += 10.0f;
    const float colW = (cw - 6.0f) / 2.0f;
    int shown = 0;
    L.typeCaptionY = 0.0f;
    for (int t = 0; t < typeCount; ++t)
        if (typePresent & (1u << (unsigned) t))
            ++shown;
    if (shown > 0)
    {
        L.typeCaptionY = y;
        y += 28.0f;
        int slot = 0;
        for (int t = 0; t < typeCount; ++t)
        {
            auto& chip = *typeChips[(size_t) t];
            const bool present = (typePresent & (1u << (unsigned) t)) != 0;
            chip.setVisible(present);
            if (! present)
                continue;
            chip.setUiScale(s);
            chip.setBounds(px(cx + (float) (slot % 2) * (colW + 6.0f), y + (float) (slot / 2) * chipPitch, colW, chipH));
            ++slot;
        }
        y += (float) ((slot + 1) / 2) * chipPitch + 10.0f;
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
        y += 28.0f;
        int slot = 0;
        for (int t = 0; t < styleCount; ++t)
        {
            auto& chip = *styleChips[(size_t) t];
            const bool present = (stylePresent & (1u << (unsigned) t)) != 0;
            chip.setVisible(present);
            if (! present)
                continue;
            chip.setUiScale(s);
            chip.setBounds(px(cx + (float) (slot % 2) * (colW + 6.0f), y + (float) (slot / 2) * chipPitch, colW, chipH));
            ++slot;
        }
    }
    else
        for (auto& chip : styleChips) chip->setVisible(false);
    L.chipsBottom = y + (float) ((shown + 1) / 2) * chipPitch;

    // The filter status line at the foot of the column; its CLEAR ALL key is a ghost while no filter is on.
    L.filterRow = { cx, L.leftPlate.getBottom() - 20.0f - 34.0f, cw, 34.0f };
    resetButton.setUiScale(s);
    resetButton.setBounds(px(L.filterRow.getRight() - 116.0f, L.filterRow.getY(), 116.0f, L.filterRow.getHeight()));
    resetButton.setVisible(true);

    // Results.
    // A whole number of rows fills the well, so no row is ever cut mid-text
    // and wheel scrolling can snap from row to row.
    const float railW = 16.0f;
    const float innerH = L.well.getHeight() - 16.0f;
    const int innerPx = juce::roundToInt(innerH * s);
    const int visibleRows = juce::jmax(3, juce::roundToInt(innerH / (compactRows ? 44.0f : 76.0f)));
    const int rowHeight = juce::jmax(24, innerPx / visibleRows);
    const int listPx = rowHeight * visibleRows;
    const int listTop = juce::roundToInt((L.well.getY() + 8.0f) * s) + (innerPx - listPx) / 2;
    const int previousRowHeight = std::max(1, list->getRowHeight());
    const int topRow = juce::roundToInt((double) list->getViewport()->getViewPositionY() / (double) previousRowHeight);
    list->setRowHeight(rowHeight);
    list->setBounds(juce::roundToInt((L.well.getX() + 8.0f) * s), listTop,
                    juce::roundToInt((L.well.getWidth() - 16.0f - railW - 4.0f) * s), listPx);
    {
        // A new row height changes what "whole rows" means: keep the same top
        // row, re-align the scroll position and keep the selection in view.
        auto* viewport = list->getViewport();
        const int snapped = topRow * rowHeight;
        if (snapped != viewport->getViewPositionY())
            viewport->setViewPosition(0, snapped);
        if (selectedFilteredRow >= 0)
            list->scrollToEnsureRowIsOnscreen(selectedFilteredRow);
    }
    rail->uiScale = s;
    rail->setBounds(juce::roundToInt((L.well.getRight() - 8.0f - railW) * s), listTop + juce::roundToInt(6.0f * s),
                    juce::roundToInt(railW * s), listPx - juce::roundToInt(12.0f * s));
    emptyResetButton.setUiScale(s);
    emptyResetButton.setBounds(px(L.well.getCentreX() - 110.0f, L.well.getCentreY() + 18.0f, 220.0f, 40.0f));
    emptyResetButton.setVisible(filtered.empty());

    // Inspector controls (the bottom stack is fixed; the scopes flex above it).
    const float dx = L.detail.getX() + 22.0f, dy = L.detail.getY(), dw = L.detail.getWidth() - 44.0f;
    const auto plan = planInspector(L.detail.getHeight(), false);
    favouriteButton.setUiScale(s);
    favouriteButton.setBounds(px(dx + dw - 38.0f, dy + 8.0f, 38.0f, 36.0f));
    layoutDescription();
    description.setFont(uiFont(13.0f * s));
    const float halfW = (dw - 8.0f) * 0.5f;
    previousButton.setUiScale(s);
    previousButton.setBounds(px(dx, dy + plan.prevNext, halfW, 34.0f));
    nextButton.setUiScale(s);
    nextButton.setBounds(px(dx + dw - halfW, dy + plan.prevNext, halfW, 34.0f));
    const float smallW = 96.0f;
    loadButton.setUiScale(s);
    loadButton.setBounds(px(dx, dy + plan.load, dw - 2.0f * smallW - 16.0f, 44.0f));
    revertButton.setUiScale(s);
    revertButton.setBounds(px(dx + dw - 2.0f * smallW - 8.0f, dy + plan.load, smallW, 44.0f));
    deleteButton.setUiScale(s);
    deleteButton.setBounds(px(dx + dw - smallW, dy + plan.load, smallW, 44.0f));
    saveName.setUiScale(s);
    saveName.setBounds(px(dx, dy + plan.saveField, dw - 124.0f, 34.0f));
    saveName.setFont(uiFont(14.0f * s));
    saveName.applyFontToAllText(uiFont(14.0f * s));
    saveName.setIndents(juce::roundToInt(11.0f * s), 0);
    saveName.setBorder(juce::BorderSize<int>(0, 0, 0, 4));
    saveButton.setUiScale(s);
    saveButton.setBounds(px(dx + dw - 112.0f, dy + plan.saveButton, 112.0f, 40.0f));
    saveStatus.setBounds(px(dx, dy + plan.status, dw, 30.0f));
    saveStatus.setFont(uiFont(kMinText * s));

    surface->setBounds(getLocalBounds());
    legends->setBounds(getLocalBounds());
    surface->repaint();
    legends->repaint();
    updateRail();
}

// The description sits under the type pills when the patch has a sentence of
// its own; shared sentences (all hardware tones read the same) are not shown.
void PresetBrowser::layoutDescription()
{
    const auto& L = layout;
    if (L.s <= 0.0f)
        return;
    const float dx = L.detail.getX() + 22.0f, dy = L.detail.getY(), dw = L.detail.getWidth() - 44.0f;
    const auto plan = planInspector(L.detail.getHeight(), true);
    description.setBounds(juce::roundToInt(dx * L.s), juce::roundToInt((dy + plan.desc) * L.s),
                          juce::roundToInt(dw * L.s), juce::roundToInt(36.0f * L.s));
}

// ---------------------------------------------------------------------------
// Data
// ---------------------------------------------------------------------------
void PresetBrowser::refresh()
{
    loadFavourites();
    compactRows = compactPreference();
    for (size_t i = 0; i < viewButtons.size(); ++i)
        viewButtons[i]->setToggleState((i == 1) == compactRows, juce::dontSendNotification);
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

    // A sentence shared by several patches (all 22 hardware tones read the
    // same) says nothing about this one, so the inspector leaves it out.
    {
        std::unordered_map<std::string, int> sentences;
        for (const auto& e : entries)
            ++sentences[e.description.toStdString()];
        for (auto& e : entries)
            e.ownDescription = ! e.isUser && sentences[e.description.toStdString()] < 3;
    }
    syncLoaded();

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

int PresetBrowser::userCount() const
{
    int n = 0;
    for (const auto& e : entries)
        n += e.isUser ? 1 : 0;
    return n;
}

// +1 draws an up arrow (first to last, A to Z, oldest first), -1 a down arrow.
int PresetBrowser::sortArrow(int mode) const
{
    return kSortDirection[juce::jlimit(0, kSortCount - 1, mode)] * (sortReverse ? -1 : 1);
}

bool PresetBrowser::anyFilterActive() const
{
    return scopeFilter != 0 || typeFilter != 0 || styleFilter != 0 || search.getText().trim().isNotEmpty();
}

int PresetBrowser::activeFilterCount() const
{
    int n = (scopeFilter != 0 ? 1 : 0) + (search.getText().trim().isNotEmpty() ? 1 : 0);
    for (int t = 0; t < typeCount; ++t)
        n += (typeFilter & (1u << (unsigned) t)) != 0 ? 1 : 0;
    for (int t = 0; t < styleCount; ++t)
        n += (styleFilter & (1u << (unsigned) t)) != 0 ? 1 : 0;
    return n;
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
    if (sortMode == 4 && userCount() == 0)
        sortMode = 0;                   // NEW only means something once a patch has been saved
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
                    return x.isUser && (sortReverse ? x.modified < y.modified : x.modified > y.modified);
                });
                break;
        default: break;
    }
    if (sortReverse && sortMode != 4)
        std::reverse(filtered.begin(), filtered.end());

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
    const bool anyUser = userCount() > 0;
    for (int i = 0; i < kSortCount; ++i)
    {
        sortButtons[(size_t) i]->setEnabled(i != 4 || anyUser);
        sortButtons[(size_t) i]->setTooltip(i == 4 && ! anyUser ? juce::String("No saved patches yet: save one to sort by NEW")
                                                                : juce::String(kSortTitles[i]) + " (click again to reverse)");
        sortButtons[(size_t) i]->setToggleState(i == sortMode, juce::dontSendNotification);
        sortButtons[(size_t) i]->setArrowDirection(i == sortMode ? sortArrow(i) : 0);
    }
    // CLEAR ALL stays on the plate as a ghost key while nothing is filtered.
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
    deleteButton.setEnabled(e != nullptr && e->isUser);
    favouriteButton.setEnabled(e != nullptr);
    favouriteButton.setToggleState(e != nullptr && isFavourite(*e), juce::dontSendNotification);
    previousButton.setEnabled(selectedFilteredRow > 0);
    nextButton.setEnabled(selectedFilteredRow >= 0 && selectedFilteredRow + 1 < (int) filtered.size());
    revertButton.setEnabled(canRevert());
    updateLoadLabel();
    updateActionTips();
    if (e != nullptr)
        description.setText(e->description, juce::dontSendNotification);
    else
        description.setText("Select a patch to inspect it. Double-click or press LOAD to load it and replace the current sound.",
                            juce::dontSendNotification);
    description.setVisible(e != nullptr && e->ownDescription);
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
           + (e.isUser ? "user" : "factory") + (isFavourite(e) ? ", favourite" : "") + (isLoaded(e) ? ", loaded" : "");
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

namespace
{
// Right-hand side of a slim row in design units: the instrument pill, and on the
// name line the USER mark and the style codes. The painter and the tooltip both
// read it, so the hover area is exactly what is drawn. More than two tags show
// two codes and a "+N" count; the tooltip spells every tag out.
struct SlimCluster
{
    float pillX = 0.0f, pillW = 80.0f;
    Rectangle<float> user, more, hover;
    Rectangle<float> codes[2];
    int codeTag[2] = { -1, -1 };
    int codeCount = 0, tagCount = 0;
    float rightEdge = 0.0f;               // the name may run up to here
    float statsEdge = 0.0f;               // the stats line may run up to here
};

// The spectrum window of a row, in the row's design units.
Rectangle<float> thumbnailRect(bool compact, float H)
{
    const float cardY = compact ? 2.5f : 4.0f, cardH = H - (compact ? 5.0f : 8.0f);
    return compact ? Rectangle<float>(72.0f, cardY + 5.0f, 76.0f, cardH - 10.0f)
                   : Rectangle<float>(72.0f, cardY + 8.0f, 108.0f, cardH - 16.0f);
}

float typePillWidth(int type)
{
    const auto font = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
    return std::max(84.0f, textWidth(font, kTypes[type].label) + 30.0f);
}

SlimCluster slimCluster(bool isUser, int type, std::uint32_t styles, float W, float H)
{
    SlimCluster c;
    const Rectangle<float> card(6.0f, 2.5f, W - 12.0f, H - 5.0f);
    c.pillW = typePillWidth(type);
    c.pillX = card.getRight() - 52.0f - c.pillW;
    c.statsEdge = c.pillX - 12.0f;
    float right = c.statsEdge;
    const float blockH = 19.0f + 3.0f + 16.0f;
    const float lineY = card.getY() + (card.getHeight() - blockH) * 0.5f + 0.5f;
    if (isUser)
    {
        const float uw = textWidth(tracked(uiFont(kMinText, juce::Font::bold), 0.14f), "USER") + 2.0f;
        c.user = { right - uw, lineY - 0.5f, uw, 19.0f };
        right -= uw + 10.0f;
    }
    int tags[styleCount] = {}, n = 0;
    for (int t = 0; t < styleCount; ++t)
        if ((styles & (1u << (unsigned) t)) != 0)
            tags[n++] = t;
    c.tagCount = n;
    if (n > 0)
    {
        float x = right;
        if (n > 2)
        {
            const float cw = textWidth(monoFont(kMinText), "+" + juce::String(n - 2)) + 2.0f;
            c.more = { x - cw, lineY - 0.5f, cw, 19.0f };
            x -= cw + 5.0f;
        }
        c.codeCount = std::min(n, 2);
        for (int i = c.codeCount - 1; i >= 0; --i)
        {
            const float w = styleTagWidth(kStyleCodes[tags[i]]);
            c.codes[i] = { x - w, lineY, w, 18.0f };
            c.codeTag[i] = tags[i];
            x -= w + 5.0f;
        }
        const auto last = c.more.isEmpty() ? c.codes[c.codeCount - 1] : c.more;
        c.hover = Rectangle<float>(c.codes[0].getX() - 3.0f, lineY - 1.0f, last.getRight() - c.codes[0].getX() + 6.0f, 20.0f);
        right = c.codes[0].getX() - 12.0f;
    }
    c.rightEdge = right;
    return c;
}
} // namespace

juce::String PresetBrowser::getTooltipForRow(int row)
{
    if (layout.s <= 0.0f || list == nullptr)
        return {};
    const int index = entryIndexForRow(row);
    auto* rowComponent = list->getComponentForRowNumber(row);
    if (index < 0 || rowComponent == nullptr)
        return {};
    const auto& e = entries[(size_t) index];
    const float s = layout.s;
    const auto mouse = rowComponent->getLocalPoint(nullptr, juce::Desktop::getInstance().getMainMouseSource().getScreenPosition()) / s;
    const float W = (float) rowComponent->getWidth() / s, H = (float) rowComponent->getHeight() / s;
    if (e.signature != nullptr && thumbnailRect(compactRows, H).expanded(3.0f).contains(mouse))
        return "Harmonic spectrum on a log frequency axis. Amber is tone I, white is tone II, each drawn against its own patch's loudest harmonic.";
    if (! compactRows || e.styles == 0)
        return {};
    const auto cluster = slimCluster(e.isUser, e.type, e.styles, W, H);
    if (! cluster.hover.contains(mouse))
        return {};
    juce::StringArray names;
    for (int t = 0; t < styleCount; ++t)
        if ((e.styles & (1u << (unsigned) t)) != 0)
            names.add(juce::String(kStyleNames[t]).toLowerCase());
    return "Style tags: " + names.joinIntoString(", ");
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
    const bool loaded = isLoaded(e);
    const bool compact = compactRows;
    const auto tc = typeColour(e.type);

    // An engraved plate sunk into the well: crinkle finish, a dark lip above and
    // a lit lip below, a lit stripe in a slot at the left. Selection tints the
    // plate gold; nothing glossy.
    const float radius = 3.0f;
    const Rectangle<float> card(6.0f, compact ? 2.5f : 4.0f, W - 12.0f, H - (compact ? 5.0f : 8.0f));
    const Colour faceTop = selected ? Colour(0xff4e3c1c) : (hover ? Colour(0xff373430) : Colour(0xff2c2a27));
    const Colour faceBottom = selected ? Colour(0xff37290f) : (hover ? Colour(0xff2d2a27) : Colour(0xff242220));
    g.setGradientFill(juce::ColourGradient(faceTop, 0.0f, card.getY(), faceBottom, 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle(card, radius);
    fillRelief(g, crinkleTile(), card, s, selected ? 0.20f : 0.17f, (float) (rowNumber * 37 % 191), (float) (rowNumber * 19 % 173));
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.drawLine(card.getX() + radius, card.getY() + 0.9f, card.getRight() - radius, card.getY() + 0.9f, 1.4f);
    g.drawLine(card.getX() + 0.9f, card.getY() + radius, card.getX() + 0.9f, card.getBottom() - radius, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(selected ? 0.14f : 0.09f));
    g.drawLine(card.getX() + radius, card.getBottom() + 0.9f, card.getRight() - radius, card.getBottom() + 0.9f, 1.0f);
    g.setColour(selected ? amberBright.withAlpha(0.9f) : juce::Colours::black.withAlpha(0.75f));
    g.drawRoundedRectangle(card, radius, selected ? 1.4f : 1.0f);

    // A grip slot at the left edge. Purely engraved decoration: it never changes colour,
    // so nothing on the row has to be decoded from it (the selection is the gold plate,
    // the instrument is the dot in the pill, the sounding patch is the lamp by the number).
    const Rectangle<float> lamp(card.getX() + 7.0f, card.getY() + (compact ? 6.0f : 9.0f), 5.0f,
                                card.getHeight() - (compact ? 12.0f : 18.0f));
    g.setColour(juce::Colours::black.withAlpha(0.8f));
    g.fillRoundedRectangle(lamp.expanded(1.2f), 2.0f);
    g.setColour(ivory.withAlpha(0.20f));
    g.fillRoundedRectangle(lamp, 1.5f);

    // Slot number, and the loaded marker: a jewel lamp on the row that is sounding
    // now, green, or amber while the sound has been edited. A LOADED / EDITED tag
    // after the name says the same in words; the gold stripe only says where the
    // browsing cursor is, and there is no preview on a row, so nothing here is a play key.
    g.setFont(monoFont(kMinText));
    const auto slotInk = (selected ? amberBright : ivory).withAlpha(selected ? 0.98f : 0.82f);
    const bool edited = loaded && loadedEdited;
    if (loaded)
    {
        const juce::Point<float> lc(card.getX() + (compact ? 27.0f : 38.0f), compact ? card.getCentreY() : card.getCentreY() - 15.0f);
        drawJewelLamp(g, lc, compact ? 5.5f : 7.0f, edited ? Colour(0xffe6a832) : Colour(0xff58d27c), true);
        g.setColour(slotInk);
        if (compact)
            g.drawText(e.slot, Rectangle<float>(card.getX() + 37.0f, card.getY(), 26.0f, card.getHeight()), juce::Justification::centredLeft, false);
        else
            g.drawText(e.slot, Rectangle<float>(card.getX() + 20.0f, card.getCentreY() + 1.0f, 38.0f, 20.0f), juce::Justification::centred, false);
    }
    else
    {
        g.setColour(slotInk);
        g.drawText(e.slot, Rectangle<float>(card.getX() + 20.0f, card.getY(), 40.0f, card.getHeight()), juce::Justification::centredLeft, false);
    }

    // Thumbnail window: the patch's own harmonic fingerprint.
    if (e.signature != nullptr)
        drawThumbnail(g, thumbnailRect(compact, H), *e.signature, selected, ! compact);

    // Right cluster: instrument pill (and USER for saved patches). The star is a
    // separate cell. Factory and hardware rows carry no source label: the
    // library filter on the left already separates them.
    const float pillH = 20.0f;
    const auto cluster = compact ? slimCluster(e.isUser, e.type, e.styles, W, H) : SlimCluster{};
    const float pillW = compact ? cluster.pillW : typePillWidth(e.type);
    const float pillX = compact ? cluster.pillX : card.getRight() - 52.0f - pillW;
    const float pillY = (e.isUser && ! compact) ? card.getCentreY() - 19.0f : card.getCentreY() - pillH * 0.5f;
    drawTypePill(g, pillX, pillY, pillH, kTypes[e.type].label, tc, selected || hover, pillW);
    float rightEdge = compact ? cluster.rightEdge : pillX - 10.0f;
    if (e.isUser)
    {
        const auto userFont = tracked(uiFont(kMinText, juce::Font::bold), 0.14f);
        if (compact)
            legend(g, "USER", cluster.user, userFont, teal.brighter(0.6f), juce::Justification::centredRight);
        else
            legend(g, "USER", { pillX, pillY + pillH + 4.0f, pillW, 16.0f }, userFont, teal.brighter(0.6f), juce::Justification::centred);
    }

    // Name, stat line, style tags.
    const float textX = card.getX() + (compact ? 150.0f : 188.0f);
    if (compact)
    {
        // Neutral short codes (square outlines, never coloured: colour belongs to the
        // instrument dot); hovering them names every tag.
        for (int i = 0; i < cluster.codeCount; ++i)
            drawStyleTag(g, cluster.codes[i].getX(), cluster.codes[i].getY(), cluster.codes[i].getHeight(),
                         kStyleCodes[cluster.codeTag[i]], selected || hover);
        if (! cluster.more.isEmpty())
            legend(g, "+" + juce::String(cluster.tagCount - 2), cluster.more,
                   monoFont(kMinText), ivory.withAlpha(0.88f), juce::Justification::centredLeft);
    }
    const float textW = (compact ? rightEdge : pillX - 16.0f) - textX;
    const float statsW = (compact ? cluster.statsEdge : pillX - 10.0f) - textX;
    if (textW <= 20.0f)
        return;

    const bool hasTags = ! compact && e.signature != nullptr && e.styles != 0;
    const float nameH = compact ? 19.0f : 24.0f;
    const float statH = compact ? 16.0f : 17.0f;
    const float blockH = nameH + 3.0f + statH + (hasTags ? 4.0f + 18.0f : 0.0f);
    const float y0 = card.getY() + (card.getHeight() - blockH) * 0.5f;
    {
        const juce::String stateText = edited ? "EDITED" : (loaded ? "LOADED" : "");
        const float tagW = stateText.isEmpty() ? 0.0f : textWidth(styleTagFont(), stateText) + 14.0f;
        const float nameMax = textW - (tagW > 0.0f ? tagW + 8.0f : 0.0f);
        const auto nameFont = fitFont(uiFont(compact ? 15.5f : 17.0f, juce::Font::bold), e.name, nameMax);
        legend(g, e.name, { textX, y0, nameMax, nameH }, nameFont,
               selected ? Colour(0xfffff1cc) : ivory, juce::Justification::centredLeft, true);
        if (tagW > 0.0f)
            drawStatusTag(g, textX + std::min(textWidth(nameFont, e.name) + 8.0f, nameMax + 2.0f), y0 + (nameH - 18.0f) * 0.5f, stateText,
                          edited ? Colour(0xffe6a832) : Colour(0xff58d27c));
    }

    if (e.signature == nullptr)
        return;
    const auto& sig = *e.signature;
    const auto& lead = sig.ch[(size_t) sig.dominant];
    const auto capFont = tracked(uiFont(kMinText, juce::Font::bold), 0.1f);
    const auto valFont = monoFont(kMinText);
    const float statGap = compact ? 14.0f : 14.0f;
    const juce::String cut = shortHertz(sig.cutoff);   // always with its unit: 850Hz, 2.6kHz
    const struct { const char* name; juce::String value; } stats[] = {
        { "CUT", cut },
        { "ATK", shortTime(sig.attack) },
        { "OSC", waveSummary(lead, true) } };
    float x = textX;
    const float rowY = y0 + nameH + 3.0f;
    for (const auto& st : stats)
    {
        const float cw = textWidth(capFont, st.name) + 5.0f;
        const float vw = textWidth(valFont, st.value) + 1.0f;
        if (x + cw + vw > textX + statsW)
            break;
        g.setFont(capFont);
        g.setColour(gold.withAlpha(selected ? 1.0f : 0.88f));
        g.drawText(st.name, Rectangle<float>(x, rowY, cw, statH), juce::Justification::centredLeft, false);
        g.setFont(valFont);
        g.setColour(selected ? Colour(0xfffff1cc) : ivory.withAlpha(0.95f));
        g.drawText(st.value, Rectangle<float>(x + cw, rowY, vw + 2.0f, statH), juce::Justification::centredLeft, false);
        x += cw + vw + statGap;
    }

    // Style tags: only when the patch has some; an untagged row is simply shorter.
    if (hasTags)
    {
        float tx = textX;
        const float tagY = rowY + statH + 4.0f;
        int shownTags = 0, hiddenTags = 0;
        for (int t = 0; t < styleCount; ++t)
        {
            if ((e.styles & (1u << (unsigned) t)) == 0)
                continue;
            const float tw = styleTagWidth(kStyleNames[t]);
            if (shownTags >= 3 || tx + tw > textX + textW - 30.0f)
            {
                ++hiddenTags;
                continue;
            }
            tx += drawStyleTag(g, tx, tagY, 18.0f, kStyleNames[t], selected || hover) + 5.0f;
            ++shownTags;
        }
        if (hiddenTags > 0)
            legend(g, "+" + juce::String(hiddenTags), { tx, tagY, 30.0f, 18.0f }, monoFont(kMinText), ivoryDim);
    }
}

void PresetBrowser::selectedRowsChanged(int lastRowSelected)
{
    selectedFilteredRow = lastRowSelected;
    if (deleteArmedKey.isNotEmpty())
    {
        const auto* armed = selectedEntry();
        if (armed == nullptr || armed->userFile.getFullPathName() != deleteArmedKey)
            disarmDelete(true);
    }
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
    saveFavourites();
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

    syncLoaded();
    list->repaint();
    updateSelectionWidgets();
    if (onPresetLoaded)
        onPresetLoaded();
}

// The sound the library found loaded when it opened. REVERT puts exactly that
// back: the bank slot, the name and the parameter values as they were, which
// also restores unsaved edits. Physical setup parameters (pedal, jack, output
// switch) are never part of a preset, so they are left alone.
void PresetBrowser::captureOpening()
{
    const auto meta = processor.getPresetMetadata();
    openingName = meta.name;
    openingIndex = meta.factoryIndex;
    openingValues = processor.currentValues();
    hasOpening = true;
}

bool PresetBrowser::canRevert() const
{
    if (! hasOpening)
        return false;
    const auto meta = processor.getPresetMetadata();
    if (meta.name != openingName || meta.factoryIndex != openingIndex)
        return true;
    const auto now = processor.currentValues();
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == cs80::P_parityMode)
            continue;
        const float a = now[i], b = openingValues[i];
        if (std::abs(a - b) > 1.0e-4f * std::max(1.0f, std::abs(b)))
            return true;
    }
    return false;
}

void PresetBrowser::revertToOpening()
{
    if (! hasOpening)
        return;
    // Loading the opening slot restores the bank index; the saved values and
    // name then put back whatever was really loaded (an edit, a user patch).
    const auto& bank = processor.getFactoryPresets();
    if (openingIndex >= 0 && openingIndex < (int) bank.size())
        processor.loadFactoryPreset(openingIndex);
    processor.applyParameterValues(openingValues);
    if (processor.getPresetName() != openingName)
        processor.setPresetName(openingName);

    syncLoaded();
    list->repaint();
    updateSelectionWidgets();
    setStatus("Back to \"" + openingName + "\", the sound loaded when the library opened.", 0);
    if (onPresetLoaded)
        onPresetLoaded();
}

// Works out which library entry is the loaded sound. A factory program counts
// when the name still matches it (a fresh instance appends " / Original SUB");
// a saved user patch counts when its file name matches the preset name; a
// factory program with the same name is the last resort.
void PresetBrowser::syncLoaded()
{
    const auto meta = processor.getPresetMetadata();
    const auto& bank = processor.getFactoryPresets();
    juce::String key;
    if (meta.factoryIndex >= 0 && meta.factoryIndex < (int) bank.size())
    {
        const juce::String factoryName(bank[(size_t) meta.factoryIndex].name);
        if (meta.name == factoryName || meta.name.startsWith(factoryName + " / "))
            key = "F:" + factoryName;
    }
    if (key.isEmpty())
        for (const auto& e : entries)
            if (e.isUser && e.name.equalsIgnoreCase(meta.name))
            {
                key = e.key;
                break;
            }
    if (key.isEmpty())
        for (const auto& e : entries)
            if (! e.isUser && e.name.equalsIgnoreCase(meta.name))
            {
                key = e.key;     // renamed or re-slotted, but still that program by name
                break;
            }
    loadedKey = key;
    loadedEdited = key.isNotEmpty() && patchEdited();
}

// True when the sounding patch differs from the library entry that is marked loaded: a
// factory program against its stored values, a saved patch against its file. A patch that
// carries a variant suffix (a fresh instance is "Init Patch / Original SUB") and any patch
// the library cannot pin to a stored version are judged like the editor's EDITED lamp does:
// against the state first seen under that name. The editor can hand its own verdict over
// through isPatchEdited; then the two can never disagree.
bool PresetBrowser::patchEdited()
{
    if (isPatchEdited != nullptr)
        return isPatchEdited();

    const Entry* loaded = nullptr;
    for (const auto& e : entries)
        if (e.key == loadedKey)
        {
            loaded = &e;
            break;
        }
    const auto meta = processor.getPresetMetadata();
    const auto current = processor.currentValues();
    const auto differs = [&](const cs80::ParamValues& reference, const std::vector<char>* used)
    {
        for (int i = 0; i < cs80::P_numParams; ++i)
        {
            if (Cs80AudioProcessor::isPhysicalSetupParameter(i) || i == cs80::P_parityMode || i == cs80::P_sourcePerformance
                || (used != nullptr && used->at((size_t) i) == 0))
                continue;
            const auto& def = cs80::paramDef(i);
            if (std::abs(current[i] - reference[i]) > 0.003f * (def.max - def.min) + 1.0e-4f)
                return true;
        }
        return false;
    };

    bool edited = false;
    if (loaded != nullptr && meta.name == loaded->name)
    {
        const auto& bank = processor.getFactoryPresets();
        if (! loaded->isUser && loaded->factoryIndex >= 0 && loaded->factoryIndex < (int) bank.size())
            return differs(bank[(size_t) loaded->factoryIndex].values, nullptr);
        if (loaded->isUser)
        {
            const auto fileKey = loaded->key + "|" + juce::String(loaded->modified);
            if (fileKey != editFileKey)
            {
                editFileKey = fileKey;
                editFileValues = {};
                readUserFileValues(loaded->userFile, editFileValues, editFileUsed);
            }
            edited = differs(editFileValues, &editFileUsed);
        }
    }

    const auto key = meta.name + "|" + juce::String(meta.factoryIndex);
    if (key != editBaselineKey)
    {
        editBaselineKey = key;
        editBaseline = current;
        return edited;
    }
    return edited || differs(editBaseline, nullptr);
}

// LOAD becomes RELOAD on the row that is already sounding; the accessible name stays "LOAD".
void PresetBrowser::updateLoadLabel()
{
    const auto* e = selectedEntry();
    const bool again = e != nullptr && isLoaded(*e);
    const juce::String text = again ? "RELOAD" : "LOAD";
    if (loadButton.getButtonText() != text)
        loadButton.setButtonText(text);
    loadButton.setTooltip(e == nullptr ? "Select a patch in the list first"
                                       : (! again ? "Load the selected patch, replacing the current sound (Enter or double-click)"
                                                  : (loadedEdited ? "Load this patch again and discard the edits (Enter or double-click)"
                                                                  : "Load this patch again (Enter or double-click)")));
}

// Every key that can be dimmed says why when the pointer rests on it.
void PresetBrowser::updateActionTips()
{
    const auto* e = selectedEntry();
    revertButton.setTooltip(revertButton.isEnabled()
                                ? "Return to the sound that was loaded when the library opened (" + openingName + ")"
                                : "Nothing to revert: the sound is still the one loaded when the library opened");
    juce::String deleteTip = "Move the selected user patch to the trash. Press once to arm, again to confirm (Delete key)";
    if (e == nullptr)
        deleteTip = "Select a saved patch to delete it";
    else if (! e->isUser)
        deleteTip = "Factory and hardware programs cannot be deleted, only patches you saved";
    else if (deleteArmedKey.isNotEmpty())
        deleteTip = "Press again to move \"" + e->name + "\" to the trash. Waiting a few seconds or choosing another patch cancels";
    deleteButton.setTooltip(deleteTip);
    resetButton.setTooltip(resetButton.isEnabled() ? "Clear every filter and the search box"
                                                   : "No filters or search text are active");
    previousButton.setTooltip(previousButton.isEnabled() ? "Select the previous patch in the list"
                                                          : (e == nullptr ? "Select a patch in the list first" : "This is the first patch in the list"));
    nextButton.setTooltip(nextButton.isEnabled() ? "Select the next patch in the list"
                                                  : (e == nullptr ? "Select a patch in the list first" : "This is the last patch in the list"));
}

void PresetBrowser::pollLoaded()
{
    if (deleteArmedKey.isNotEmpty() && juce::Time::getMillisecondCounter() - deleteArmedAt > 4500)
        disarmDelete(true);
    const auto before = loadedKey;
    const bool wasEdited = loadedEdited;
    syncLoaded();
    if (before != loadedKey || wasEdited != loadedEdited)
    {
        list->repaint();
        legends->repaintDetail();
        updateLoadLabel();
    }
    const bool can = canRevert();
    if (can != revertButton.isEnabled())
    {
        revertButton.setEnabled(can);
        updateActionTips();
    }
}

void PresetBrowser::setCompactRows(bool compact, bool persist)
{
    compactPreference() = compact;
    const bool changed = compact != compactRows;
    compactRows = compact;
    for (size_t i = 0; i < viewButtons.size(); ++i)
        viewButtons[i]->setToggleState((i == 1) == compactRows, juce::dontSendNotification);
    if (changed)
    {
        layoutAll();
        list->repaint();
    }
    if (persist)
        saveFavourites();
}

void PresetBrowser::visibilityChanged()
{
    if (watcher == nullptr)
        return;
    if (isVisible())
    {
        captureOpening();
        syncLoaded();
        updateSelectionWidgets();
        watcher->startTimerHz(4);
    }
    else
    {
        disarmDelete(false);
        watcher->stopTimer();
    }
}

// DELETE is two presses: the first arms the key (it turns red and reads CONFIRM, the status line
// names the patch), the second moves the file to the trash. Waiting, hiding the library or choosing
// another patch disarms it. A press that follows within a third of a second (key repeat) is ignored.
void PresetBrowser::deleteSelected()
{
    const auto* entry = selectedEntry();
    if (entry == nullptr || ! entry->isUser)
        return;

    const auto file = entry->userFile;
    const auto name = entry->name;
    const auto key = file.getFullPathName();
    const auto now = juce::Time::getMillisecondCounter();

    if (deleteArmedKey != key)
    {
        deleteArmedKey = key;
        deleteArmedAt = now;
        deleteButton.setButtonText("CONFIRM");
        deleteButton.setArmed(true);
        setStatus("Press DELETE again to move \"" + name + "\" to the trash.", -1);
        updateActionTips();
        return;
    }
    if (now - deleteArmedAt < 300)
        return;

    disarmDelete(false);
    const bool removed = file.moveToTrash() || file.deleteFile();
    refresh();
    setStatus(removed ? "Deleted \"" + name + "\"." : "Could not delete \"" + name + "\".", removed ? 1 : -1);
}

void PresetBrowser::disarmDelete(bool announce)
{
    if (deleteArmedKey.isEmpty())
        return;
    deleteArmedKey = {};
    deleteButton.setArmed(false);
    deleteButton.setButtonText("DELETE");
    updateActionTips();
    if (announce)
        setStatus("Delete cancelled. The preset is unchanged.", 0);
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

// On opening, the library selects the loaded sound and brings it to the middle
// of the list, so the marker is on screen. A sound that is not in the library
// (an edited or unnamed patch) falls back to the bank slot or the name.
void PresetBrowser::selectCurrentPreset()
{
    syncLoaded();
    const int current = processor.getCurrentPresetIndex();
    const auto currentName = processor.getPresetName();
    int target = -1;
    for (int row = 0; row < (int) filtered.size() && loadedKey.isNotEmpty(); ++row)
        if (entries[(size_t) filtered[(size_t) row]].key == loadedKey)
        {
            target = row;
            break;
        }
    for (int row = 0; row < (int) filtered.size() && target < 0; ++row)
    {
        const auto& entry = entries[(size_t) filtered[(size_t) row]];
        if ((! entry.isUser && entry.factoryIndex == current) || entry.name.equalsIgnoreCase(currentName))
            target = row;
    }
    if (target < 0)
        return;

    selectedFilteredRow = target;
    list->selectRow(target, true, true);
    auto* viewport = list->getViewport();
    const int rowH = std::max(1, list->getRowHeight());
    const int visibleRows = std::max(1, viewport->getMaximumVisibleHeight() / rowH);
    const int first = viewport->getViewPositionY() / rowH;
    if (target < first || target >= first + visibleRows)
        viewport->setViewPosition(0, std::max(0, target - visibleRows / 2) * rowH);
    updateSelectionWidgets();
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
    const double rowH = (double) std::max(1, list->getRowHeight());
    if (wheel.deltaY == 0.0f)
        return;

    // Scrolling lands on whole cards. A virtual position collects the small
    // steps of a trackpad so it still moves; a wheel notch moves one to three
    // cards. Anything else that moved the list (keyboard, rail) resets it.
    if (! isTimerRunning() && std::abs(current - scrollTarget) > 1.0)
        scrollVirtual = current;
    if (wheel.isSmooth)
        scrollVirtual -= (double) wheel.deltaY * 14.0 * 12.0 * (double) layout.s;
    else
    {
        const double steps = std::max(1.0, std::round(std::abs((double) wheel.deltaY) * 3.0));
        scrollVirtual -= (wheel.deltaY < 0.0f ? -steps : steps) * rowH;
    }
    scrollVirtual = juce::jlimit(0.0, maxY, scrollVirtual);
    smoothScrollTo(scrollVirtual);
}

void PresetBrowser::smoothScrollTo(double y)
{
    auto* viewport = list->getViewport();
    const double maxY = std::max(0, viewport->getViewedComponent()->getHeight() - viewport->getMaximumVisibleHeight());
    const double rowH = (double) std::max(1, list->getRowHeight());
    scrollTarget = juce::jlimit(0.0, maxY, std::round(y / rowH) * rowH);
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

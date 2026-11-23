#include "FactoryPresets.h"
#include "InstrumentBody.h"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <utility>

namespace cs80
{

namespace
{
// Original owner manual tone-selector rows, not the previous shared generic bank.
const char* kToneNames[2][11] = {
    { "String 1", "String 3", "Brass 1", "Flute", "E. Piano", "Clavichord 1",
      "Harpsichord 1", "Organ 1", "Guitar 1", "Funky 1", "Funky 3" },
    { "String 2", "String 4", "Brass 2", "Brass 3", "Bass", "Clavichord 2",
      "Harpsichord 2", "Organ 2", "Guitar 2", "Funky 2", "Funky 4" }
};

void setC(ParamValues& p, int ch, int param, float v)
{
    p[chParam(ch, param)] = v;
}

void vco(ParamValues& p, int ch, bool saw, bool pulse, float sine, float noise,
         float pw = 0.5f, float pwmDepth = 0.0f, float pwmSpeed = 0.4f)
{
    setC(p, ch, CP_saw, saw ? 1.0f : 0.0f);
    setC(p, ch, CP_pulse, pulse ? 1.0f : 0.0f);
    setC(p, ch, CP_sine, sine);
    setC(p, ch, CP_noise, noise);
    setC(p, ch, CP_pw, pw);
    setC(p, ch, CP_pwmDepth, pwmDepth);
    setC(p, ch, CP_pwmSpeed, pwmSpeed);
}

void filter(ParamValues& p, int ch, float hp, float lp, float res)
{
    setC(p, ch, CP_hpfCutoff, hp);
    setC(p, ch, CP_lpfCutoff, lp);
    setC(p, ch, CP_lpfRes, res);
    setC(p, ch, CP_hpfRes, res * 0.35f);
}

void filterEnv(ParamValues& p, int ch, float il, float al, float a, float d, float r)
{
    setC(p, ch, CP_fIl, il);
    setC(p, ch, CP_fAl, al);
    setC(p, ch, CP_fA, a);
    setC(p, ch, CP_fD, d);
    setC(p, ch, CP_fR, r);
}

void ampEnv(ParamValues& p, int ch, float a, float d, float s, float r, float level = 0.72f)
{
    setC(p, ch, CP_vA, a);
    setC(p, ch, CP_vD, d);
    setC(p, ch, CP_vS, s);
    setC(p, ch, CP_vR, r);
    setC(p, ch, CP_vLevel, level);
}

void touch(ParamValues& p, int ch, float ib, float il, float ab, float al)
{
    setC(p, ch, CP_initBrill, ib);
    setC(p, ch, CP_initLevel, il);
    setC(p, ch, CP_afterBrill, ab);
    setC(p, ch, CP_afterLevel, al);
}

void tonePatch(ParamValues& p, int ch, int tone)
{
    switch (tone)
    {
        case 0: // Flute 1
            vco(p, ch, false, false, 0.94f, 0.015f);
            setC(p, ch, CP_feet, 1.0f);
            filter(p, ch, 180.0f, 3800.0f, 0.06f);
            filterEnv(p, ch, 0.5f, 0.54f, 0.22f, 0.5f, 0.4f);
            ampEnv(p, ch, 0.18f, 0.3f, 0.9f, 0.35f, 0.78f);
            touch(p, ch, 0.05f, 0.05f, 0.30f, 0.10f);
            break;
        case 1: // Flute 2
            vco(p, ch, false, false, 0.82f, 0.03f);
            filter(p, ch, 260.0f, 3000.0f, 0.10f);
            ampEnv(p, ch, 0.3f, 0.4f, 0.8f, 0.5f, 0.7f);
            touch(p, ch, 0.02f, 0.04f, 0.4f, 0.15f);
            break;
        case 2: // Oboe
            vco(p, ch, true, true, 0.18f, 0.015f, 0.62f, 0.32f, 0.35f);
            filter(p, ch, 420.0f, 2200.0f, 0.3f);
            filterEnv(p, ch, 0.5f, 0.58f, 0.06f, 0.55f, 0.4f);
            ampEnv(p, ch, 0.06f, 0.4f, 0.8f, 0.35f, 0.66f);
            touch(p, ch, 0.10f, 0.08f, 0.48f, 0.16f);
            break;
        case 3: // Clarinet
            vco(p, ch, false, true, 0.04f, 0.0f, 0.5f);
            filter(p, ch, 240.0f, 1500.0f, 0.22f);
            filterEnv(p, ch, 0.5f, 0.56f, 0.05f, 0.5f, 0.25f);
            ampEnv(p, ch, 0.05f, 0.3f, 0.82f, 0.22f, 0.7f);
            touch(p, ch, 0.06f, 0.05f, 0.4f, 0.12f);
            break;
        case 4: // Bass 1
            vco(p, ch, true, false, 0.22f, 0.0f);
            setC(p, ch, CP_feet, 0.0f);
            filter(p, ch, 30.0f, 450.0f, 0.18f);
            filterEnv(p, ch, 0.48f, 0.57f, 0.004f, 0.3f, 0.2f);
            ampEnv(p, ch, 0.003f, 0.35f, 0.7f, 0.15f, 0.8f);
            break;
        case 5: // Bass 2
            vco(p, ch, false, true, 0.3f, 0.0f, 0.52f);
            setC(p, ch, CP_feet, 0.0f);
            filter(p, ch, 30.0f, 620.0f, 0.28f);
            filterEnv(p, ch, 0.48f, 0.6f, 0.006f, 0.25f, 0.18f);
            ampEnv(p, ch, 0.003f, 0.28f, 0.75f, 0.18f, 0.8f);
            break;
        case 6: // Brass 1
            vco(p, ch, true, false, 0.05f, 0.0f);
            filter(p, ch, 90.0f, 850.0f, 0.18f);
            filterEnv(p, ch, 0.52f, 0.66f, 0.3f, 0.8f, 0.9f);
            ampEnv(p, ch, 0.18f, 0.9f, 0.75f, 1.0f, 0.72f);
            touch(p, ch, 0.3f, 0.16f, 0.42f, 0.2f);
            break;
        case 7: // Brass 2
            vco(p, ch, true, true, 0.02f, 0.0f, 0.58f, 0.1f, 0.3f);
            filter(p, ch, 120.0f, 1100.0f, 0.24f);
            filterEnv(p, ch, 0.5f, 0.64f, 0.22f, 0.7f, 0.8f);
            ampEnv(p, ch, 0.12f, 0.8f, 0.7f, 0.8f, 0.7f);
            touch(p, ch, 0.34f, 0.18f, 0.5f, 0.24f);
            break;
        case 8: // Strings 1
            vco(p, ch, true, true, 0.0f, 0.0f, 0.53f, 0.4f, 0.3f);
            filter(p, ch, 55.0f, 3600.0f, 0.1f);
            filterEnv(p, ch, 0.5f, 0.58f, 0.35f, 0.8f, 0.8f);
            ampEnv(p, ch, 0.25f, 1.0f, 0.82f, 1.0f, 0.68f);
            touch(p, ch, 0.16f, 0.08f, 0.3f, 0.12f);
            break;
        case 9: // Strings 2
            vco(p, ch, true, true, 0.02f, 0.0f, 0.62f, 0.6f, 0.18f);
            filter(p, ch, 180.0f, 2600.0f, 0.16f);
            filterEnv(p, ch, 0.5f, 0.56f, 0.5f, 1.2f, 1.2f);
            ampEnv(p, ch, 0.4f, 1.4f, 0.8f, 1.4f, 0.64f);
            touch(p, ch, 0.12f, 0.06f, 0.4f, 0.14f);
            break;
        default: // Organ
            vco(p, ch, false, false, 0.7f, 0.01f);
            filter(p, ch, 60.0f, 7000.0f, 0.04f);
            filterEnv(p, ch, 0.5f, 0.5f, 0.02f, 0.3f, 0.2f);
            ampEnv(p, ch, 0.004f, 0.2f, 0.9f, 0.1f, 0.72f);
            break;
    }
}

// Source-backed tab identity with deliberately provisional sound settings.
// These are original approximations awaiting preset-board resistor extraction
// and calibrated hardware captures; none is advertised as a measured factory tone.
void hardwareTonePatch(ParamValues& p, int ch, int tone)
{
    static constexpr int families[2][11] = {
        { 8, 9, 6, 0, 10, 3, 3, 10, 3, 7, 7 },
        { 8, 9, 7, 6, 4, 3, 3, 10, 3, 7, 7 }
    };
    tonePatch(p, ch, families[ch][tone]);
    if (tone == 4 && ch == 0) // Electric-piano family, provisional percussive sine/pulse.
    {
        vco(p, ch, false, true, 0.45f, 0.0f, 0.72f);
        filter(p, ch, 30.0f, 2400.0f, 0.1f);
        ampEnv(p, ch, 0.002f, 1.0f, 0.12f, 0.25f);
    }
    if (tone == 5 || tone == 6 || tone == 8)
    {
        vco(p, ch, tone != 5, true, tone == 8 ? 0.12f : 0.0f, 0.0f,
            ch == 0 ? 0.68f : 0.76f);
        filter(p, ch, 50.0f, tone == 6 ? 7500.0f : 2800.0f, 0.12f);
        filterEnv(p, ch, 0.5f, 0.65f, 0.002f, 0.3f, 0.2f);
        ampEnv(p, ch, 0.001f, tone == 8 ? 0.9f : 0.45f, 0.0f, 0.12f);
    }
    if (tone >= 9)
    {
        filter(p, ch, 160.0f, tone == 9 ? 900.0f : 1700.0f, 0.65f);
        filterEnv(p, ch, 0.25f, 0.85f, 0.015f, 0.3f, 0.15f);
        ampEnv(p, ch, 0.002f, 0.4f, 0.35f, 0.12f);
    }
}

} // namespace

const char* hardwareToneName(int index, int channel)
{
    if (index < 0 || index >= 11 || channel < 0 || channel > 1)
        return "";
    return kToneNames[channel][index];
}

void applyHardwareTone(ParamValues& values, int channel, int toneIndex)
{
    if (channel < 0 || channel > 1)
        return;

    // A tone-selector button replaces the channel card, not just a handful of
    // parameters. Start from the channel defaults so residue from the previous
    // tone (noise, sine level, PWM, pan, etc.) cannot leak into the new one.
    for (int i = 0; i < CP_count; ++i)
        values[chParam(channel, i)] = kChannelDefs[i].def;

    // The hardware tab replaces its rank's software body as well as the card.
    // A piano or guitar enhancement must never leak into the newly selected tab.
    for (int i = 0; i < 4; ++i)
        values[bodyParam(channel, i)] = paramDef(bodyParam(channel, i)).def;

    hardwareTonePatch(values, channel, std::clamp(toneIndex, 0, 10));
}

#include "OriginalCircuitLibrary.inc"

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank = [] {
        auto programs = original_circuit::build();
        struct Level { const char* name; float volume; };
        constexpr Level levels[] = {
#include "OriginalCircuitLevels.inc"
            { nullptr, 0.0f }
        };
        for (auto& program : programs)
            for (const auto& level : levels)
                if (level.name != nullptr && program.name == level.name)
                    program.values[P_volume] = level.volume;
        return programs;
    }();
    return bank;
}

// ---------------------------------------------------------------------------
// Curated circuit-engine library and the host program list
// ---------------------------------------------------------------------------
#include "CuratedLibrary.inc"

namespace
{
struct CalibratedLevel { const char* name; float volume; };
constexpr CalibratedLevel kCalibratedLevels[] = {
#include "CuratedLibraryLevels.inc"
    { nullptr, 0.0f }
};
} // namespace

const std::vector<FactoryPreset>& curatedLibrary()
{
    static const std::vector<FactoryPreset> library = [] {
        std::vector<FactoryPreset> l;
        l.reserve(320);
        curated::buildLibrary(l);
        for (auto& preset : l)
            for (const auto& level : kCalibratedLevels)
                if (level.name != nullptr && preset.name == level.name)
                    preset.values[P_volume] = level.volume;
        return l;
    }();
    return library;
}

const std::vector<FactoryPreset>& programLibrary()
{
    static const std::vector<FactoryPreset> programs = [] {
        std::vector<FactoryPreset> all = legacyLibrary();
        const auto& curated = curatedLibrary();
        all.insert(all.end(), curated.begin(), curated.end());
        return all;
    }();
    return programs;
}

int curatedProgramCount()
{
    return (int) curatedLibrary().size();
}

int programIndexForCurated(int curatedIndex)
{
    if (curatedIndex < 0 || curatedIndex >= curatedProgramCount())
        return -1;
    return legacyProgramCount + curatedIndex;
}

int curatedIndexForProgram(int programIndex)
{
    const int curated = programIndex - legacyProgramCount;
    return curated >= 0 && curated < curatedProgramCount() ? curated : -1;
}

int programIndexForMidi(int bankMsb, int bankLsb, int program)
{
    if (bankLsb < 0 || bankLsb > 127 || program < 0 || program > 127)
        return -1;
    const int inLibrary = bankLsb * 128 + program;
    if (bankMsb == 0)
        return programIndexForCurated(inLibrary);
    if (bankMsb == 1)
        return inLibrary < (int) legacyLibrary().size() ? inLibrary : -1;
    return -1;
}

} // namespace cs80

#include "FactoryPresets.h"

#include <algorithm>
#include <random>

namespace cs80
{

namespace
{
const char* kToneNames[11] = {
    "Flute 1", "Flute 2", "Oboe", "Clarinet", "Bass 1",
    "Bass 2", "Brass 1", "Brass 2", "Strings 1", "Strings 2", "Organ"
};

void setC(ParamValues& p, int ch, int param, float v)
{
    p[chParam(ch, param)] = v;
}

void both(ParamValues& p, int param, float v)
{
    setC(p, 0, param, v);
    setC(p, 1, param, v);
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

void initPatch(ParamValues& p)
{
    p = ParamValues();
    p[P_mix] = 0.5f;
    p[P_volume] = 0.58f;
    p[P_brilliance] = 0.0f;
    p[P_resonance] = 0.20f;
    p[P_voiceDispersion] = 0.38f;
    p[P_drift] = 0.25f;
    p[P_chorusOn] = 1.0f;
    p[P_chorusSpeed] = 0.42f;
    p[P_chorusDepth] = 0.52f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, false, 0.0f, 0.0f);
        setC(p, ch, CP_feet, 1.0f);
        filter(p, ch, 30.0f, 6500.0f, 0.12f);
        filterEnv(p, ch, 0.5f, 0.55f, 0.03f, 0.5f, 0.35f);
        ampEnv(p, ch, 0.01f, 0.4f, 0.75f, 0.35f, 0.72f);
        touch(p, ch, 0.0f, 0.0f, 0.0f, 0.0f);
        setC(p, ch, CP_vcfLevel, 0.8f);
    }
}

void brassBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 1.0f;
    p[P_chorusDepth] = 0.62f;
    p[P_resonance] = 0.26f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, false, 0.06f, 0.0f);
        setC(p, ch, CP_feet, ch == 0 ? 1.0f : 1.0f);
        filter(p, ch, 90.0f, 850.0f, 0.18f);
        filterEnv(p, ch, 0.52f, 0.66f, 0.38f, 0.9f, 1.0f);
        ampEnv(p, ch, 0.26f, 1.0f, 0.72f, 1.2f, 0.7f);
        touch(p, ch, 0.32f, 0.16f, 0.42f, 0.22f);
    }
    p[P_subSpeed] = 4.8f;
    p[P_subVco] = 0.018f;
    p[P_subVcf] = 0.012f;
}

void stringsBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 1.0f;
    p[P_chorusDepth] = 0.78f;
    p[P_detune] = 13.0f;
    p[P_resonance] = 0.18f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, true, 0.0f, 0.0f, 0.54f, 0.38f, 0.35f);
        filter(p, ch, 55.0f, 4300.0f, 0.10f);
        filterEnv(p, ch, 0.5f, 0.57f, 0.3f, 0.9f, 0.8f);
        ampEnv(p, ch, 0.3f, 1.1f, 0.8f, 1.1f, 0.66f);
        touch(p, ch, 0.18f, 0.10f, 0.34f, 0.14f);
    }
    p[P_subSpeed] = 4.4f;
    p[P_subVco] = 0.028f;
    p[P_subVcf] = 0.018f;
}

void bassBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 0.0f;
    p[P_brilliance] = -0.35f;
    p[P_resonance] = 0.2f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, ch == 1, 0.24f, 0.0f, 0.5f);
        setC(p, ch, CP_feet, 0.0f);
        filter(p, ch, 30.0f, 520.0f, 0.24f);
        filterEnv(p, ch, 0.48f, 0.58f, 0.006f, 0.28f, 0.18f);
        ampEnv(p, ch, 0.004f, 0.35f, 0.72f, 0.18f, 0.78f);
        touch(p, ch, 0.04f, 0.12f, 0.18f, 0.08f);
    }
}

void padBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 1.0f;
    p[P_chorusDepth] = 0.82f;
    p[P_detune] = 18.0f;
    p[P_resonance] = 0.3f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, true, 0.04f, 0.004f, ch == 0 ? 0.52f : 0.58f, 0.55f, 0.22f);
        filter(p, ch, 110.0f, 2600.0f, 0.16f);
        filterEnv(p, ch, 0.5f, 0.62f, 0.55f, 1.6f, 1.7f);
        ampEnv(p, ch, 0.58f, 2.0f, 0.8f, 2.2f, 0.62f);
        touch(p, ch, 0.22f, 0.1f, 0.48f, 0.16f);
    }
    p[P_subSpeed] = 0.18f;
    p[P_subVco] = 0.06f;
    p[P_subVcf] = 0.08f;
}

void leadBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 0.0f;
    p[P_portamento] = 0.08f;
    p[P_initBend] = 0.22f;
    p[P_resonance] = 0.42f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, ch == 0, true, 0.05f, 0.0f, 0.55f, 0.22f, 0.4f);
        filter(p, ch, 120.0f, 1600.0f, 0.35f);
        filterEnv(p, ch, 0.5f, 0.62f, 0.02f, 0.55f, 0.4f);
        ampEnv(p, ch, 0.008f, 0.45f, 0.8f, 0.4f, 0.7f);
        touch(p, ch, 0.18f, 0.12f, 0.55f, 0.2f);
    }
}

void keysBase(ParamValues& p)
{
    initPatch(p);
    p[P_chorusOn] = 0.0f;
    p[P_resonance] = 0.12f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, ch == 0, ch == 1, 0.12f, 0.0f, 0.5f);
        filter(p, ch, 60.0f, 2600.0f, 0.1f);
        filterEnv(p, ch, 0.52f, 0.62f, 0.005f, 0.45f, 0.28f);
        ampEnv(p, ch, 0.003f, 0.55f, 0.45f, 0.28f, 0.68f);
        touch(p, ch, 0.14f, 0.16f, 0.24f, 0.08f);
    }
}

void fxBase(ParamValues& p)
{
    initPatch(p);
    p[P_ringAmount] = 0.45f;
    p[P_ringSpeed] = 28.0f;
    p[P_ringDepth] = 0.7f;
    p[P_ringAttack] = 0.01f;
    p[P_ringDecay] = 0.7f;
    p[P_subVcf] = 0.22f;
    for (int ch = 0; ch < 2; ++ch)
    {
        vco(p, ch, true, true, 0.0f, 0.12f, 0.6f, 0.5f, 0.7f);
        filter(p, ch, 250.0f, 1800.0f, 0.42f);
        filterEnv(p, ch, 0.48f, 0.7f, 0.08f, 0.8f, 0.6f);
        ampEnv(p, ch, 0.02f, 0.7f, 0.55f, 0.7f, 0.65f);
    }
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

FactoryPreset make(const std::string& name, const std::string& category,
                   const std::string& description, const ParamValues& p)
{
    return { name, category, description, p };
}

} // namespace

const char* hardwareToneName(int index)
{
    if (index < 0 || index >= 11)
        return "";
    return kToneNames[index];
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

    tonePatch(values, channel, std::clamp(toneIndex, 0, 10));
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> bank = [] {
        std::vector<FactoryPreset> b;
        b.reserve(240);

        // 22 hardware tone-selector programs: one program per tone per channel.
        for (int t = 0; t < 11; ++t)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                ParamValues p;
                initPatch(p);
                tonePatch(p, ch, t);
                tonePatch(p, 1 - ch, t == 10 ? 8 : t);
                p[P_mix] = ch == 0 ? 0.68f : 0.32f;
                p[P_chorusOn] = 1.0f;
                b.push_back(make(std::string(ch == 0 ? "Tone I: " : "Tone II: ") + kToneNames[t],
                                 "Hardware Tones",
                                 "Recreation of a CS-80 tone-selector position.", p));
            }
        }

        {
            ParamValues p;
            brassBase(p);
            p[P_brilliance] = 0.12f;
            p[P_chorusDepth] = 0.7f;
            b.push_back(make("Replicant Brass", "Brass",
                             "Slow swelling brass with aftertouch brilliance, the cinematic CS-80 signature.", p));
        }
        {
            ParamValues p;
            brassBase(p);
            p[P_brilliance] = -0.1f;
            p[P_detune] = 11.0f;
            setC(p, 0, CP_fA, 0.55f);
            setC(p, 1, CP_fA, 0.62f);
            b.push_back(make("Runner Brass", "Brass", "Wide, slow brass with a long filter bloom.", p));
        }
        {
            ParamValues p;
            brassBase(p);
            p[P_initBend] = 0.32f;
            setC(p, 0, CP_fA, 0.12f);
            setC(p, 1, CP_fA, 0.16f);
            b.push_back(make("French Horn", "Brass", "Soft attack, narrow pulse, expressive pressure response.", p));
        }
        {
            ParamValues p;
            brassBase(p);
            p[P_ringAmount] = 0.12f;
            p[P_ringSpeed] = 5.4f;
            p[P_ringDepth] = 0.5f;
            b.push_back(make("Breathing Brass", "Brass", "Brass with a slow ring-mod breath underneath.", p));
        }
        {
            ParamValues p;
            stringsBase(p);
            b.push_back(make("Cinema Strings", "Strings", "Lush BBD chorus strings with slow PWM movement.", p));
        }
        {
            ParamValues p;
            stringsBase(p);
            p[P_chorusDepth] = 0.9f;
            p[P_detune] = 24.0f;
            setC(p, 1, CP_feet, 2.0f);
            b.push_back(make("Glass Strings", "Strings", "High, shimmering strings with a fifth above.", p));
        }
        {
            ParamValues p;
            stringsBase(p);
            p[P_brilliance] = -0.55f;
            p[P_resonance] = 0.35f;
            setC(p, 0, CP_hpfCutoff, 420.0f);
            setC(p, 1, CP_hpfCutoff, 380.0f);
            b.push_back(make("Distant Oceans", "Strings", "Dark high-pass strings that open under pressure.", p));
        }
        {
            ParamValues p;
            stringsBase(p);
            p[P_chorusOn] = 0.0f;
            setC(p, 0, CP_pulse, 0.0f);
            setC(p, 1, CP_pulse, 0.0f);
            b.push_back(make("Dry Ensemble", "Strings", "No chorus; the raw dual-rank detune.", p));
        }
        {
            ParamValues p;
            padBase(p);
            p[P_ringAmount] = 0.08f;
            p[P_ringSpeed] = 0.4f;
            b.push_back(make("1982 Pad", "Pads", "Slow, wide pad with a slow ring-modulator drift.", p));
        }
        {
            ParamValues p;
            padBase(p);
            p[P_subSpeed] = 0.11f;
            p[P_subVcf] = 0.18f;
            p[P_subVca] = 0.1f;
            b.push_back(make("Sweep VCA", "Pads", "Evolving tremolo pad driven by the sub-oscillator.", p));
        }
        {
            ParamValues p;
            padBase(p);
            setC(p, 0, CP_lpfCutoff, 900.0f);
            setC(p, 1, CP_lpfCutoff, 1500.0f);
            p[P_resonance] = 0.45f;
            p[P_ringAmount] = 0.18f;
            p[P_ringSpeed] = 0.8f;
            b.push_back(make("Blade Choir", "Pads", "Dark choir-like pad with moving band-pass color.", p));
        }
        {
            ParamValues p;
            bassBase(p);
            b.push_back(make("Dry Bass", "Bass", "Punchy dry bass with no chorus.", p));
        }
        {
            ParamValues p;
            bassBase(p);
            p[P_brilliance] = -0.1f;
            setC(p, 0, CP_pulse, 0.0f);
            setC(p, 1, CP_pulse, 1.0f);
            b.push_back(make("Resonant Bass", "Bass", "Resonant dual-rank bass with a pulse upper layer.", p));
        }
        {
            ParamValues p;
            bassBase(p);
            setC(p, 0, CP_feet, 1.0f);
            setC(p, 1, CP_feet, 1.0f);
            setC(p, 0, CP_lpfCutoff, 900.0f);
            setC(p, 1, CP_lpfCutoff, 700.0f);
            b.push_back(make("Bass Stack", "Bass", "Two 8' layers for a higher, growling bass.", p));
        }
        {
            ParamValues p;
            leadBase(p);
            b.push_back(make("Ribbon Lead", "Leads", "Portamento lead designed for the ribbon and pressure.", p));
        }
        {
            ParamValues p;
            leadBase(p);
            p[P_ringAmount] = 0.2f;
            p[P_ringSpeed] = 62.0f;
            b.push_back(make("Metal Lead", "Leads", "Ring-modulated lead with a narrow pulse.", p));
        }
        {
            ParamValues p;
            keysBase(p);
            b.push_back(make("Analog Piano", "Keys", "CS-80 electric-piano approximation with a fast filter pluck.", p));
        }
        {
            ParamValues p;
            keysBase(p);
            p[P_chorusOn] = 1.0f;
            p[P_chorusDepth] = 0.35f;
            b.push_back(make("Bright Keys", "Keys", "Chorused bright keys with strong velocity brightness.", p));
        }
        {
            ParamValues p;
            fxBase(p);
            b.push_back(make("Ring Bell", "FX", "Metallic ring-mod bell with a long tail.", p));
        }
        {
            ParamValues p;
            fxBase(p);
            p[P_ringAmount] = 0.7f;
            p[P_ringSpeed] = 0.6f;
            p[P_ringDepth] = 0.9f;
            b.push_back(make("Laser Sweep", "FX", "Slow ring-mod sweep for sci-fi transitions.", p));
        }
        {
            ParamValues p;
            padBase(p);
            p[P_arpOn] = 1.0f;
            p[P_arpRate] = 6.0f;
            p[P_arpOctaves] = 2.0f;
            b.push_back(make("Arp Motion", "Sequences", "Arpeggiated pad with a two-octave pattern.", p));
        }
        {
            ParamValues p;
            p = ParamValues();
            b.push_back(make("Init Patch", "Init", "A clean starting point.", p));
        }

        // Generate the rest of the bank deterministically. Each family gets
        // meaningful parameter changes rather than a reverb preset with a new
        // name.
        std::mt19937 rng(0xC580u);
        auto uni = [&](float lo, float hi) {
            return lo + (hi - lo) * (float) (rng() % 10000) / 9999.0f;
        };
        const char* families[] = { "Brass", "Strings", "Pads", "Bass", "Keys", "Leads", "FX", "Sequences" };
        const char* adjectives[] = { "Amber", "Velvet", "Distant", "Neon", "Hollow", "Golden", "Frozen",
                                     "Electric", "Midnight", "Slow", "Bright", "Dark", "Wide", "Soft" };
        int familyCounts[8] = { 22, 24, 28, 22, 18, 22, 20, 22 };
        int serial = 1;
        for (int family = 0; family < 8; ++family)
        {
            for (int n = 0; n < familyCounts[family]; ++n)
            {
                ParamValues p;
                switch (family)
                {
                    case 0: brassBase(p); break;
                    case 1: stringsBase(p); break;
                    case 2: padBase(p); break;
                    case 3: bassBase(p); break;
                    case 4: keysBase(p); break;
                    case 5: leadBase(p); break;
                    case 6: fxBase(p); break;
                    default: padBase(p); break;
                }
                p[P_brilliance] = uni(-0.55f, 0.45f);
                p[P_resonance] = uni(0.08f, 0.58f);
                p[P_detune] = uni(2.0f, 26.0f);
                p[P_chorusOn] = uni(0.0f, 1.0f) > 0.28f ? 1.0f : 0.0f;
                p[P_chorusDepth] = uni(0.2f, 0.9f);
                p[P_subSpeed] = uni(0.1f, 7.5f);
                p[P_subVco] = uni(0.0f, 0.08f);
                p[P_subVcf] = uni(0.0f, 0.14f);
                p[P_initBend] = uni(0.0f, 0.35f);
                p[P_portamento] = uni(0.0f, 0.25f);
                for (int ch = 0; ch < 2; ++ch)
                {
                    setC(p, ch, CP_lpfCutoff, uni(380.0f, 8200.0f));
                    setC(p, ch, CP_fA, uni(0.005f, 1.8f));
                    setC(p, ch, CP_fD, uni(0.05f, 2.5f));
                    setC(p, ch, CP_vA, uni(0.004f, 0.8f));
                    setC(p, ch, CP_vR, uni(0.08f, 2.8f));
                    setC(p, ch, CP_pwmDepth, uni(0.0f, 0.55f));
                }
                if (family == 6)
                {
                    p[P_ringAmount] = uni(0.15f, 0.85f);
                    p[P_ringSpeed] = uni(0.3f, 120.0f);
                    p[P_ringDepth] = uni(0.2f, 0.9f);
                }
                if (family == 7)
                {
                    p[P_arpOn] = 1.0f;
                    p[P_arpRate] = uni(2.0f, 12.0f);
                    p[P_arpOctaves] = (float) (1 + (rng() % 3));
                }
                const std::string name = std::string(adjectives[n % 14]) + " " + families[family] + " " + std::to_string(serial++);
                b.push_back(make(name, families[family], "Factory variation generated from the CS-80 architecture.", p));
            }
        }
        return b;
    }();
    return bank;
}

} // namespace cs80

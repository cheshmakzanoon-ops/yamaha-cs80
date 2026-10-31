#pragma once

#include "Params.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// "Randomise within original ranges" (docs/workstreams/ws6-library.md, rules R1-R8).
// Deterministic for a seed: one splitmix64 stream and an explicit float mapping,
// never <random> distributions (their output is implementation-defined). The
// log-uniform draws use std::exp2/log2, so across different C libraries a value
// may differ in its last float bit; the rules below hold either way.
// Only the original CS-80 panel controls move, and only inside sub-ranges of the
// hardware control ranges that keep a note audible and away from clipping-prone
// extremes. Software extensions, the engine policy, the volume, physical setup,
// tuning and performance controls are copied from the base sound unchanged.
namespace cs80::library
{
class SeededRandom
{
public:
    explicit SeededRandom(std::uint32_t seed) noexcept : state(0x9e3779b97f4a7c15ull ^ (std::uint64_t) seed) {}

    std::uint64_t next() noexcept
    {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    // [0, 1) with 24 bits, exactly representable as float.
    float unit() noexcept { return (float) (next() >> 40) * (1.0f / 16777216.0f); }
    float range(float lo, float hi) noexcept { return lo + (hi - lo) * unit(); }
    // Log-uniform for times and frequencies (equal chance per octave).
    float logRange(float lo, float hi) noexcept { return lo * std::exp2(std::log2(hi / lo) * unit()); }
    bool chance(float p) noexcept { return unit() < p; }
    int pick(const float* weights, int count) noexcept
    {
        float total = 0.0f;
        for (int i = 0; i < count; ++i) total += weights[i];
        float x = unit() * total;
        for (int i = 0; i < count; ++i)
        {
            if (x < weights[i]) return i;
            x -= weights[i];
        }
        return count - 1;
    }

private:
    std::uint64_t state;
};

// Bounds of the randomised sub-ranges (all inside the ParamDef ranges).
struct RandomiseBounds
{
    float maxLevelSum = 1.45f;        // R5: vLevel + initLevel + afterLevel per rank
    float maxCombinedQ = 0.85f;       // R6: rank RES * 0.8 + master RESONANCE * 0.6
    float maxRingAmount = 0.35f;      // R7
    float minCutoffRatio = 2.5f;      // R3: LPF >= 2.5 x HPF (open passband)
    float minLowpassHz = 250.0f;      // R3
    float maxAttackSeconds = 1.5f;    // R4
};

inline ParamValues randomiseWithinOriginalRanges(const ParamValues& base, std::uint32_t seed,
                                                 const RandomiseBounds& b = {})
{
    SeededRandom r(seed);
    ParamValues v = base;
    // R1: a fixed draw order per seed, so adding a rule changes results only
    // from the point it is added; the rule list is versioned in the docs.
    float rankRes[2] {};
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto set = [&](int local, float x) { v[chParam(ch, local)] = x; };
        // R2: at least one audible source per rank: saw, pulse, both, or sine.
        const float waveWeights[] = { 0.3f, 0.25f, 0.3f, 0.15f };
        const int wave = r.pick(waveWeights, 4);
        set(CP_saw, wave == 0 || wave == 2 ? 1.0f : 0.0f);
        set(CP_pulse, wave == 1 || wave == 2 ? 1.0f : 0.0f);
        set(CP_sine, wave == 3 ? r.range(0.6f, 1.0f) : (r.chance(0.3f) ? r.range(0.0f, 0.4f) : 0.0f));
        set(CP_noise, r.chance(0.2f) ? r.range(0.0f, 0.25f) : 0.0f);
        set(CP_pw, r.range(0.5f, 0.85f));
        set(CP_pwmDepth, r.chance(0.5f) ? r.range(0.0f, 0.6f) : 0.0f);
        set(CP_pwmSpeed, r.logRange(0.1f, 8.0f));
        const float feetWeights[] = { 0.2f, 0.4f, 0.08f, 0.22f, 0.05f, 0.05f };
        set(CP_feet, (float) r.pick(feetWeights, 6));
        // R3: filters keep an open passband.
        const float hp = r.logRange(20.0f, 600.0f);
        set(CP_hpfCutoff, hp);
        set(CP_hpfRes, r.range(0.0f, 0.3f));
        const float lpLow = std::max(b.minLowpassHz, hp * b.minCutoffRatio);
        set(CP_lpfCutoff, r.logRange(lpLow, std::max(lpLow * 1.01f, 8000.0f)));
        rankRes[ch] = r.range(0.0f, 0.6f);
        set(CP_lpfRes, rankRes[ch]);
        set(CP_fIl, r.range(0.0f, 0.6f));
        set(CP_fAl, r.range(0.0f, 0.8f));
        set(CP_fA, r.logRange(0.002f, 2.0f));
        set(CP_fD, r.logRange(0.05f, 4.0f));
        set(CP_fR, r.logRange(0.05f, 3.0f));
        set(CP_vcfLevel, r.range(0.6f, 1.0f));
        // R4: every note is audible for a while: sustained, or a decay of at least 0.4 s.
        set(CP_vA, r.logRange(0.002f, b.maxAttackSeconds));
        const bool percussive = r.chance(0.25f);
        set(CP_vD, percussive ? r.logRange(0.4f, 3.0f) : r.logRange(0.1f, 4.0f));
        set(CP_vS, percussive ? r.range(0.0f, 0.3f) : r.range(0.3f, 1.0f));
        set(CP_vR, r.logRange(0.05f, 3.0f));
        // R5: bounded total level and touch.
        const float level = r.range(0.5f, 0.85f);
        const float touchLevel = std::max(0.0f, b.maxLevelSum - level);
        set(CP_vLevel, level);
        set(CP_initLevel, std::min(r.range(0.0f, 0.3f), touchLevel));
        set(CP_afterLevel, std::min(r.range(0.0f, 0.3f), std::max(0.0f, touchLevel - v[chParam(ch, CP_initLevel)])));
        set(CP_initBrill, r.range(0.0f, 0.4f));
        set(CP_afterBrill, r.range(0.0f, 0.5f));
    }
    v[P_detune] = r.range(0.0f, 20.0f);
    v[P_mix] = r.range(0.25f, 0.75f);
    v[P_brilliance] = r.range(-0.6f, 0.6f);
    // R6: combined resonance stays below self-oscillation.
    const float rankMax = std::max(rankRes[0], rankRes[1]);
    v[P_resonance] = std::min(r.range(0.0f, 0.55f), std::max(0.0f, (b.maxCombinedQ - 0.8f * rankMax) / 0.6f));
    v[P_initBend] = r.chance(0.3f) ? r.range(0.0f, 0.3f) : 0.0f;
    // Original SUB: six functions minus EXT (it needs an external signal).
    v[P_subOriginalFunction] = (float) (int) (r.unit() * 5.0f);
    v[P_subOriginalSpeed] = r.logRange(0.7f, 20.0f);
    v[P_subVco] = r.chance(0.4f) ? r.range(0.0f, 0.15f) : 0.0f;
    v[P_subVcf] = r.chance(0.4f) ? r.range(0.0f, 0.3f) : 0.0f;
    v[P_subVca] = r.chance(0.25f) ? r.range(0.0f, 0.25f) : 0.0f;
    v[P_subAtSpeed] = r.range(0.0f, 0.4f);
    v[P_subAtVco] = r.chance(0.5f) ? r.range(0.0f, 0.4f) : 0.0f;
    v[P_subAtVcf] = r.chance(0.3f) ? r.range(0.0f, 0.4f) : 0.0f;
    // R7: ring modulator used sparingly and never at full multiplication.
    v[P_ringAmount] = r.chance(0.25f) ? r.range(0.0f, b.maxRingAmount) : 0.0f;
    v[P_ringSpeed] = r.logRange(0.5f, 400.0f);
    v[P_ringDepth] = r.range(0.0f, 0.5f);
    v[P_ringAttack] = r.logRange(0.001f, 2.0f);
    v[P_ringDecay] = r.logRange(0.05f, 3.0f);
    v[P_chorusOn] = r.chance(0.6f) ? 1.0f : 0.0f;
    v[P_chorusMode] = r.chance(0.2f) ? 1.0f : 0.0f;
    v[P_chorusSpeed] = r.logRange(0.2f, 6.0f);
    v[P_chorusDepth] = r.range(0.2f, 0.9f);
    for (int i : { (int) P_kbdBrillianceLow, (int) P_kbdBrillianceHigh, (int) P_kbdLevelLow, (int) P_kbdLevelHigh })
        v[i] = r.chance(0.3f) ? r.range(-0.4f, 0.4f) : 0.0f;
    // R8: everything else is copied from the base unchanged (policy, VOLUME,
    // physical setup, tuning, ribbon, sustain, glide, FX/LFO/arp/macros, pan,
    // HPF/LPF bypass, legacy SUB controls and parameters appended later).
    return v;
}

// The parameters randomiseWithinOriginalRanges() may change (for tests and UI).
inline bool isRandomisedParameter(int index) noexcept
{
    for (int ch = 0; ch < 2; ++ch)
        for (int local : { (int) CP_saw, (int) CP_pulse, (int) CP_sine, (int) CP_noise, (int) CP_pw, (int) CP_pwmDepth,
                           (int) CP_pwmSpeed, (int) CP_feet, (int) CP_hpfCutoff, (int) CP_hpfRes, (int) CP_lpfCutoff,
                           (int) CP_lpfRes, (int) CP_fIl, (int) CP_fAl, (int) CP_fA, (int) CP_fD, (int) CP_fR,
                           (int) CP_vcfLevel, (int) CP_vA, (int) CP_vD, (int) CP_vS, (int) CP_vR, (int) CP_vLevel,
                           (int) CP_initLevel, (int) CP_afterLevel, (int) CP_initBrill, (int) CP_afterBrill })
            if (index == chParam(ch, local)) return true;
    switch (index)
    {
        case P_detune: case P_mix: case P_brilliance: case P_resonance: case P_initBend:
        case P_subOriginalFunction: case P_subOriginalSpeed: case P_subVco: case P_subVcf: case P_subVca:
        case P_subAtSpeed: case P_subAtVco: case P_subAtVcf: case P_ringAmount: case P_ringSpeed: case P_ringDepth:
        case P_ringAttack: case P_ringDecay: case P_chorusOn: case P_chorusMode: case P_chorusSpeed: case P_chorusDepth:
        case P_kbdBrillianceLow: case P_kbdBrillianceHigh: case P_kbdLevelLow: case P_kbdLevelHigh:
            return true;
        default:
            return false;
    }
}
} // namespace cs80::library

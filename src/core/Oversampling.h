#pragma once

#include <algorithm>

namespace cs80
{
// Oversampling quality (IMPROVEMENT_PLAN 1.6). Numerical engine quality, not an
// original CS-80 control: it is not masked by PURE CS-80.
//
// The mode names a factor at a 44.1/48 kHz host. At higher host rates the factor
// shrinks so the INTERNAL rate, which is what sets where aliases fold, stays the
// same: a 96 kHz host at 2x already runs at the internal rate of 48 kHz at 4x.
// No mode goes below 2x, the architecture's minimum.
enum class OversamplingMode { eco = 0, standard = 1, hq = 2 };

inline constexpr int kEcoOversampling = 2;
// INTEGRATOR CONSTANT. The circuit engine's Standard factor at 44.1/48 kHz: the
// lowest factor whose measured worst non-harmonic energy meets the -70 dB alias
// target inside the realtime CPU budget, or, when no factor does both, the
// largest factor inside that budget. Chosen by cs80_engine_audit
// --suite alias,cpu on the current oscillator/filter (docs/workstreams/ws2-engine.md);
// re-measure and change this one value when the rank chain changes.
//
// 2026-10-31, after the ws1 merge (reconstructed oscillator, nodal IG00156
// stages), cs80_engine_audit --suite alias --rates 44100,48000,96000, circuit
// engine, ring modulator off, worst row:
//   Eco 2x  -88.5 / -88.3 / -89.5 dB    Standard 4x -88.9 / -89.1 / -89.5 dB
//   HQ 8x   -88.9 / -89.2 / -89.5 dB    (44.1 / 48 / 96 kHz hosts)
// 2x already meets -70 dB with 18 dB to spare, so Standard = 2x by the rule
// (4x cost about 1.6x the 2x CPU). The ring-on rows read -43 dB at EVERY factor:
// that is ws4's declared RMO carrier feedthrough (a spectral line the alias
// metric counts as non-harmonic), not fold-over, so it does not choose a
// factor. What 4x/8x still buy is the nodal filter's top-octave small-signal
// accuracy (ws1 step 1.1b); offline bounces always render at HQ.
// Previously 4 (ws2, PolyBLEP oscillator: no factor reached -70 dB).
inline constexpr int kStandardOversampling = 2;
inline constexpr int kHqOversampling = 8;
inline constexpr int kMaxOversampling = kHqOversampling;

inline constexpr OversamplingMode oversamplingMode(float parameter) noexcept
{
    return parameter >= 1.5f ? OversamplingMode::hq
         : parameter >= 0.5f ? OversamplingMode::standard
                             : OversamplingMode::eco;
}

// Largest power of two not above hostRate / 44.1 kHz (1 at 44.1/48 kHz).
inline constexpr int hostRateMultiple(double hostRate) noexcept
{
    int multiple = 1;
    while (multiple < kMaxOversampling && hostRate >= 44100.0 * (double) (multiple * 2))
        multiple *= 2;
    return multiple;
}

// Effective factor. Legacy (compat) keeps its historical 2x for Eco and Standard
// and ignores the offline override, so an old session renders and bounces
// exactly as it always did; only an explicit HQ selection changes it. The
// circuit engine uses the measured Standard factor and renders every offline
// bounce at HQ.
inline constexpr int oversamplingFactor(OversamplingMode mode, bool circuitEngine, bool offline,
                                        double hostRate) noexcept
{
    int atBaseRate = kEcoOversampling;
    if (circuitEngine && offline)
        atBaseRate = kHqOversampling;
    else if (mode == OversamplingMode::hq)
        atBaseRate = kHqOversampling;
    else if (mode == OversamplingMode::standard && circuitEngine)
        atBaseRate = kStandardOversampling;
    return std::max(kEcoOversampling, atBaseRate / hostRateMultiple(hostRate));
}

// The largest factor any mode can select at this host rate; rate-dependent
// buffers are reserved for it so a quality change never allocates.
inline constexpr int maximumOversamplingFactor(double hostRate) noexcept
{
    return oversamplingFactor(OversamplingMode::hq, true, true, hostRate);
}

static_assert(oversamplingFactor(OversamplingMode::standard, false, true, 48000.0) == 2,
              "legacy Standard and offline bounce stay at the historical 2x");
static_assert(oversamplingFactor(OversamplingMode::hq, false, false, 44100.0) == 8);
static_assert(oversamplingFactor(OversamplingMode::eco, true, true, 48000.0) == 8,
              "circuit-engine offline bounce is always HQ");
static_assert(oversamplingFactor(OversamplingMode::hq, true, false, 96000.0) == 4);
static_assert(oversamplingFactor(OversamplingMode::hq, true, false, 192000.0) == 2);
} // namespace cs80

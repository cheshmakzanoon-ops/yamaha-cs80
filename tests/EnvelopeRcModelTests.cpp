// Circuit-engine envelope RC law (docs/ENVELOPE_RC_MODEL.md, IMPROVEMENT_PLAN 1.3).
// These check the declared INFERRED law and its engine wiring, not hardware.
#include "core/Dsp.h"
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* text) { ++checks; if (!ok) { ++failures; std::cerr << text << '\n'; } }

// Samples until the amplitude attack leaves the Attack stage.
int adsrAttackSamples(float rate, float seconds, float startLevel)
{
    Adsr env;
    // Bring the capacitor to startLevel with an instant attack and a long decay
    // toward that level, then retrigger from it (the circuit-engine reuse path).
    env.noteOn(0.0001f, 0.001f, startLevel, 10.0f);
    for (int i = 0; i < (int) (rate * 0.05f); ++i) env.tick(rate, true, true);
    env.noteOn(seconds, 1.0f, 0.5f, 1.0f);
    int n = 0;
    while (env.getState().stage == Adsr::Stage::Attack && n < (int) (rate * seconds * 3.0f))
    {
        env.tick(rate, true, true);
        ++n;
    }
    return n;
}

void attackReachesPeakAtT()
{
    for (float rate : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
        for (float seconds : { 0.01f, 0.2f, 1.0f })
            for (float start : { 0.0f, 0.3f, 0.7f })
            {
                const int n = adsrAttackSamples(rate, seconds, start);
                check(std::abs(n - seconds * rate) <= 1.5f, "amplitude attack reaches its peak at T from any start level");
            }
    // Filter IL -> AL, both directions, reaches AL at T.
    for (float rate : { 48000.0f, 96000.0f })
        for (auto levels : { std::array<float, 2> { -2.0f, 1.5f }, std::array<float, 2> { 2.0f, -1.0f } })
        {
            FilterEnv env;
            env.noteOn(levels[0], levels[1], 0.25f, 1.0f, 1.0f, 0.0f);
            int n = 0;
            while (env.getState().stage == FilterEnv::Stage::Attack && n < (int) rate) { env.tick(rate, true, true); ++n; }
            check(std::abs(n - 0.25f * rate) <= 1.5f, "filter attack reaches AL at T in either direction");
        }
}

void attackIsConcaveCharge()
{
    // Charging toward an overshoot target: slope is largest at the start and
    // falls monotonically (an RC curve, not linear and not a time-window bend).
    Adsr env;
    env.noteOn(0.1f, 1.0f, 0.5f, 1.0f);
    float previous = 0.0f, previousStep = 1.0f;
    bool concave = true;
    for (int i = 0; i < 4700; ++i)
    {
        const float v = env.tick(48000.0f, true, true);
        const float step = v - previous;
        if (step > previousStep + 1e-7f || step <= 0.0f) concave = false;
        previous = v; previousStep = step;
    }
    check(concave, "attack is a monotonic concave RC charge");
}

void settleIsMinus60dBAtT()
{
    for (float rate : { 44100.0f, 96000.0f })
    {
        Adsr release;
        release.noteOn(0.0001f, 0.0001f, 1.0f, 0.5f);
        for (int i = 0; i < 64; ++i) release.tick(rate, true, true);
        release.noteOff();
        for (int i = 0; i < (int) std::lround(0.5f * rate); ++i) release.tick(rate, true, true);
        check(std::abs(release.getValue() - 0.001f) < 0.001f * 0.01f, "release settles to -60 dB at T");

        Adsr decay;
        decay.noteOn(0.0001f, 0.8f, 0.2f, 1.0f);
        decay.tick(rate, true, true);
        const double span = decay.getValue() - 0.2;
        for (int i = 1; i < (int) std::lround(0.8f * rate); ++i) decay.tick(rate, true, true);
        // The stage snaps to S once within 0.0005, so read just before -60 dB.
        check(decay.getValue() - 0.2f < span * 0.0011 && decay.getValue() - 0.2f >= 0.0f,
              "decay settles to -60 dB of its span at T");
    }
}

void noTimeWindowBend()
{
    // The legacy x5 windows ended at 35/40/45 ms; the circuit-engine law has a
    // constant ratio between successive samples throughout decay and release.
    Adsr release;
    release.noteOn(0.0001f, 0.0001f, 1.0f, 2.0f);
    for (int i = 0; i < 64; ++i) release.tick(48000.0f, true, true);
    release.noteOff();
    double previous = release.tick(48000.0f, true, true), ratio0 = 0.0;
    bool constant = true;
    for (int i = 0; i < 4800; ++i)
    {
        const double v = release.tick(48000.0f, true, true);
        const double ratio = v / previous;
        if (i == 0) ratio0 = ratio;
        else if (std::abs(ratio - ratio0) > 1e-6) constant = false;
        previous = v;
    }
    check(constant, "release has no time-window bend");
}

void filterEnvelopeContinuesOnReuse()
{
    // A sounding release tail reused by a new key must not jump to IL.
    auto p = defaultValues();
    p[P_sourcePerformance] = 1;
    p[P_masterSustain] = 0;
    for (int ch = 0; ch < 2; ++ch)
    {
        p[chParam(ch, CP_fIl)] = 0.0f;   // IL far from AL
        p[chParam(ch, CP_fAl)] = 1.0f;
        p[chParam(ch, CP_fA)] = 0.001f;
        p[chParam(ch, CP_fD)] = 2.0f;
        p[chParam(ch, CP_fR)] = 2.0f;
        p[chParam(ch, CP_vR)] = 2.0f;
    }
    Engine e;
    e.setParams(p);
    e.prepare(48000.0);
    std::array<float, 512> out {};
    for (int n = 0; n < 8; ++n) e.noteOn(48 + n, 0.7f);
    for (int b = 0; b < 40; ++b) e.render(out.data(), 256);
    for (int n = 0; n < 8; ++n) e.noteOff(48 + n);
    for (int b = 0; b < 8; ++b) e.render(out.data(), 256);
    // A slow retrigger attack makes continuity visible as a tiny first step,
    // while a reset to IL (several octaves away) would be a large one.
    for (int ch = 0; ch < 2; ++ch) p[chParam(ch, CP_fA)] = 0.2f;
    e.setParams(p);
    const auto before = e.getRankEnvelopeState(0, 0).filter.value;
    e.noteOn(72, 0.7f); // all voices are sounding tails; one is reused
    int reused = -1;
    for (int v = 0; v < 8; ++v) if (e.getVoiceInfo(v).note == 72) reused = v;
    check(reused >= 0, "a sounding tail is reused");
    if (reused < 0) return;
    const auto prior = reused == 0 ? before : e.getRankEnvelopeState(reused, 0).filter.value;
    e.render(out.data(), 1);
    const auto after = e.getRankEnvelopeState(reused, 0).filter.value;
    check(std::abs(after - prior) < 0.01f && after > prior, "reused filter envelope charges on from its present level");

    // Legacy keeps its historical reset; that path is pinned byte-for-byte by
    // the legacy-compatibility probe rather than asserted here.
}
} // namespace

int main()
{
    attackReachesPeakAtT();
    attackIsConcaveCharge();
    settleIsMinus60dBAtT();
    noTimeWindowBend();
    filterEnvelopeContinuesOnReuse();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

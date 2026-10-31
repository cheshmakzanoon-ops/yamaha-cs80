// Modern-layer LFO waveforms (src/core/ModulationLfo.h) against an independent
// double-precision description of each shape. These are modern software shapes, not
// CS-80 circuitry; agreement here says nothing about the instrument.
#include "core/Engine.h"
#include "core/ModulationLfo.h"
#include "ModernLayerTolerances.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace cs80;
namespace tol = cs80::tests::ws5;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (! ok)
    {
        if (failures < 30)
            std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr double kPi = 3.14159265358979323846;

// Independent analytic shapes, phase in [0, 1), all phase-aligned with sin(2 pi phase).
double truth(int wave, double phase)
{
    switch (wave)
    {
        case 1: // triangle: 0 rising, +1 at 1/4, 0 at 1/2, -1 at 3/4
            return phase < 0.25 ? 4.0 * phase : phase < 0.75 ? 2.0 - 4.0 * phase : 4.0 * phase - 4.0;
        case 2: return phase < 0.5 ? 2.0 * phase : 2.0 * phase - 2.0;   // saw up through 0
        case 3: return phase < 0.5 ? -2.0 * phase : 2.0 - 2.0 * phase;  // saw down through 0
        case 4: return phase < 0.5 ? 1.0 : -1.0;                         // square
        default: return 0.0;
    }
}

void testSineIsHistoricalLaw()
{
    // The default waveform must be byte-identical to the Lfo law it replaces in the engine.
    for (float sr : { 88200.0f, 96000.0f, 192000.0f, 384000.0f })
        for (float rate : { 0.05f, 0.28f, 1.1f, 7.3f, 20.0f })
        {
            Lfo legacy;
            ModulationLfo modern;
            Rng rng;
            legacy.reset(0.0f);
            modern.reset(0.0f);
            bool same = true;
            for (int n = 0; n < 200000 && same; ++n)
            {
                const float a = legacy.tick(rate, sr, 0, rng);
                const float b = modern.tick(rate, sr, 0);
                same = std::memcmp(&a, &b, sizeof a) == 0;
            }
            check(same, "sine waveform is bit-identical to the historical Lfo law");
        }
}

void testAnalyticShapes()
{
    for (int wave = 1; wave <= 4; ++wave)
        for (float sr : { 88200.0f, 96000.0f, 384000.0f })
            for (float rate : { 0.05f, 3.7f, 20.0f })
            {
                ModulationLfo lfo;
                lfo.reset(0.0f);
                double worst = 0.0;
                const int length = (int) std::min(400000.0, 3.0 * sr / rate);
                for (int n = 0; n < length; ++n)
                {
                    const float value = lfo.tick(rate, sr, wave);
                    const double phase = lfo.getShapePhase();
                    // Skip the one sample on either side of a discontinuity (saw, square).
                    const bool edge = (wave >= 2) && (std::abs(phase - 0.5) * sr / rate < 1.5
                                                      || std::min(phase, 1.0 - phase) * sr / rate < 1.5);
                    if (! edge)
                        worst = std::max(worst, std::abs((double) value - truth(wave, phase)));
                }
                check(worst <= tol::kLfoShapeAbsTolerance, "LFO shape matches its analytic definition");
            }

    // Phase alignment with the sine: zero (or the period mean) at the start, positive first
    // (saw down is the mirror image and goes negative first).
    for (int wave = 0; wave <= 4; ++wave)
    {
        ModulationLfo lfo;
        lfo.reset(0.0f);
        const float first = lfo.tick(1.0f, 96000.0f, wave);
        float quarter = 0.0f;
        for (int n = 1; n < 24000; ++n)
            quarter = lfo.tick(1.0f, 96000.0f, wave);
        const float sign = wave == 3 ? -1.0f : 1.0f;
        check(sign * quarter > 0.45f && (wave == 4 || std::abs(first) < 1.0e-3f),
              "LFO shapes start at their mean and rise first, like the sine");
    }
}

void testPeriods()
{
    // Wrap times of the double phase against k * sr / rate, over the whole run.
    for (float sr : { 88200.0f, 96000.0f, 192000.0f })
        for (float rate : { 0.05f, 0.9f, 13.3f })
        {
            ModulationLfo lfo;
            lfo.reset(0.0f);
            double previous = 0.0;
            int wraps = 0;
            double worst = 0.0;
            const int length = (int) std::max(2.5 * sr / rate, std::min(2.0e6, 40.0 * sr / rate));
            for (int n = 1; n <= length; ++n)
            {
                lfo.tick(rate, sr, 1);
                const double phase = lfo.getShapePhase();
                if (phase < previous)
                {
                    ++wraps;
                    const double expected = (double) wraps * (double) sr / (double) rate;
                    worst = std::max(worst, std::abs((double) n - expected));
                }
                previous = phase;
            }
            check(wraps >= 1, "LFO wraps during the run");
            check(worst <= tol::kLfoPeriodToleranceSamples, "LFO period is rate-exact over the run");
        }
}

void testRandomShapes()
{
    constexpr float sr = 96000.0f, rate = 5.0f;
    // Sample & hold: constant inside a period, a new level at each period start.
    {
        ModulationLfo lfo;
        lfo.reset(0.0f);
        float previous = lfo.tick(rate, sr, 5);
        double previousPhase = lfo.getShapePhase();
        bool constantInside = true, inRange = true;
        int changes = 0, wraps = 0;
        double sum = 0.0;
        const int length = (int) (2000.0f * sr / rate);
        for (int n = 1; n < length; ++n)
        {
            const float value = lfo.tick(rate, sr, 5);
            const double phase = lfo.getShapePhase();
            const bool wrapped = phase < previousPhase;
            wraps += wrapped ? 1 : 0;
            if (! wrapped && value != previous) constantInside = false;
            if (wrapped && value != previous) ++changes;
            inRange = inRange && std::abs(value) <= 1.0f;
            sum += value;
            previous = value;
            previousPhase = phase;
        }
        check(constantInside, "S&H holds its level for a whole period");
        check(changes >= wraps - 2 && wraps >= 1999, "S&H takes a new level at each period start");
        check(inRange && std::abs(sum / length) < 0.06, "S&H levels are bipolar, bounded and centred");
    }
    // Smooth random: continuous with slope bounded by the raised-cosine law.
    {
        ModulationLfo lfo;
        lfo.reset(0.0f);
        float previous = lfo.tick(rate, sr, 6);
        const double maxStep = kPi * (double) rate / (double) sr * 1.0001 + 1.0e-6;
        bool smooth = true, inRange = true;
        for (int n = 1; n < (int) (500.0f * sr / rate); ++n)
        {
            const float value = lfo.tick(rate, sr, 6);
            smooth = smooth && std::abs((double) value - (double) previous) <= maxStep;
            inRange = inRange && std::abs(value) <= 1.0f;
            previous = value;
        }
        check(smooth, "smooth random never steps faster than its raised-cosine slope bound");
        check(inRange, "smooth random stays in -1..1");
    }
    // Each LFO owns its stream: reset repeats it, a different seed decorrelates it.
    {
        ModulationLfo a(1u), b(1u), c(2u);
        a.reset(); b.reset(); c.reset();
        bool repeat = true, differ = false;
        for (int n = 0; n < 50000; ++n)
        {
            const float x = a.tick(40.0f, sr, 5), y = b.tick(40.0f, sr, 5), z = c.tick(40.0f, sr, 5);
            repeat = repeat && x == y;
            differ = differ || x != z;
        }
        a.reset();
        ModulationLfo fresh(1u);
        fresh.reset();
        for (int n = 0; n < 1000; ++n)
            repeat = repeat && a.tick(40.0f, sr, 5) == fresh.tick(40.0f, sr, 5);
        check(repeat && differ, "random shapes are reproducible after reset and independent per seed");
    }
}

std::vector<float> render(Engine& e, int frames)
{
    std::vector<float> out((std::size_t) frames * 2);
    e.render(out.data(), frames);
    return out;
}

void testEngineHook()
{
    // Depth zero: the waveform choice is inert. Depth > 0: each waveform is audible and
    // distinct. PURE CS-80: the waveform choice changes nothing.
    auto run = [](int wave, float depth, bool pure)
    {
        ParamValues p;
        p[P_lfo1Depth] = depth;
        p[P_lfo1Dest] = 1.0f;
        p[P_lfo1Rate] = 6.0f;
        p[P_lfo1Wave] = (float) wave;
        p[P_parityMode] = pure ? 1.0f : 0.0f;
        Engine e;
        e.prepare(48000.0);
        e.setParams(p);
        e.noteOn(60, 0.8f);
        return render(e, 24000);
    };
    const auto reference = run(0, 0.0f, false);
    bool inert = true;
    for (int wave = 1; wave < kLfoWaveCount; ++wave)
        inert = inert && run(wave, 0.0f, false) == reference;
    check(inert, "LFO waveform is inert at zero depth");
    const auto sine = run(0, 0.6f, false);
    for (int wave = 1; wave < kLfoWaveCount; ++wave)
        check(run(wave, 0.6f, false) != sine, "every LFO waveform is audible at depth");
    const auto pure = run(0, 0.6f, true);
    bool pureInert = true;
    for (int wave = 1; wave < kLfoWaveCount; ++wave)
        pureInert = pureInert && run(wave, 0.6f, true) == pure;
    check(pureInert, "LFO waveform choice is inert under PURE CS-80");
}

} // namespace

int main()
{
    testSineIsHistoricalLaw();
    testAnalyticShapes();
    testPeriods();
    testRandomShapes();
    testEngineHook();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

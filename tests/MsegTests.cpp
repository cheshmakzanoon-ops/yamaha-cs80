// Multi-segment envelope runner (src/core/Mseg.h) against the closed-form reference in
// src/reference/MsegReference.h: timing, curves, loops, release and tempo sync. A modern
// control; agreement with the reference says nothing about the CS-80.
#include "core/Mseg.h"
#include "reference/MsegReference.h"
#include "ModernLayerTolerances.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace cs80::modern;
using cs80::reference::msegReferenceLevel;
using cs80::reference::msegReferenceHeldPosition;
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

MsegShape makeShape(std::initializer_list<MsegPoint> points, MsegLoop loop, int loopStart, int loopEnd)
{
    MsegShape s;
    s.count = 0;
    for (const auto& p : points)
        s.points[(std::size_t) s.count++] = p;
    s.loop = loop;
    s.loopStart = loopStart;
    s.loopEnd = loopEnd;
    return sanitizeShape(s);
}

std::vector<MsegShape> fixtureShapes()
{
    std::vector<MsegShape> shapes;
    for (int i = 0; i < kMsegCount; ++i)
        shapes.push_back(defaultShape(i));
    // Mixed curves, a zero-length step and a loop that starts later than the first point.
    for (auto loop : { MsegLoop::off, MsegLoop::sustain, MsegLoop::always })
    {
        shapes.push_back(makeShape({ { 0.0f, 0.2f, -0.8f }, { 0.13f, 0.9f, 0.35f }, { 0.13f, 0.1f, 1.0f },
                                     { 0.41f, 0.75f, -1.0f }, { 0.77f, 0.3f, 0.0f }, { 1.1f, 0.0f, 0.6f } },
                                   loop, 1, 4));
        shapes.push_back(makeShape({ { 0.0f, 0.0f, 0.2f }, { 0.25f, 1.0f, 0.0f }, { 0.6f, 0.5f, -0.3f },
                                     { 0.9f, 0.0f, 0.0f } },
                                   loop, 2, 2)); // sustain point
    }
    return shapes;
}

double runAgainstReference(const MsegShape& s, double advance, long ticks, long releaseTick)
{
    MsegRunner runner;
    runner.trigger(s);
    double worst = 0.0;
    for (long k = 1; k <= ticks; ++k)
    {
        const bool gate = releaseTick < 0 || k < releaseTick;
        const double value = runner.tick(s, advance, gate);
        worst = std::max(worst, std::abs(value - msegReferenceLevel(s, advance, k, releaseTick)));
    }
    return worst;
}

void testCurveLaw()
{
    bool ends = true, monotone = true, matches = true;
    for (double c : { -1.0, -0.37, 0.0, 0.2, 1.0 })
    {
        ends = ends && std::abs(msegCurve(0.0, c)) < 1e-12 && std::abs(msegCurve(1.0, c) - 1.0) < 1e-12;
        double previous = -1.0;
        for (int i = 0; i <= 1000; ++i)
        {
            const double t = i / 1000.0, y = msegCurve(t, c);
            monotone = monotone && y >= previous;
            previous = y;
            matches = matches && std::abs(y - cs80::reference::msegReferenceCurve(t, c)) < 1e-12;
        }
    }
    check(ends && monotone && matches, "segment curve law runs 0 to 1, monotone, as specified");
    // c = +1 is an RC charge that is 60 dB from its target at the end of a unit segment.
    const double rc = (1.0 - std::exp(-6.907755278982137 * 0.5)) / (1.0 - 0.001);
    check(std::abs(msegCurve(0.5, 1.0) - rc) < 1e-12, "fully curved segment is the ln(1000) RC charge");
}

void testHeldLevels()
{
    for (const auto& shape : fixtureShapes())
        for (double sr : { 88200.0, 96000.0, 384000.0 })
            for (double rate : { 0.5, 1.0, 3.3 })
            {
                const double advance = msegAdvance(rate, false, 0.0, sr);
                const long ticks = (long) std::min(6.0e5, 4.5 * (double) shape.endTime() / advance + 10.0);
                check(runAgainstReference(shape, advance, ticks, -1) <= tol::kMsegLevelAbsTolerance,
                      "held MSEG level matches the closed-form reference");
            }
}

void testRelease()
{
    for (const auto& shape : fixtureShapes())
        for (double fraction : { 0.03, 0.2, 0.55, 0.9, 1.7, 3.1 })
        {
            const double sr = 96000.0, advance = msegAdvance(1.3, false, 0.0, sr);
            const long releaseTick = 1 + (long) (fraction * (double) shape.endTime() / advance);
            const long ticks = releaseTick + (long) (1.2 * (double) shape.endTime() / advance) + 10;
            check(runAgainstReference(shape, advance, ticks, releaseTick) <= tol::kMsegLevelAbsTolerance,
                  "MSEG release (loop exit and level continuity) matches the reference");
        }
    // Releasing mid-attack starts the release from the level reached: no step at all.
    const auto env = defaultShape(0);
    MsegRunner runner;
    runner.trigger(env);
    const double advance = msegAdvance(1.0, false, 0.0, 96000.0);
    float last = 0.0f, worstStep = 0.0f;
    for (int k = 1; k < 96000; ++k)
    {
        const float v = runner.tick(env, advance, k < 3000);
        if (k > 1) worstStep = std::max(worstStep, std::abs(v - last));
        last = v;
    }
    check(worstStep < 2.0e-3f && last == 0.0f, "early release is continuous and reaches the final level");
}

long firstTickAtOrAfter(double time, double advance)
{
    long k = (long) std::ceil(time / advance);
    while ((double) k * advance < time) ++k;
    while (k > 0 && (double) (k - 1) * advance >= time) --k;
    return k;
}

void testBoundaries()
{
    // A vertical step at t = X: the first tick at or after X must show the new level.
    for (double sr : { 44100.0, 96000.0, 384000.0 })
        for (float stepAt : { 0.1f, 0.4567f, 1.25f })
        {
            const auto s = makeShape({ { 0.0f, 0.0f, 0.0f }, { stepAt, 0.0f, 0.0f }, { stepAt, 1.0f, 0.0f },
                                       { stepAt + 0.5f, 1.0f, 0.0f } }, MsegLoop::off, 0, 0);
            const double advance = msegAdvance(1.0, false, 0.0, sr);
            MsegRunner runner;
            runner.trigger(s);
            long seen = -1;
            for (long k = 1; k < (long) (2.0 * sr) && seen < 0; ++k)
                if (runner.tick(s, advance, true) > 0.5f) seen = k;
            check(std::abs((double) (seen - firstTickAtOrAfter(stepAt, advance))) <= tol::kMsegBoundaryToleranceSamples,
                  "MSEG segment boundary lands on its analytic sample");
        }
    // The release jump acts on the tick whose gate is low.
    const auto s = makeShape({ { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 0.5f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f } },
                             MsegLoop::sustain, 0, 1);
    MsegRunner runner;
    runner.trigger(s);
    const double advance = msegAdvance(1.0, false, 0.0, 96000.0);
    long dropped = -1;
    for (long k = 1; k < 200000 && dropped < 0; ++k)
        if (runner.tick(s, advance, k < 123457) < 0.5f) dropped = k;
    check(std::abs((double) (dropped - 123457)) <= tol::kMsegBoundaryToleranceSamples, "release jump is sample-exact");
}

void testLoopDrift()
{
    for (double sr : { 44100.0, 96000.0, 192000.0 })
        for (float length : { 0.0371f, 0.37f, 1.9f })
        {
            const auto s = makeShape({ { 0.0f, 0.0f, 0.0f }, { 0.05f, 1.0f, 0.0f }, { 0.05f + length, 0.0f, 0.0f } },
                                     MsegLoop::always, 1, 2);
            const double advance = msegAdvance(1.0, false, 0.0, sr);
            MsegRunner runner;
            runner.trigger(s);
            double previous = 0.0;
            int wraps = 0;
            long lastWrap = 0;
            const long ticks = (long) ((0.05 + 100.5 * length) / advance);
            for (long k = 1; k <= ticks; ++k)
            {
                runner.tick(s, advance, true);
                if (runner.getPosition() < previous) { ++wraps; lastWrap = k; }
                previous = runner.getPosition();
            }
            const double expected = (double) firstTickAtOrAfter(0.05 + 100.0 * (double) length, advance);
            check(wraps == 100, "looping MSEG wraps once per loop");
            check(std::abs((double) lastWrap - expected) <= tol::kMsegLoopDriftSamplesPer100Loops,
                  "loop period does not drift over 100 loops");
        }
}

void testSync()
{
    // One beat lasts 60 / bpm seconds; no host tempo falls back to 120 BPM.
    const double sr = 96000.0;
    check(effectiveBpm(std::numeric_limits<double>::quiet_NaN()) == tol::kMsegFallbackBpm
              && effectiveBpm(0.0) == tol::kMsegFallbackBpm && effectiveBpm(-5.0) == tol::kMsegFallbackBpm
              && kMsegFallbackBpm == tol::kMsegFallbackBpm,
          "missing or unusable host tempo falls back to 120 BPM");
    for (double bpm : { 120.0, 90.0, 174.0, 0.0 })
        for (double rate : { 1.0, 0.5 })
        {
            const auto s = makeShape({ { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f } },
                                     MsegLoop::off, 0, 0);
            const double advance = msegAdvance(rate, true, bpm, sr);
            MsegRunner runner;
            runner.trigger(s);
            long top = -1;
            float previous = 0.0f;
            for (long k = 1; k < (long) (4.0 * sr) && top < 0; ++k)
            {
                const float v = runner.tick(s, advance, true);
                if (v < previous) top = k; // first tick past the one-beat peak
                previous = v;
            }
            const double beatSeconds = 60.0 / effectiveBpm(bpm) / rate;
            check(std::abs((double) top - std::ceil(beatSeconds * sr)) <= tol::kMsegBoundaryToleranceSamples,
                  "synced MSEG beat lasts 60/bpm seconds");
        }
    // Transport lock: seeking to beat position q gives the level the shape has there.
    const auto loop = defaultShape(1);
    MsegRunner runner;
    bool locked = true;
    for (double q : { 0.0, 0.3, 0.999, 1.0, 7.25, 1234.567 })
    {
        runner.seek(loop, q);
        const double expected = cs80::reference::msegReferenceStatic(
            loop, msegReferenceHeldPosition(loop, q, 1), 0, loop.points[0].level);
        locked = locked && std::abs(runner.getValue() - expected) <= tol::kMsegLevelAbsTolerance;
    }
    check(locked, "transport-locked MSEG lands on the shape's level at the song position");
}

void testSanitize()
{
    MsegShape bad;
    bad.count = 99;
    for (int i = 0; i < kMsegMaxPoints; ++i)
        bad.points[(std::size_t) i] = { (float) (kMsegMaxPoints - i), i % 3 == 0 ? 7.0f : -2.0f, i % 2 ? 9.0f : -9.0f };
    bad.points[3].time = std::numeric_limits<float>::quiet_NaN();
    bad.loopStart = 40;
    bad.loopEnd = -3;
    bad.loop = (MsegLoop) 9;
    bad.trigger = (MsegTrigger) -1;
    const auto s = sanitizeShape(bad);
    bool valid = s.count == kMsegMaxPoints && s.points[0].time == 0.0f && s.loopStart <= s.loopEnd
                 && s.loopEnd < s.count && s.loop == MsegLoop::off && s.trigger == MsegTrigger::poly;
    for (int i = 0; i < s.count; ++i)
    {
        const auto& p = s.points[(std::size_t) i];
        valid = valid && std::isfinite(p.time) && p.time <= kMsegMaxTime && p.level >= 0.0f && p.level <= 1.0f
                && p.curve >= -1.0f && p.curve <= 1.0f && (i == 0 || p.time >= s.points[(std::size_t) i - 1].time);
    }
    check(valid, "sanitized shape is inside the valid domain");
    check(sanitizeShape(s) == s, "sanitizing is idempotent");
    for (int i = 0; i < kMsegCount; ++i)
        check(sanitizeShape(defaultShape(i)) == defaultShape(i), "default shapes are valid");
}

void testRunnerLifecycle()
{
    const auto s = defaultShape(0);
    MsegRunner runner;
    check(runner.tick(s, 1.0e-4, true) == 0.0f && ! runner.isRunning(), "an untriggered MSEG is silent");
    runner.trigger(s);
    for (int k = 0; k < 20000; ++k) runner.tick(s, 1.0e-4, true);
    const float held = runner.getValue();
    for (int k = 0; k < 20000; ++k) runner.tick(s, 1.0e-4, true);
    check(std::abs(held - 0.6f) < 1e-6f && runner.getValue() == held, "sustain point holds while the gate is held");
    runner.trigger(s);
    check(runner.getPosition() == 0.0 && ! runner.isReleased(), "trigger restarts the envelope");
    // A loop-off shape ignores the gate and holds its final level.
    const auto ramp = defaultShape(2);
    runner.trigger(ramp);
    for (int k = 0; k < 40000; ++k) runner.tick(ramp, 1.0e-4, k < 10);
    check(runner.getValue() == 1.0f && ! runner.isRunning(), "one-shot shape plays through and holds its end");
}

} // namespace

int main()
{
    testCurveLaw();
    testHeldLevels();
    testRelease();
    testBoundaries();
    testLoopDrift();
    testSync();
    testSanitize();
    testRunnerLifecycle();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

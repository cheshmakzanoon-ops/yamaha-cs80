// Modern Tape echo time automation only, not evidence of CS-80 circuitry.
// Exercise audible playback through the public model API: a Time increase must not
// reverse the read head across an already-heard burst, discard pending tape audio,
// or prevent the requested delay from being reached.
#include "core/FxModels.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using cs80::fx::StereoSample;
using cs80::fx::TapeEchoFx;

namespace
{
int checks = 0, failures = 0;
constexpr double kTwoPi = 6.283185307179586;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

float burst(double t, double begin, double duration = 0.02)
{
    return t >= begin && t < begin + duration
        ? 0.1f * (float) std::sin(kTwoPi * 500.0 * (t - begin)) : 0.0f;
}

float channel(StereoSample sample, int side) { return side == 0 ? sample.l : sample.r; }

void testNoReverseReplay()
{
    // The source burst has already passed the read head before the time jump.
    // There is no ordinary same-channel repeat in the post-jump measurement window.
    // Multiple jump times cover different wow/flutter phases and delay-buffer wraps.
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
        for (int side = 0; side < 2; ++side)
            for (double jump : { 0.5, 1.3, 3.7 })
            {
                TapeEchoFx tape;
                tape.prepare(fs);
                double firstEnergy = 0.0, replayEnergy = 0.0;
                for (int n = 0; n < (int) ((jump + 0.06) * fs); ++n)
                {
                    const double t = n / fs;
                    const float x = burst(t, jump - 0.1);
                    const auto wet = tape.process(side == 0 ? x : 0.0f, side == 1 ? x : 0.0f,
                                                  t < jump ? 0.0f : 1.0f, 0.0f);
                    const double y = channel(wet, side);
                    if (t >= jump - 0.05 && t < jump) firstEnergy += y * y;
                    if (t >= jump + 0.005) replayEnergy += y * y;
                }
                check(firstEnergy / fs > 1.0e-5, "the original burst is audible before the Time change");
                // -40 dB energy ratio rejects the original ~-6 dB reverse replay,
                // while allowing the short, ordinary playback-filter settling tail.
                check(replayEnergy < firstEnergy * 1.0e-4,
                      "an upward Time jump does not replay the already-heard burst backwards");
                std::printf("replay fs=%.0f channel=%d jump=%.1f ratio=%.9g\n",
                            fs, side, jump, replayEnergy / firstEnergy);
            }
}

void testPendingAudioSurvives()
{
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
        for (int side = 0; side < 2; ++side)
        {
            TapeEchoFx tape;
            tape.prepare(fs);
            double beforeEnergy = 0.0, pendingEnergy = 0.0;
            for (int n = 0; n < (int) (1.2 * fs); ++n)
            {
                const double t = n / fs;
                const float x = burst(t, 0.47, 0.01);
                const auto wet = tape.process(side == 0 ? x : 0.0f, side == 1 ? x : 0.0f,
                                              t < 0.5 ? 0.0f : 1.0f, 0.0f);
                const double y = channel(wet, side);
                if (t < 0.5) beforeEnergy += y * y;
                else pendingEnergy += y * y;
            }
            check(beforeEnergy == 0.0, "the pending burst has not played before the Time change");
            check(pendingEnergy / fs > 1.0e-5, "the forward glide preserves and plays pending tape audio");
        }
}

void testSettledDelay()
{
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
        for (float target : { 0.0f, 1.0f })
        {
            TapeEchoFx tape;
            tape.prepare(fs);
            const double expected = TapeEchoFx::delaySeconds(target);
            constexpr double jump = 0.02, marker = 4.0;
            double peaks[2] {}, arrivals[2] {};
            for (int n = 0; n < (int) ((marker + expected + 0.04) * fs); ++n)
            {
                const double t = n / fs;
                const float x = burst(t, marker, 0.008);
                const auto wet = tape.process(x, x, t < jump ? 1.0f - target : target, 0.0f);
                for (int side = 0; side < 2; ++side)
                    if (const double amplitude = std::abs(channel(wet, side)); amplitude > peaks[side])
                    {
                        peaks[side] = amplitude;
                        arrivals[side] = t - marker;
                    }
            }
            for (int side = 0; side < 2; ++side)
            {
                check(peaks[side] > 0.01, "a new burst remains audible after the glide settles");
                check(std::abs(arrivals[side] - expected) < 0.012,
                      "upward and downward Time changes converge to the requested delay");
            }
        }
}

void testLifecycle()
{
    TapeEchoFx reused;
    reused.prepare(96000.0);
    for (double fs : { 88200.0, 96000.0, 192000.0, 384000.0 })
    {
        // Reprepare after an active upward glide, then reset without a rate change.
        for (int lifecycle = 0; lifecycle < 2; ++lifecycle)
        {
            for (int n = 0; n < 10000; ++n)
                reused.process(0.2f, -0.1f, n < 100 ? 0.0f : 1.0f, 0.7f);
            if (lifecycle == 0) reused.prepare(fs);
            else reused.reset();
            TapeEchoFx fresh;
            fresh.prepare(fs);
            bool identical = true;
            for (int n = 0; n < (int) (0.2 * fs); ++n)
            {
                const float x = n < 100 ? 0.2f : 0.0f;
                const float control = n < 1000 ? 0.0f : 1.0f;
                const auto actual = reused.process(x, -x, control, 0.4f);
                const auto expected = fresh.process(x, -x, control, 0.4f);
                identical = identical && actual.l == expected.l && actual.r == expected.r;
            }
            check(identical, "reset and reprepare clear glide history and agree with a fresh Tape echo");
        }
    }
}
} // namespace

int main()
{
    testNoReverseReplay();
    testPendingAudioSurvives();
    testSettledDelay();
    testLifecycle();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

// Modern Ensemble rate-coefficient caching: exact comparison against the frozen
// pre-cache model. This is software-model evidence, not CS-80 hardware evidence.
#include "EnsembleFxBaseline.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using cs80::fx::EnsembleFx;
using cs80::fx::StereoSample;
using cs80::fx::test::EnsembleFxBaseline;

namespace
{
int checks = 0, failures = 0;
std::uint64_t frames = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
}
bool identical(StereoSample a, StereoSample b)
{
    return std::bit_cast<std::uint32_t>(a.l) == std::bit_cast<std::uint32_t>(b.l)
        && std::bit_cast<std::uint32_t>(a.r) == std::bit_cast<std::uint32_t>(b.r);
}
struct Frame { float l, r, a, b, trim; };
std::vector<Frame> signal(int n, int mode, float rate = 0.37f, float depth = 0.68f)
{
    std::vector<Frame> out((std::size_t) n);
    cs80::Rng rng(0x7135eab9u);
    const float inf = std::numeric_limits<float>::infinity();
    const float edge[] = { -inf, -4.0f, -1.0f, -0.0f, 0.0f,
        std::nextafter(0.0f, 1.0f), std::nextafter(0.5f, 0.0f), 0.5f,
        std::nextafter(0.5f, 1.0f), std::nextafter(1.0f, 0.0f), 1.0f, 2.0f, inf };
    for (int i = 0; i < n; ++i)
    {
        auto& f = out[(std::size_t) i];
        f = { 0.24f * rng.nextBipolar() + 0.12f * std::sin(0.023f * (float) i),
              0.18f * rng.nextBipolar() - 0.08f * std::cos(0.047f * (float) i), rate, depth, 1.0f };
        if (mode == 1) // Steps, including changes on consecutive samples.
        {
            f.a = edge[(i / 4093) % 13];
            f.b = edge[(i / 2053 + 5) % 13];
            if (i % 4093 == 1) f.a = std::nextafter(f.a, 0.5f);
        }
        if (mode == 2) // Every sample changes both controls.
        {
            f.a = 0.5f + 0.6f * rng.nextBipolar();
            f.b = 0.5f + 0.6f * rng.nextBipolar();
        }
        if (mode == 3) // Rate holds while depth changes every sample.
            f.b = 0.5f + 0.6f * rng.nextBipolar();
        if (mode == 4) // Adjacent floats, signed zero and clamp boundaries.
        {
            f.a = edge[(i / 19) % 13];
            f.b = edge[(i / 23 + 3) % 13];
        }
        if (mode == 5 && i > n / 3) // Long free-running clocks and a decay tail.
            f.l = f.r = 0.0f;
        if (mode != 0)
            f.trim = (i / 1009) % 3 == 0 ? 0.71f : ((i / 1009) % 3 == 1 ? 1.0f : 1.42f);
    }
    return out;
}

void compare(EnsembleFx& actual, EnsembleFxBaseline& reference,
             const std::vector<Frame>& input, const char* label)
{
    bool same = true, finite = true, audible = false;
    for (const auto f : input)
    {
        actual.setTrim(f.trim); reference.setTrim(f.trim);
        const auto a = actual.process(f.l, f.r, f.a, f.b);
        const auto b = reference.process(f.l, f.r, f.a, f.b);
        same = identical(a, b) && same;
        finite = std::isfinite(a.l) && std::isfinite(a.r) && finite;
        audible = audible || std::abs(a.l) > 1.0e-7f || std::abs(a.r) > 1.0e-7f;
    }
    frames += input.size();
    check(same, label);
    check(finite, "comparison output is finite");
    check(audible, "comparison exercised non-silent output");
}

void regression()
{
    constexpr double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0, 384000.0 };
    for (double sr : rates)
    {
        for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            for (float b : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
            {
                EnsembleFx actual; EnsembleFxBaseline reference;
                actual.prepare(sr); reference.prepare(sr);
                compare(actual, reference, signal((int) (0.07 * sr), 0, a, b), "held rate/depth grid is bit-identical");
            }
        for (int mode = 1; mode <= 5; ++mode)
        {
            EnsembleFx actual; EnsembleFxBaseline reference;
            actual.prepare(sr); reference.prepare(sr);
            const int n = mode == 5 ? (int) (8.1 * sr) : 70001;
            compare(actual, reference, signal(n, mode), "automated controls, wraps and tails are bit-identical");
        }
        // Keep the same A through a rate change, when the cache must be invalidated.
        EnsembleFx actual; EnsembleFxBaseline reference;
        for (double nextRate : rates)
        {
            actual.prepare(sr); reference.prepare(sr);
            compare(actual, reference, signal((int) (0.11 * sr), 0), "pre-reprepare held-rate history is bit-identical");
            actual.prepare(nextRate); reference.prepare(nextRate);
            compare(actual, reference, signal((int) (0.11 * nextRate), 0), "reprepare at unchanged A refreshes increments");
            actual.reset(); reference.reset();
            compare(actual, reference, signal((int) (0.11 * nextRate), 4), "reset then boundary automation is bit-identical");
        }
    }
    // Neither instance may share another instance's cached controls or running clocks.
    EnsembleFx a, b; EnsembleFxBaseline ar, br;
    a.prepare(96000.0); ar.prepare(96000.0); b.prepare(88200.0); br.prepare(88200.0);
    const auto in = signal(100003, 2);
    bool same = true;
    for (const auto f : in)
    {
        same = identical(a.process(f.l, f.r, f.a, f.b), ar.process(f.l, f.r, f.a, f.b)) && same;
        same = identical(b.process(f.r, f.l, 1.0f - f.b, f.a), br.process(f.r, f.l, 1.0f - f.b, f.a)) && same;
    }
    frames += 2 * in.size();
    check(same, "independent interleaved instances are bit-identical");
}

// Exercise the adaptive fallback's miss/bypass/re-probe boundaries, not just
// long stationary or continuously moving streams. Timing changes never justify
// a different sample: compare the complete output by bit pattern.
void adaptiveTransitions()
{
    constexpr int holds[] = { 1, 2, 31, 63, 64, 65, 66, 127, 1024 };
    constexpr double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0, 384000.0 };
    for (double sr : rates)
        for (int hold : holds)
        {
            EnsembleFx actual; EnsembleFxBaseline reference;
            actual.prepare(sr); reference.prepare(sr);
            auto input = signal(32771, 2);
            for (std::size_t i = 0; i < input.size(); ++i)
            {
                const int period = 137 + hold;
                const int phase = static_cast<int>(i) % period;
                if (phase >= 137)
                    input[i].a = (static_cast<int>(i) / period) % 2 ? 0.37f : 0.73f;
            }
            compare(actual, reference, input, "moving/held transitions across re-probe boundaries are bit-identical");
            // Reprepare/reset while an uncached window is active, with the
            // unchanged held key. Sample rate must not leave stale increments.
            for (int prefix : { 1, 2, 31, 63, 64, 65 })
            {
                actual.prepare(sr); reference.prepare(sr);
                bool prefixSame = true;
                for (const auto f : signal(prefix, 0))
                    prefixSame = identical(actual.process(f.l, f.r, f.a, f.b), reference.process(f.l, f.r, f.a, f.b)) && prefixSame;
                frames += static_cast<std::uint64_t>(prefix);
                check(prefixSame, "adaptive-window prefix is bit-identical before the delay fills");
                const double nextRate = sr == 96000.0 ? 192000.0 : 96000.0;
                actual.prepare(nextRate); reference.prepare(nextRate);
                compare(actual, reference, signal(2053, 0), "reprepare within adaptive window refreshes Rate increments");
                actual.reset(); reference.reset();
                prefixSame = true;
                for (const auto f : signal(prefix, 0))
                    prefixSame = identical(actual.process(f.l, f.r, f.a, f.b), reference.process(f.l, f.r, f.a, f.b)) && prefixSame;
                frames += static_cast<std::uint64_t>(prefix);
                check(prefixSame, "second adaptive-window prefix is bit-identical");
                actual.reset(); reference.reset();
                compare(actual, reference, signal(2053, 4), "reset within adaptive window retains exact boundary controls");
            }
        }
}

struct Timing { double ms; std::uint64_t hash; };
template<class Model> Timing timed(const std::vector<Frame>& input)
{
    Model model;
    model.prepare(96000.0);
    for (int i = 0; i < 32768; ++i)
        (void) model.process(0.1f, -0.07f, 0.37f, 0.68f);
    std::vector<StereoSample> output(input.size());
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        const auto f = input[i];
        model.setTrim(f.trim);
        output[i] = model.process(f.l, f.r, f.a, f.b);
    }
    const auto stop = std::chrono::steady_clock::now();
    // Hash outside the timed loop, consuming every sample so work cannot be removed.
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto y : output)
        for (float x : { y.l, y.r })
            hash = (hash ^ std::bit_cast<std::uint32_t>(x)) * 1099511628211ull;
    return { std::chrono::duration<double, std::milli>(stop - start).count(), hash };
}
void benchmark(int frameCount)
{
    for (int mode : { 0, 1, 2, 3 })
    {
        const auto input = signal(frameCount, mode);
        std::array<double, 7> oldTimes {}, newTimes {};
        for (int trial = 0; trial < 7; ++trial)
        {
            Timing old, now;
            if (trial % 2 == 0) { old = timed<EnsembleFxBaseline>(input); now = timed<EnsembleFx>(input); }
            else { now = timed<EnsembleFx>(input); old = timed<EnsembleFxBaseline>(input); }
            check(old.hash == now.hash, "benchmark output checksum matches");
            oldTimes[(std::size_t) trial] = old.ms; newTimes[(std::size_t) trial] = now.ms;
            std::printf("mode %d trial %d baseline %.3f ms cached %.3f ms checksum %016llx\n",
                        mode, trial, old.ms, now.ms, (unsigned long long) now.hash);
        }
        std::sort(oldTimes.begin(), oldTimes.end()); std::sort(newTimes.begin(), newTimes.end());
        std::printf("mode %d medians baseline %.3f ms cached %.3f ms change %.2f%%\n",
                    mode, oldTimes[3], newTimes[3], 100.0 * (newTimes[3] / oldTimes[3] - 1.0));
    }
}
}
int main(int argc, char** argv)
{
    regression();
    adaptiveTransitions();
    if (argc == 2 && std::string(argv[1]) == "--benchmark") benchmark(524288);
    if (argc == 2 && std::string(argv[1]) == "--benchmark-long") benchmark(4194304);
    std::printf("%d checks, %d failures, %llu compared stereo frames\n", checks, failures, (unsigned long long) frames);
    return failures == 0 ? 0 : 1;
}

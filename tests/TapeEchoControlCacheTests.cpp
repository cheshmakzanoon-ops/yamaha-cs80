// Compatibility/performance checks of a modern software effect, not hardware evidence.
#include "TapeEchoUncachedReference.h"

#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
using cs80::fx::StereoSample;
using cs80::fx::TapeEchoFx;
using cs80::fx::test::UncachedTapeEcho;
int checks = 0, failures = 0;
std::uint64_t comparedFrames = 0;
void check(bool condition, const char* description)
{
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", description); }
}
bool same(StereoSample a, StereoSample b)
{
    return std::bit_cast<std::uint32_t>(a.l) == std::bit_cast<std::uint32_t>(b.l)
        && std::bit_cast<std::uint32_t>(a.r) == std::bit_cast<std::uint32_t>(b.r);
}

struct Input { float l, r, a, b; };
Input inputAt(int n, int scenario)
{
    // Separate stereo broadband signals, with repeated silence and impulses.
    const float l = n % 31 < 3 ? 0.0f : (float) ((n * 17) % 127 - 63) / 128.0f;
    const float r = n % 47 < 3 ? 0.0f : (float) ((n * 29) % 131 - 65) / 256.0f;
    static constexpr float steps[] { 0.0f, 1.0f, 0.25f, 0.75f, 0.5f };
    float a = 0.37f, b = 0.71f;
    if (scenario == 1) a = steps[(n / 12289) % 5];
    if (scenario == 2) b = steps[(n / 8191) % 5];
    if (scenario == 3)
    {
        a = (float) (n % 257) / 256.0f;
        b = (float) (n % 263) / 262.0f;
    }
    if (scenario == 4)
    {
        a = n % 2 ? std::nextafter(0.5f, 1.0f) : 0.5f;
        b = n % 3 ? std::nextafter(0.5f, 0.0f) : 0.5f;
    }
    if (scenario == 5)
    {
        constexpr float inf = std::numeric_limits<float>::infinity();
        static constexpr float bounds[] { -1.0f, -0.0f, 0.0f, 1.0f, 2.0f, -inf, inf };
        a = bounds[(n / 4093) % 7]; b = bounds[(n / 4051) % 7];
    }
    if (scenario == 6)
    {
        // Sparse single-sample excursions must not be swallowed by a cache.
        a = n % 997 == 0 ? 0.95f : 0.05f;
        b = n % 991 == 0 ? 0.01f : 0.9f;
    }
    return { l, r, a, b };
}

struct Comparison
{
    bool identical = true, finite = true;
    double energy = 0.0;
    void add(StereoSample actual, StereoSample expected)
    {
        identical &= same(actual, expected);
        finite &= std::isfinite(actual.l) && std::isfinite(actual.r);
        energy += (double) actual.l * actual.l + (double) actual.r * actual.r;
        ++comparedFrames;
    }
    void verify() const
    {
        check(identical, "every stereo sample bit matches frozen uncached main");
        check(finite, "supported audio and clamped controls produce finite output");
        check(energy > 1.0e-6, "comparison includes audible delayed/feedback audio, not only startup silence");
    }
};

void testExactOutput()
{
    for (double fs : { 88200.0, 96000.0, 176400.0, 192000.0, 384000.0, 768000.0 })
        for (int scenario = 0; scenario < 7; ++scenario)
        {
            TapeEchoFx cached; UncachedTapeEcho original;
            cached.prepare(fs); original.prepare(fs);
            Comparison comparison;
            // Beyond the maximum 1.2 s delay and at least one full buffer wrap.
            for (int n = 0; n < (int) (1.4 * fs); ++n)
            {
                const auto in = inputAt(n, scenario);
                comparison.add(cached.process(in.l, in.r, in.a, in.b),
                               original.process(in.l, in.r, in.a, in.b));
            }
            comparison.verify();
        }
    // Full held-control grid at the reference rate, including the maximum delay.
    for (float a : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        for (float b : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            TapeEchoFx cached; UncachedTapeEcho original;
            cached.prepare(96000.0); original.prepare(96000.0);
            Comparison comparison;
            for (int n = 0; n < 134400; ++n)
            {
                const auto in = inputAt(n, 0);
                comparison.add(cached.process(in.l, in.r, a, b), original.process(in.l, in.r, a, b));
            }
            comparison.verify();
        }
}

void testLifecycle()
{
    TapeEchoFx cached; UncachedTapeEcho original;
    cached.prepare(96000.0); original.prepare(96000.0);
    for (double fs : { 768000.0, 88200.0, 192000.0, 96000.0 })
        for (int resetKind = 0; resetKind < 2; ++resetKind)
        {
            if (resetKind == 0) { cached.prepare(fs); original.prepare(fs); }
            else { cached.reset(); original.reset(); }
            Comparison comparison;
            for (int n = 0; n < (int) (0.3 * fs); ++n)
            {
                const auto in = inputAt(n, 0);
                // Held controls survive rate changes and reset. Trim updates must
                // not invalidate or perturb the tape state/control result.
                const float trim = n < 10000 ? 0.5f : 1.25f;
                cached.setTrim(trim); original.setTrim(trim);
                comparison.add(cached.process(in.l, in.r, 0.0f, 0.8f),
                               original.process(in.l, in.r, 0.0f, 0.8f));
            }
            comparison.verify();
        }

    TapeEchoFx neighbor, isolated;
    neighbor.prepare(96000.0); isolated.prepare(96000.0);
    Comparison independence;
    for (int n = 0; n < 134400; ++n)
    {
        const auto fixed = inputAt(n, 0), varying = inputAt(n, 3);
        const auto actual = neighbor.process(fixed.l, fixed.r, fixed.a, fixed.b);
        cached.process(varying.l, varying.r, varying.a, varying.b);
        independence.add(actual, isolated.process(fixed.l, fixed.r, fixed.a, fixed.b));
    }
    independence.verify();

    // Long traversal through both glide directions, silence/feedback tail, and
    // repeated circular-buffer wraps. No changes to the evolving repeat gain.
    cached.prepare(96000.0); original.prepare(96000.0);
    Comparison glide;
    for (int n = 0; n < 960000; ++n)
    {
        auto in = inputAt(n, 0);
        const float a = n < 19200 || n >= 480000 ? 0.0f : 1.0f;
        const float b = n < 700000 ? 0.9f : 0.1f;
        if (n >= 600000) in.l = in.r = 0.0f;
        glide.add(cached.process(in.l, in.r, a, b), original.process(in.l, in.r, a, b));
    }
    glide.verify();
}

#if defined(_MSC_VER)
#define CS80_NOINLINE __declspec(noinline)
#else
#define CS80_NOINLINE __attribute__((noinline))
#endif
// Runtime control boundary: do not let the compiler hoist the original powers
// out of the loop because the benchmark happens to know the held controls.
template <class Model>
CS80_NOINLINE StereoSample tick(Model& model, const Input& input)
{ return model.process(input.l, input.r, input.a, input.b); }

template <class Model>
double measure(const std::vector<Input>& input, double fs, double& checksum)
{
    Model model;
    model.prepare(fs);
    for (int n = 0; n < 8192; ++n) tick(model, input[(size_t) n]);
    const auto begin = std::chrono::steady_clock::now();
    double sum = 0.0;
    for (const auto& in : input)
    {
        const auto sample = tick(model, in);
        sum += (double) sample.l + (double) sample.r;
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    checksum = sum;
    return elapsed;
}

void benchmark()
{
    for (double fs : { 96000.0, 384000.0 })
        for (int scenario : { 0, 1, 2, 3 })
        {
            std::vector<Input> input;
            for (int n = 0; n < 524288; ++n) input.push_back(inputAt(n, scenario));
            std::array<double, 5> oldTime {}, newTime {};
            double oldSum = 0.0, newSum = 0.0;
            for (int round = 0; round < 5; ++round)
            {
                if (round % 2 == 0)
                {
                    oldTime[(size_t) round] = measure<UncachedTapeEcho>(input, fs, oldSum);
                    newTime[(size_t) round] = measure<TapeEchoFx>(input, fs, newSum);
                }
                else
                {
                    newTime[(size_t) round] = measure<TapeEchoFx>(input, fs, newSum);
                    oldTime[(size_t) round] = measure<UncachedTapeEcho>(input, fs, oldSum);
                }
                check(oldSum == newSum, "each benchmark trial preserves its output checksum");
                std::printf("trial fs=%.0f scenario=%d round=%d old_ms=%.3f cached_ms=%.3f\n",
                            fs, scenario, round, 1000.0 * oldTime[(size_t) round], 1000.0 * newTime[(size_t) round]);
            }
            std::sort(oldTime.begin(), oldTime.end()); std::sort(newTime.begin(), newTime.end());
            std::printf("tape fs=%.0f scenario=%d samples=%zu median_ms old=%.3f cached=%.3f saved=%.1f%% checksum=%.12g\n",
                        fs, scenario, input.size(), 1000.0 * oldTime[2], 1000.0 * newTime[2],
                        100.0 * (1.0 - newTime[2] / oldTime[2]), oldSum);
        }
}
} // namespace

int main(int argc, char** argv)
{
    testExactOutput();
    testLifecycle();
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) benchmark();
    std::printf("%d checks, %d failures; %llu stereo frames compared by bit pattern\n",
                checks, failures, (unsigned long long) comparedFrames);
    return failures == 0 ? 0 : 1;
}

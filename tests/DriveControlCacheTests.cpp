// Exact-output guard for caching the modern Drive's control-only laws.
// The reference freezes DriveFx at main b5e0f1f, before control caching.
// This is a software performance/compatibility check, not CS-80 evidence.
#include "core/FxModels.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
using cs80::fx::DriveFx;
using cs80::fx::StereoSample;

// Deliberately retain the original per-sample control evaluation here. Do not
// share the production cache: this must detect stale/reset/rate regressions.
class UncachedDrive
{
public:
    void prepare(double fs)
    {
        sr = fs;
        dcCoef = (float) std::exp(-6.283185307179586 * 10.0 / fs);
        reset();
    }
    void reset()
    {
        for (auto& o : oversampler) o.reset();
        dcIn = dcOut = { 0.0f, 0.0f };
        toneState = { 0.0f, 0.0f };
    }
    StereoSample process(float l, float r, float a, float b, int stages) noexcept
    {
        const float g = std::pow(10.0f, 1.5f * cs80::clampf(a, 0.0f, 1.0f));
        const float toneHz = 1500.0f * std::pow(12.0f, cs80::clampf(b, 0.0f, 1.0f));
        const float toneCoef = 1.0f - (float) std::exp(-6.283185307179586 * std::min((double) toneHz, 0.45 * sr) / sr);
        const float in[2] = { l, r };
        float out[2] {};
        for (int c = 0; c < 2; ++c)
        {
            const float y = oversampler[(size_t) c].process(in[c], stages, [g](float v) {
                const float t = std::tanh(0.2f);
                return (std::tanh(g * v + 0.2f) - t) / (g * (1.0f - t * t));
            });
            const float blocked = y - dcIn[(size_t) c] + dcCoef * dcOut[(size_t) c];
            dcIn[(size_t) c] = y;
            dcOut[(size_t) c] = blocked;
            toneState[(size_t) c] += toneCoef * (blocked - toneState[(size_t) c]);
            out[c] = toneState[(size_t) c];
        }
        return { out[0], out[1] };
    }

private:
    std::array<cs80::fx::Oversampler, 2> oversampler;
    std::array<float, 2> dcIn {}, dcOut {}, toneState {};
    double sr = 96000.0;
    float dcCoef = 0.0f;
};

int checks = 0, failures = 0;
void check(bool ok, const char* description)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", description);
    }
}

bool same(float a, float b)
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
bool same(StereoSample a, StereoSample b)
{
    return same(a.l, b.l) && same(a.r, b.r);
}

struct Input { float l, r, a, b; int stages; };
Input inputAt(int n, int scenario, int stages)
{
    // Deterministic broadband input, including silence, without timing libm.
    const float l = n % 31 < 3 ? 0.0f : (float) ((n * 17) % 127 - 63) / 128.0f;
    const float r = n % 47 < 3 ? 0.0f : (float) ((n * 29) % 131 - 65) / 256.0f;
    float a = 0.63f, b = 0.41f;
    static constexpr float steps[] { 0.0f, 0.5f, 1.0f, 0.25f, 0.75f, 0.5f };
    if (scenario == 1) a = steps[(n / 137) % 6];
    if (scenario == 2) b = steps[(n / 131) % 6];
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
        a = steps[(n / 17) % 6];
        b = steps[(n / 19) % 6];
        stages = (n / 113) % 4;
    }
    if (scenario == 6) a = b = steps[(n / 113) % 6];
    if (scenario == 7)
    {
        // Preserve the public model's existing clamp and signed-zero behavior.
        static constexpr float bounds[] { -2.0f, -0.0f, 0.0f, 1.0f, 2.0f };
        a = bounds[(n / 17) % 5];
        b = bounds[(n / 19) % 5];
    }
    return { l, r, a, b, stages };
}

void testExactOutput()
{
    for (double fs : { 16000.0, 88200.0, 96000.0, 192000.0, 384000.0, 1536000.0 })
        for (int stages = 0; stages <= 3; ++stages)
            for (int scenario = 0; scenario < 8; ++scenario)
            {
                DriveFx cached;
                UncachedDrive reference;
                cached.prepare(fs);
                reference.prepare(fs);
                bool identical = true, finite = true;
                for (int n = 0; n < 4096; ++n)
                {
                    const auto in = inputAt(n, scenario, stages);
                    const auto actual = cached.process(in.l, in.r, in.a, in.b, in.stages);
                    const auto expected = reference.process(in.l, in.r, in.a, in.b, in.stages);
                    identical &= same(actual, expected);
                    finite &= std::isfinite(actual.l) && std::isfinite(actual.r);
                }
                check(identical, "held, stepped and sample-automated controls preserve every output bit");
                check(finite, "supported drive output remains finite");
            }
}

void testLifecycle()
{
    // The public model's default-state behavior stays unchanged too, even
    // though the engine always prepares its effects before rendering them.
    {
        DriveFx cached;
        UncachedDrive reference;
        bool identical = true;
        for (int n = 0; n < 4096; ++n)
        {
            const auto in = inputAt(n, 6, 2);
            identical &= same(cached.process(in.l, in.r, in.a, in.b, in.stages),
                              reference.process(in.l, in.r, in.a, in.b, in.stages));
        }
        check(identical, "default-state processing retains the uncached model's output");
    }
    DriveFx cached;
    UncachedDrive reference;
    cached.prepare(96000.0);
    reference.prepare(96000.0);
    // Hold A/B across every rate change: this catches a cache that omits sr.
    for (double fs : { 88200.0, 1536000.0, 16000.0, 384000.0, 96000.0 })
        for (int lifecycle = 0; lifecycle < 2; ++lifecycle)
        {
            if (lifecycle == 0) { cached.prepare(fs); reference.prepare(fs); }
            else { cached.reset(); reference.reset(); }
            bool identical = true;
            for (int n = 0; n < 4096; ++n)
            {
                const auto in = inputAt(n, 0, 2);
                identical &= same(cached.process(in.l, in.r, in.a, in.b, in.stages),
                                  reference.process(in.l, in.r, in.a, in.b, in.stages));
            }
            check(identical, "reset and reprepare preserve exact output with unchanged controls");
        }

    // Caches are instance-owned; one slot's automation cannot change another.
    DriveFx neighbor, isolated;
    neighbor.prepare(96000.0); isolated.prepare(96000.0);
    bool independent = true;
    for (int n = 0; n < 4096; ++n)
    {
        const auto fixed = inputAt(n, 0, 1), moving = inputAt(n, 3, 3);
        const auto actual = neighbor.process(fixed.l, fixed.r, fixed.a, fixed.b, fixed.stages);
        cached.process(moving.l, moving.r, moving.a, moving.b, moving.stages);
        independent &= same(actual, isolated.process(fixed.l, fixed.r, fixed.a, fixed.b, fixed.stages));
    }
    check(independent, "separate drive slots keep independent control caches");
}

#if defined(_MSC_VER)
#define CS80_NOINLINE __declspec(noinline)
#else
#define CS80_NOINLINE __attribute__((noinline))
#endif

// Match an audio callback receiving runtime controls. Keeping one call boundary
// prevents constant-control compiler hoisting from hiding the original cost.
template <class Model>
CS80_NOINLINE StereoSample tick(Model& model, const Input& in)
{
    return model.process(in.l, in.r, in.a, in.b, in.stages);
}

template <class Model>
double measure(const std::vector<Input>& input, double& checksum)
{
    Model model;
    model.prepare(96000.0);
    for (int n = 0; n < 8192; ++n) tick(model, input[(size_t) n]);
    const auto begin = std::chrono::steady_clock::now();
    double sum = 0.0;
    for (const auto& in : input)
    {
        const auto sample = tick(model, in);
        sum += sample.l + sample.r;
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    checksum = sum;
    return elapsed;
}

void benchmark()
{
    for (int stages : { 0, 2, 3 })
        for (int scenario : { 0, 3 })
        {
            std::vector<Input> input;
            for (int n = 0; n < 262144; ++n) input.push_back(inputAt(n, scenario, stages));
            std::array<double, 7> oldTime {}, newTime {};
            double oldSum = 0.0, newSum = 0.0;
            for (int round = 0; round < 7; ++round)
            {
                if (round % 2 == 0)
                {
                    oldTime[(size_t) round] = measure<UncachedDrive>(input, oldSum);
                    newTime[(size_t) round] = measure<DriveFx>(input, newSum);
                }
                else
                {
                    newTime[(size_t) round] = measure<DriveFx>(input, newSum);
                    oldTime[(size_t) round] = measure<UncachedDrive>(input, oldSum);
                }
                check(oldSum == newSum, "each benchmark trial retains its output checksum");
            }
            std::sort(oldTime.begin(), oldTime.end());
            std::sort(newTime.begin(), newTime.end());
            std::printf("drive stages=%d controls=%s samples=%zu median_ms old=%.3f cached=%.3f saved=%.1f%% checksum=%.12g\n",
                        stages, scenario == 0 ? "held" : "per-sample", input.size(),
                        1000.0 * oldTime[3], 1000.0 * newTime[3],
                        100.0 * (1.0 - newTime[3] / oldTime[3]), oldSum);
        }
}
} // namespace

int main(int argc, char** argv)
{
    testExactOutput();
    testLifecycle();
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) benchmark();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

// Exact reconstruction parity against main b5e0f1f, before sparse evaluation.
// This is an offline, JUCE-free DSP test; no processor, GUI or host is loaded.
#include "core/ExternalModulationInterpolator.h"

#include <atomic>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <string_view>

static std::atomic<long> allocations { 0 };
void* operator new(std::size_t size)
{
    ++allocations;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace
{
using namespace cs80;
// Frozen zero-insertion/every-internal-sample path. OversamplingDecimator and
// its coefficient design are unchanged by this optimization.
class FrozenInterpolator
{
public:
    static constexpr double kHostDelay = (double) Decimator::kCenter / 2.0; // 2x
    using Frame = std::array<float, OversamplingDecimator::kMaxFactor>;

    void prepare(float hostRate, int oversampling = 2)
    {
        (void) hostRate; // normalized design, like the output decimator
        filter.prepare(oversampling);
        reset();
    }

    void reset() noexcept
    {
        filter.reset();
        connected = false;
    }

    void setConnected(bool enabled) noexcept
    {
        if (enabled != connected)
        {
            filter.reset();
            connected = enabled;
        }
    }

    static float sanitise(float input) noexcept
    {
        // Explicit software protection, NOT measured input-stage clipping.
        return std::isfinite(input) ? clampf(input, -1.0f, 1.0f) : 0.0f;
    }

    int getFactor() const noexcept { return filter.getFactor(); }
    double getHostDelay() const noexcept
    { return (double) filter.getCenter() / (double) filter.getFactor(); }

    // The first getFactor() entries are this host frame's internal samples.
    Frame tick(float input) noexcept
    {
        Frame out {};
        if (!connected) return out;
        const int factor = filter.getFactor();
        const float x = static_cast<float>(factor) * sanitise(input);
        // Finite FIR ringing can exceed1; do not silently add a second clipper.
        out[0] = filter.process(x);
        for (int i = 1; i < factor; ++i)
            out[(size_t) i] = filter.process(0.0f);
        return out;
    }

private:
    OversamplingDecimator filter;
    bool connected = false;
};

int checks = 0, failures = 0;
std::uint64_t comparedFrames = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool same(const ExternalModulationInterpolator::Frame& a,
          const ExternalModulationInterpolator::Frame& b)
{
    ++comparedFrames;
    for (size_t i = 0; i < a.size(); ++i)
        if (std::bit_cast<std::uint32_t>(a[i]) != std::bit_cast<std::uint32_t>(b[i]))
            return false;
    return true;
}
float signal(int kind, int n, Rng& rng)
{
    const float tiny = std::numeric_limits<float>::denorm_min();
    switch (kind)
    {
        case 0: return 0.0f;
        case 1: return -0.0f;
        case 2: return (n & 1) ? -0.0f : 0.0f;
        case 3: return 1.0f;
        case 4: return -0.37f;
        case 5: return (n & 1) ? tiny : -tiny;
        case 6: return (n % 239 == 0) ? 1.0f : 0.0f;
        case 7: return ((n / 197) & 1) ? 1.0f : -1.0f;
        case 8: return (float) (n % 509) / 254.0f - 1.0f;
        case 9: return (float) std::sin(0.00003 * n * n);
        case 10: return rng.nextBipolar();
        case 11: return std::bit_cast<float>(rng.nextUInt());
        default:
        {
            const float limits[] = { tiny, -tiny, std::numeric_limits<float>::min(),
                -std::numeric_limits<float>::min(), std::numeric_limits<float>::max(),
                -std::numeric_limits<float>::max(), std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN(),
                std::nextafter(1.0f, 0.0f), std::nextafter(1.0f, 2.0f),
                std::nextafter(-1.0f, 0.0f), std::nextafter(-1.0f, -2.0f), 0.0f, -0.0f };
            return limits[n % (sizeof(limits) / sizeof(*limits))];
        }
    }
}

void exactSignals()
{
    for (float rate : { 44100.0f, 48000.0f, 96000.0f, 192000.0f })
        for (int factor : { 2, 4, 8 })
            for (int kind = 0; kind < 13; ++kind)
            {
                ExternalModulationInterpolator actual;
                FrozenInterpolator expected;
                actual.prepare(rate, factor); expected.prepare(rate, factor);
                actual.setConnected(true); expected.setConnected(true);
                Rng rng(0x1234abcd);
                bool exact = true;
                for (int n = 0; n < 8192; ++n)
                {
                    const float x = signal(kind, n, rng);
                    exact = same(actual.tick(x), expected.tick(x)) && exact;
                }
                check(exact, "every phase, including unused entries, matches the frozen FIR bits");
                check(actual.getFactor() == expected.getFactor()
                    && actual.getHostDelay() == expected.getHostDelay(), "factor and fractional host delay unchanged");
            }
}

void lifecycle()
{
    ExternalModulationInterpolator actual;
    FrozenInterpolator expected;
    Rng rng(0x7f84d912);
    bool exact = true;
    // Default-state, disconnected, then connected before the first prepare.
    for (int n = 0; n < 512; ++n)
    {
        if (n == 128) { actual.setConnected(true); expected.setConnected(true); }
        const float x = rng.nextBipolar();
        exact = same(actual.tick(x), expected.tick(x)) && exact;
    }
    check(exact, "default-state behavior matches before prepare");
    const long before = allocations.load();
    // Rate/factor changes on an existing object, including invalid factors.
    for (int factor : { 8, 2, 4, 1, -8, 0, 3, 16, 8, 4, 2 })
    {
        actual.prepare(48000.0f, factor); expected.prepare(48000.0f, factor);
        check(actual.getFactor() == expected.getFactor()
            && actual.getHostDelay() == expected.getHostDelay(), "invalid factors retain the 2x fallback");
        exact = true;
        for (int n = 0; n < 4096; ++n)
        {
            if (n == 127 || n == 128 || n == 513 || n == 1025 || n == 2048)
            { actual.setConnected(true); expected.setConnected(true); }
            if (n == 256 || n == 512 || n == 2047)
            { actual.setConnected(false); expected.setConnected(false); }
            if (n == 1024) { actual.reset(); expected.reset(); }
            const float x = signal(12, n, rng);
            exact = same(actual.tick(x), expected.tick(x)) && exact;
        }
        check(exact, "reset, repeated connection, disconnect, reconnect and reprepare match");
    }
    check(allocations.load() == before, "preparation and rendering allocate no heap storage");

    // Interleaved instances must retain independent histories.
    ExternalModulationInterpolator a, b;
    FrozenInterpolator ar, br;
    a.prepare(44100, 4); ar.prepare(44100, 4);
    b.prepare(96000, 8); br.prepare(96000, 8);
    a.setConnected(true); ar.setConnected(true);
    b.setConnected(true); br.setConnected(true);
    exact = true;
    for (int n = 0; n < 4096; ++n)
    {
        const float x = rng.nextBipolar();
        exact = same(a.tick(x), ar.tick(x)) && exact;
        exact = same(b.tick(-x), br.tick(-x)) && exact;
    }
    check(exact, "independent instances preserve separate histories");
}

struct Timing { double milliseconds, sum; };
template <class Interpolator>
Timing measure(int factor)
{
    Interpolator instance;
    instance.prepare(48000, factor);
    instance.setConnected(true);
    constexpr int frames = 262144;
    std::array<float, 1024> input {};
    Rng rng(0xabcdef01);
    for (auto& x : input) x = rng.nextBipolar();
    double sum = 0.0;
    const auto start = std::chrono::steady_clock::now();
    for (int n = 0; n < frames; ++n)
    {
        const auto out = instance.tick(input[(size_t) n % input.size()]);
        // Consume EVERY phase: observing only phase zero would allow the
        // compiler to discard the other reference convolutions.
        for (int p = 0; p < factor; ++p) sum += out[(size_t) p];
    }
    return { std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count(), sum };
}
void benchmark()
{
    for (int factor : { 2, 4, 8 })
        for (int trial = 0; trial < 5; ++trial)
        {
            Timing before, after;
            if (trial & 1) { after = measure<ExternalModulationInterpolator>(factor); before = measure<FrozenInterpolator>(factor); }
            else { before = measure<FrozenInterpolator>(factor); after = measure<ExternalModulationInterpolator>(factor); }
            check(std::bit_cast<std::uint64_t>(before.sum) == std::bit_cast<std::uint64_t>(after.sum),
                  "benchmark consumes identical complete output");
            std::printf("%dx trial %d: full %.3f ms, sparse %.3f ms, speedup %.2fx, checksum %.12g\n",
                factor, trial, before.milliseconds, after.milliseconds, before.milliseconds / after.milliseconds, after.sum);
        }
}
} // namespace
int main(int argc, char** argv)
{
    exactSignals();
    lifecycle();
    if (argc == 2 && std::string_view(argv[1]) == "--benchmark") benchmark();
    std::printf("%d checks, %d failures; %llu complete frames compared bit-for-bit\n",
        checks, failures, (unsigned long long) comparedFrames);
    return failures ? 1 : 0;
}

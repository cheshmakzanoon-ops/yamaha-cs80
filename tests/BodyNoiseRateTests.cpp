// Observable PCM contracts for breath, contact and flutter across body rates.
// No private-state access or implementation variance formula is used here.
// Requesting a fundamental above host Nyquist removes the harmonic bank via
// its public antialias path while retaining the real note-on and noise stream.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace cs80::body;
int checks = 0, failures = 0;

void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
double db(double ratio) { return 10.0 * std::log10(std::max(ratio, 1.e-30)); }

struct Energy
{
    double sum = 0.0, peak = 0.0;
    std::uint64_t count = 0;
    bool finite = true;
    void add(double sample)
    {
        sum += sample * sample;
        peak = std::max(peak, std::abs(sample));
        finite = finite && std::isfinite(sample);
        ++count;
    }
    double mean() const { return sum / std::max<std::uint64_t>(count, 1); }
};

Energy noise(double rate, double host, int model, int seeds, double begin, double end,
             float velocity = .8f)
{
    Energy result;
    for (int seed = 0; seed < seeds; ++seed)
    {
        InstrumentBody body;
        body.prepare(rate, host);
        body.noteOn(model, 60.f, velocity, .5f, 1.f, 42u + 7919u * seed);
        const auto first = static_cast<int>(std::lround(begin * rate));
        const auto last = static_cast<int>(std::lround(end * rate));
        for (int i = 0; i < last; ++i)
        {
            // At the 8 kHz/8 kHz limit the internal pitch clamp is 0.49Fs,
            // still above the complete harmonic-rejection edge (0.465Fs).
            const double sample = body.tick(static_cast<float>(host), .5f);
            if (i >= first) result.add(sample);
        }
    }
    check(result.finite && result.peak < .08, "isolated noise is finite and bounded");
    return result;
}

void steadyAir()
{
    const auto reference = noise(96000, 48000, flute, 3, .15, 1.15);
    check(reference.mean() > 5.e-7, "reference breath is measurable");
    for (double host : {44100., 48000., 96000.})
        for (int factor : {2, 4, 8})
        {
            const auto capture = noise(host * factor, host, flute, 3, .15, 1.15);
            const double change = db(capture.mean() / reference.mean());
            check(std::abs(change) < .16, "breath level survives host/oversampling change");
            std::printf("AIR host=%.0f factor=%d rms=%.10g relative=%+.5f dB\n",
                        host, factor, std::sqrt(capture.mean()), change);
        }
    // Wide legal prepare rates also reject a sqrt(Fs) approximation, which
    // overcorrects this discrete filter at low rates.
    for (double rate : {8000., 11025., 16000., 22050., 48000., 1536000.})
    {
        const auto capture = noise(rate, std::min(rate, 48000.), flute, 3, .15, 1.15);
        const double change = db(capture.mean() / reference.mean());
        check(std::abs(change) < .18, "breath energy remains stable across legal rate limits");
        std::printf("WIDE rate=%.0f rms=%.10g relative=%+.5f dB\n",
                    rate, std::sqrt(capture.mean()), change);
    }
    for (int model : {bowed, clarinet, oboe, bassoon, brass})
    {
        const auto base = noise(96000, 48000, model, 2, .15, .65);
        const auto high = noise(384000, 48000, model, 2, .15, .65);
        check(base.mean() > 1.e-8 && std::abs(db(high.mean() / base.mean())) < .22,
              std::string(name(model)) + ": sustained texture is rate consistent");
    }
    const auto silent = noise(96000, 48000, organ, 1, 0., .02);
    check(silent.sum == 0.0, "harmonic rejection control is exactly silent without noise");
}

void contacts()
{
    // Short contacts need an ensemble: comparing a single random onset can
    // mistake seed luck for an energy change. Integrate PCM energy in seconds
    // over 96 strikes, including the genuine filter startup and onset fade.
    for (int model : {piano, tine, clav, steel, marimba})
        for (float velocity : {.2f, .9f})
        {
            const auto reference = noise(96000, 48000, model, 96, 0., .025, velocity);
            check(reference.mean() > 1.e-9, "contact ensemble contains measurable excitation");
            for (double rate : {88200., 192000., 384000., 768000.})
            {
                const auto capture = noise(rate, 44100., model, 96, 0., .025, velocity);
                const double change = db(capture.mean() / reference.mean());
                check(std::abs(change) < .45, std::string(name(model))
                      + ": contact onset energy survives oversampling");
                std::printf("CONTACT model=%d velocity=%.1f rate=%.0f relative=%+.5f dB\n",
                            model, velocity, rate, change);
            }
        }
}

double flutterEnergy(double host, int factor)
{
    const double rate = host * factor;
    const int quarterPeriod = factor;
    const int first = static_cast<int>(rate * .25);
    const int last = static_cast<int>(rate * 2.25);
    double sumVariance = 0.0;
    for (std::uint32_t seed : {42u, 7919u, 0x983134u})
    {
        InstrumentBody sounded, airOnly;
        sounded.prepare(rate, host); airOnly.prepare(rate, host);
        sounded.noteOn(flute, 60, .8f, .5f, 1, seed);
        airOnly.noteOn(flute, 60, .8f, .5f, 1, seed);
        std::vector<double> signal(static_cast<std::size_t>(last + quarterPeriod));
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            // A carrier at host/4 retains only the fundamental. Its dark and
            // bright weights coincide, so timbre motion cannot change its
            // amplitude. Subtract the identical-seed air-only render to remove
            // additive breath before measuring amplitude modulation.
            signal[i] = static_cast<double>(sounded.tick(static_cast<float>(host / 4), .5f))
                      - airOnly.tick(static_cast<float>(host), .5f);
        }
        double sum = 0.0, squares = 0.0;
        int count = 0;
        for (int i = first; i < last; i += quarterPeriod)
        {
            // These two carrier phases differ by exactly a quarter cycle;
            // their hypotenuse removes the carrier without knowing its phase.
            const double envelope = std::hypot(signal[static_cast<std::size_t>(i)],
                                    signal[static_cast<std::size_t>(i + quarterPeriod)]);
            sum += envelope; squares += envelope * envelope; ++count;
        }
        const double mean = sum / count;
        sumVariance += std::max(0.0, squares / count - mean * mean) / (mean * mean);
    }
    return sumVariance / 3.0;
}

void flutter()
{
    const double reference = flutterEnergy(48000, 2);
    check(reference > 1.e-8 && reference < 1.e-7, "flutter is measurable in sounded PCM");
    for (const auto rates : {std::array<int, 2>{44100, 2}, {48000, 4}, {48000, 8}, {96000, 8}})
    {
        const double measured = flutterEnergy(rates[0], rates[1]);
        const double change = db(measured / reference);
        // A 19 Hz process has far fewer independent observations than the
        // air band. Six pooled seconds support this separate, wider limit.
        check(std::abs(change) < .8, "flutter depth survives rate changes independently of air");
        std::printf("FLUTTER host=%d factor=%d relative=%+.5f dB\n", rates[0], rates[1], change);
    }
}

void defaultStream()
{
    // Golden output fingerprints captured from the unmodified header. These
    // exercise public PCM, including note-on draws, onset and contact decay.
    struct Case { int model; std::uint64_t hash; };
    constexpr Case cases[] = {
        {flute, 0x741fa1bfadfb90b0ULL},
        {piano, 0xc86def2df355d73fULL},
        {steel, 0x43898cb5b1e5ababULL}
    };
    for (const auto item : cases)
    {
        InstrumentBody body;
        body.prepare(96000, 48000);
        body.noteOn(item.model, 60, .8f, .5f, 1, 42);
        std::uint64_t hash = 1469598103934665603ULL;
        for (int i = 0; i < 4096; ++i)
        {
            const float sample = body.tick(48000, .5f);
            std::uint32_t bits = 0;
            std::memcpy(&bits, &sample, sizeof(bits));
            hash ^= bits; hash *= 1099511628211ULL;
        }
        std::printf("STREAM model=%d hash=%016llx\n", item.model, static_cast<unsigned long long>(hash));
        check(hash == item.hash, "96 kHz default noise stream remains byte-identical");
    }
}
}

int main()
{
    steadyAir(); contacts(); flutter(); defaultStream();
    std::printf("Body noise rates: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

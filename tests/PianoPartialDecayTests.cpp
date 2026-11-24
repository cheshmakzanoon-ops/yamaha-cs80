// Rendered regression for the piano's former h6/h7 prompt-loss boundary.
// Pool several windows and hammer seeds: one short tail window can mistake
// course beating for a loss discontinuity. These are relative PCM contracts,
// not a claim of acoustic equivalence or measured piano decay constants.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
std::uint64_t generatedSamples = 0;

void check(bool condition, const std::string& description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description.c_str()); }
}

double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }

struct Capture
{
    double rate;
    std::vector<float> pcm;
};

Capture render(int note, double hostRate, float velocity, float colour,
               float decay, std::uint32_t seed, double duration)
{
    Capture result {2.0 * hostRate, {}};
    result.pcm.resize(static_cast<std::size_t>(std::llround(duration * result.rate)));
    cs80::body::InstrumentBody piano;
    piano.prepare(result.rate, hostRate);
    piano.noteOn(cs80::body::piano, static_cast<float>(note), velocity, colour, decay, seed);
    const float frequency = static_cast<float>(hz(note));
    bool finite = true;
    double peak = 0.0;
    for (auto& sample : result.pcm)
    {
        sample = piano.tick(frequency, colour);
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    generatedSamples += result.pcm.size();
    check(finite && peak > 1.0e-4 && peak < 0.80, "piano capture is finite, sounding and bounded");
    return result;
}

// Arbitrary-frequency Hann projections resolve stretched partials without
// quantizing them to FFT bins. The actual float playback pitch is the reference.
struct Spectrum
{
    double rate, weightSum = 0.0;
    std::vector<double> windowed;

    Spectrum(const Capture& audio, double start, double length) : rate(audio.rate)
    {
        const auto first = static_cast<std::size_t>(std::llround(start * rate));
        const auto count = static_cast<std::size_t>(std::llround(length * rate));
        windowed.resize(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * i / (count - 1));
            weightSum += weight;
            windowed[i] = weight * audio.pcm[first + i];
        }
    }

    double tone(double frequency) const
    {
        const double angle = 2.0 * pi * frequency / rate;
        const double cosine = std::cos(angle), sine = std::sin(angle);
        double real = 1.0, imaginary = 0.0, sumReal = 0.0, sumImaginary = 0.0;
        for (double sample : windowed)
        {
            sumReal += real * sample;
            sumImaginary += imaginary * sample;
            const double nextReal = real * cosine - imaginary * sine;
            imaginary = imaginary * cosine + real * sine;
            real = nextReal;
        }
        return 2.0 * std::hypot(sumReal, sumImaginary) / weightSum;
    }
};

struct Bands
{
    // Squared projection magnitudes are pooled BEFORE forming a ratio. A
    // single h6 beat null consequently cannot produce an unbounded ratio.
    std::array<double, 2> energy {}, offEnergy {};
    int windows = 0;

    void add(const Spectrum& spectrum, double frequency)
    {
        constexpr double stiffness = 0.000045; // Both tested bass registers.
        for (std::size_t band = 0; band < energy.size(); ++band)
        {
            const double h = 6.0 + band;
            const double center = frequency * h
                * std::sqrt((1.0 + stiffness * h * h) / (1.0 + stiffness));
            const double signal = spectrum.tone(center);
            const double left = spectrum.tone(center - 0.4 * frequency);
            const double right = spectrum.tone(center + 0.4 * frequency);
            energy[band] += signal * signal;
            offEnergy[band] += 0.5 * (left * left + right * right);
        }
        ++windows;
    }

    double amplitude(std::size_t band) const { return std::sqrt(energy[band] / windows); }
    double ratioDb() const { return db(amplitude(1) / amplitude(0)); }
    double prominenceDb(std::size_t band) const
    {
        return db(std::sqrt(energy[band] / std::max(offEnergy[band], 1.0e-30)));
    }
};

struct Result { double change, minimumProminence; };

Result measure(int note, double hostRate, float velocity, float colour,
               float decay, std::uint32_t seed)
{
    // Decay changes the relative-partial clock. Retain at least 250 ms for
    // frequency resolution at C1 when the clock is shortened to 0.25.
    const double length = std::max(0.25, 0.25 * decay);
    const auto audio = render(note, hostRate, velocity, colour, decay, seed,
                              3.0 * decay + length + 0.001);
    const double frequency = static_cast<float>(hz(note));
    Bands early, late;
    for (double start : {0.10, 0.40})
        early.add(Spectrum(audio, start * decay, length), frequency);
    for (double start : {2.20, 2.60, 3.00})
        late.add(Spectrum(audio, start * decay, length), frequency);

    char context[160];
    std::snprintf(context, sizeof(context), "note=%d rate=%.0f velocity=%.2f colour=%.2f decay=%.2f seed=%u",
                  note, hostRate, velocity, colour, decay, seed);
    const std::string label(context);
    double prominence = 1000.0;
    for (std::size_t band = 0; band < 2; ++band)
    {
        check(early.amplitude(band) > 1.0e-4 && late.amplitude(band) > 3.0e-6,
              label + ": both partials remain measurable; silence cannot pass");
        prominence = std::min({prominence, early.prominenceDb(band), late.prominenceDb(band)});
    }
    check(prominence > 8.0, label + ": h6 and h7 exceed adjacent off-partial probes");
    const double change = late.ratioDb() - early.ratioDb();
    // Allow 3 dB (a factor sqrt(2) in amplitude ratio) for course interference
    // and finite windows. The old damping boundary
    // produces a much larger rise over this interval. Do not require every
    // individual window or every pair of neighbouring partials to be monotone.
    check(change < 3.0, label + ": pooled h7/h6 balance has no large tail rise");
    return {change, prominence};
}

void registerRateAndStrikeCoverage()
{
    struct Touch { float velocity, colour; };
    constexpr std::array<Touch, 2> touches {{{0.35f, 0.45f}, {0.95f, 0.80f}}};
    for (int note : {24, 36})
        for (double hostRate : {44100.0, 48000.0, 96000.0})
            for (const auto touch : touches)
            {
                double minimum = 1000.0, maximum = -1000.0, prominence = 1000.0;
                for (std::uint32_t seed : {123u, 0x7812a3u, 0xeff00d33u})
                {
                    const auto result = measure(note, hostRate, touch.velocity, touch.colour, 1.0f, seed);
                    minimum = std::min(minimum, result.change);
                    maximum = std::max(maximum, result.change);
                    prominence = std::min(prominence, result.minimumProminence);
                }
                std::printf("PIANO TAIL note=%d rate=%.0f velocity=%.2f colour=%.2f change=[%+.4f,%+.4f] dB prominence=%.2f dB\n",
                            note, hostRate, touch.velocity, touch.colour, minimum, maximum, prominence);
            }
}

void decayControlCoverage()
{
    // Endpoint controls use the same normalized analysis times. Multiple
    // seeds and windows still matter because course detuning does not scale
    // with the damping control. Full rate/strike coverage is above.
    for (float decay : {0.25f, 4.0f})
        for (int note : {24, 36})
        {
            double maximum = -1000.0;
            for (std::uint32_t seed : {123u, 0xeff00d33u})
                maximum = std::max(maximum, measure(note, 48000.0, 0.85f, 0.65f, decay, seed).change);
            std::printf("PIANO DECAY note=%d decay=%.2f maximum_change=%+.4f dB\n", note, decay, maximum);
        }
}
}

int main()
{
    registerRateAndStrikeCoverage();
    decayControlCoverage();
    std::printf("Piano partial decay: %d checks, %d failures; %llu generated samples\n",
                checks, failures, static_cast<unsigned long long>(generatedSamples));
    return failures == 0 ? 0 : 1;
}

// A bowed string changes pitch against a stationary instrument body. Compare
// rendered harmonic ratios after a slide with the same target played directly;
// this rejects the old behavior that transposed the body's spectral shape.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }
struct Audio
{
    double rate;
    std::vector<float> samples;
};
Audio render(double from, double to, float size, double hostRate)
{
    Audio result {hostRate * 2.0, {}};
    result.samples.resize(static_cast<std::size_t>(1.6 * result.rate));
    cs80::body::InstrumentBody body;
    body.prepare(result.rate, hostRate);
    body.noteOn(cs80::body::bowed, static_cast<float>(from), 0.8f, 0.65f,
                1.0f, 0x7812a3u, size);
    bool finite = true;
    double peak = 0.0;
    for (std::size_t i = 0; i < result.samples.size(); ++i)
    {
        const double time = i / result.rate;
        const double position = std::clamp((time - 0.25) / 0.35, 0.0, 1.0);
        const float frequency = static_cast<float>(hz(from + (to - from) * position));
        const float value = body.tick(frequency, 0.65f);
        result.samples[i] = value;
        finite = finite && std::isfinite(value);
        peak = std::max(peak, std::abs(static_cast<double>(value)));
    }
    check(finite && peak < 0.8, "Moving bowed excitation remains finite and inside its mode-sum headroom");
    return result;
}
double amplitude(const Audio& audio, double frequency)
{
    const int first = static_cast<int>(0.9 * audio.rate);
    const int last = static_cast<int>(1.5 * audio.rate);
    const double angle = 2.0 * pi * frequency / audio.rate;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    double real = 1.0, imaginary = 0.0, sumReal = 0.0, sumImaginary = 0.0, weightSum = 0.0;
    for (int i = first; i < last; ++i)
    {
        const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
        const double value = audio.samples[static_cast<std::size_t>(i)] * weight;
        sumReal += value * real;
        sumImaginary += value * imaginary;
        weightSum += weight;
        const double next = real * cosine - imaginary * sine;
        imaginary = imaginary * cosine + real * sine;
        real = next;
    }
    return 2.0 * std::hypot(sumReal, sumImaginary) / weightSum;
}

void spectralTracking()
{
    // The fractional targets cover ordinary vibrato-sized excursions. The
    // octave slides expose large erroneous formant transposition unambiguously.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (float size : {0.75f, 1.0f, 1.8f})
            for (const auto notes : {std::array<double, 2>{36.0, 48.0},
                                     {48.0, 36.0}, {60.0, 72.0}, {72.0, 60.0},
                                     {60.0, 60.35}, {60.0, 59.65}})
            {
                const auto moved = render(notes[0], notes[1], size, rate);
                const auto direct = render(notes[1], notes[1], size, rate);
                const double frequency = hz(notes[1]);
                const double movedBase = amplitude(moved, frequency);
                const double directBase = amplitude(direct, frequency);
                check(movedBase > 0.01 && directBase > 0.01,
                      "Target-pitch comparison has a measurable fundamental");
                double maximumError = 0.0;
                int measured = 0;
                for (int harmonic : {2, 3, 4, 5, 8})
                {
                    if (frequency * harmonic >= rate * 0.38) continue;
                    const double target = amplitude(direct, frequency * harmonic) / directBase;
                    if (target < 0.003) continue;
                    const double actual = amplitude(moved, frequency * harmonic) / movedBase;
                    const double error = std::abs(db(actual / target));
                    maximumError = std::max(maximumError, error);
                    check(error < 0.15,
                          "A slide changes source pitch while preserving the target body's harmonic ratios");
                    ++measured;
                }
                check(measured >= 3, "Each register/size comparison measures several audible harmonics");
                std::printf("BOWED rate=%.0f size=%.2f from=%.2f to=%.2f maximum_ratio_error=%.6f dB\n",
                            rate, size, notes[0], notes[1], maximumError);
            }
}

void continuousPitchSafety()
{
    // Wide bends and fast vibrato must keep finite, bounded output through
    // changing headroom factors, host Nyquist fades and return to onset pitch.
    for (double hostRate : {44100.0, 48000.0, 96000.0})
        for (float size : {0.5f, 2.5f})
        {
            cs80::body::InstrumentBody body;
            const double rate = hostRate * 2.0;
            body.prepare(rate, hostRate);
            body.noteOn(cs80::body::bowed, 60.0f, 1.0f, 1.0f, 1.0f, 0x12345678u, size);
            bool finite = true;
            double peak = 0.0;
            for (int i = 0; i < static_cast<int>(rate); ++i)
            {
                const double t = i / rate;
                const float frequency = static_cast<float>(hz(60.0 + 36.0 * std::sin(2.0 * pi * 7.0 * t)));
                const float value = body.tick(frequency, 1.0f, 1.0f);
                finite = finite && std::isfinite(value);
                peak = std::max(peak, std::abs(static_cast<double>(value)));
            }
            check(finite && peak < 0.8, "Continuous wide pitch motion respects excitation headroom");
        }
}
}

int main()
{
    spectralTracking();
    continuousPitchSafety();
    std::printf("Bowed body formants: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

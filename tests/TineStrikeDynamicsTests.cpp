// Rendered touch-response contracts for the tine body. These check the
// excitation before the rank VCA supplies overall playing dynamics. A softer
// spectrum must not acquire a louder fundamental through normalization.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description); }
}

double db(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-15));
}

struct Measurement
{
    double fundamental = 0.0, third = 0.0, rms = 0.0, peak = 0.0;
    bool finite = true;
};

Measurement render(double hostRate, int note, float velocity, float colour)
{
    const double rate = hostRate * 2.0;
    const float frequency = static_cast<float>(440.0 * std::exp2((note - 69.0) / 12.0));
    const int samples = static_cast<int>(std::lround(0.30 * rate));
    const int first = static_cast<int>(std::lround(0.04 * rate));
    const int last = static_cast<int>(std::lround(0.28 * rate));
    cs80::body::InstrumentBody instrument;
    instrument.prepare(rate, hostRate);
    instrument.noteOn(cs80::body::tine, static_cast<float>(note), velocity,
                      colour, 1.0f, 0x7812a3u);
    double weightSum = 0.0, squared = 0.0;
    std::array<double, 2> real {}, imaginary {};
    Measurement measurement;
    for (int i = 0; i < samples; ++i)
    {
        const double sample = instrument.tick(frequency, colour);
        measurement.finite = measurement.finite && std::isfinite(sample);
        measurement.peak = std::max(measurement.peak, std::abs(sample));
        if (i < first || i >= last) continue;
        const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
        weightSum += weight;
        squared += sample * sample;
        for (int harmonic = 0; harmonic < 2; ++harmonic)
        {
            const double angle = 2.0 * pi * frequency * (harmonic == 0 ? 1.0 : 3.0) * i / rate;
            real[static_cast<std::size_t>(harmonic)] += sample * weight * std::cos(angle);
            imaginary[static_cast<std::size_t>(harmonic)] += sample * weight * std::sin(angle);
        }
    }
    measurement.fundamental = 2.0 * std::hypot(real[0], imaginary[0]) / weightSum;
    measurement.third = 2.0 * std::hypot(real[1], imaginary[1]) / weightSum;
    measurement.rms = std::sqrt(squared / (last - first));
    return measurement;
}
}

int main()
{
    constexpr std::array<float, 4> velocities {0.2f, 0.5f, 0.8f, 1.0f};
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int note : {36, 60, 84})
            for (float colour : {0.0f, 0.65f, 1.0f})
            {
                std::array<Measurement, velocities.size()> captures;
                for (std::size_t i = 0; i < velocities.size(); ++i)
                {
                    captures[i] = render(rate, note, velocities[i], colour);
                    check(captures[i].finite && captures[i].peak < 0.8,
                          "Every strike is finite and retains excitation headroom");
                    check(captures[i].fundamental > 0.1 && captures[i].third > 1.0e-4,
                          "Soft and firm strikes retain measurable fundamental and upper tone");
                }
                const auto& firm = captures.back();
                for (std::size_t i = 0; i + 1 < captures.size(); ++i)
                {
                    const auto& softer = captures[i];
                    check(std::abs(db(softer.fundamental / firm.fundamental)) < 0.15,
                          "Fundamental does not receive reciprocal gain from a softer spectrum");
                    check(db(captures[i + 1].rms / softer.rms) > -0.05,
                          "Stronger excitation does not reduce early body RMS");
                    check(captures[i + 1].third / captures[i + 1].fundamental
                            > 1.10 * softer.third / softer.fundamental,
                          "Each velocity step increases upper-to-fundamental tone");
                }
                const double colourChange = db((firm.third / firm.fundamental)
                    / (captures.front().third / captures.front().fundamental));
                check(colourChange > 10.0,
                      "Soft-to-firm spectral contrast remains substantial at every register");
                std::printf("TINE rate=%5.0f note=%d colour=%.2f fundamental_delta=%+.5f dB "
                            "rms_delta=%+.3f dB third_ratio_delta=%+.3f dB peak=%.5f\n",
                            rate, note, colour,
                            db(firm.fundamental / captures.front().fundamental),
                            db(firm.rms / captures.front().rms), colourChange, firm.peak);
            }
    std::printf("Tine strike dynamics: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

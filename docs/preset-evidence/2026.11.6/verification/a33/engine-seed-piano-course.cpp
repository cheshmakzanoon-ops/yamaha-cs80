// Existing geometry regression with rank-I accepted strike ages 1, 2 and 3.
// Rendered piano-course stability: a hammer changes excitation, not the tuning
// of the instrument's neighbouring string. Frequency is measured from audio;
// no test access to the private modal arrays or random generator is required.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
using cs80::body::InstrumentBody;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;

void check(bool condition, const std::string& description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description.c_str()); }
}

double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }

struct Projection
{
    double real = 0.0, imaginary = 0.0, weight = 0.0;
};

struct FrequencyMeasurement
{
    std::array<double, 2> cents {};
    bool finite = true;
    double peak = 0.0;
};

// Two overlapping Hann-window projections measure phase advance of the late
// resonant tail. At this point the fast principal polarization has decayed.
// The interval is short enough to resolve the bounded detuning without phase
// wrapping, while each full window rejects other harmonics. The test does not
// search for the implementation's authored detune value.
FrequencyMeasurement measureFrequency(int note, double hostRate, std::uint32_t seed,
                                      int oversampling = 1)
{
    const double rate = hostRate * oversampling;
    const float pitch = static_cast<float>(hz(note));
    constexpr double begin = 12.0, windowSeconds = 1.0, separation = 0.25;
    const int first = static_cast<int>(std::lround(begin * rate));
    const int window = static_cast<int>(std::lround(windowSeconds * rate));
    const int offset = static_cast<int>(std::lround(separation * rate));
    const int end = first + window + offset;
    // The existing piano string's stiffness law defines its unison reference.
    // The regression measures whether one SECOND course stays coherent and
    // seed-independent, not whether this authored stiffness fits a real piano.
    const double treble = std::clamp((note - 48.0) / 40.0, 0.0, 1.6);
    const double stiffness = 0.000045 + 0.00020 * treble * treble;
    const std::array<double, 2> reference {
        pitch, pitch * 2.0 * std::sqrt((1.0 + 4.0 * stiffness) / (1.0 + stiffness))
    };
    std::array<double, 2> real {1.0, 1.0}, imaginary {}, cosine {}, sine {};
    std::array<std::array<Projection, 2>, 2> projection {};
    for (std::size_t harmonic = 0; harmonic < reference.size(); ++harmonic)
    {
        cosine[harmonic] = std::cos(2.0 * pi * reference[harmonic] / rate);
        sine[harmonic] = std::sin(2.0 * pi * reference[harmonic] / rate);
    }
    InstrumentBody piano;
    piano.prepare(rate, hostRate);
    piano.noteOn(cs80::body::piano, static_cast<float>(note), 0.85f, 0.65f, 1.0f, seed);
    FrequencyMeasurement result;
    for (int sample = 0; sample < end; ++sample)
    {
        const double value = piano.tick(pitch, 0.65f);
        result.finite = result.finite && std::isfinite(value);
        result.peak = std::max(result.peak, std::abs(value));
        for (int which = 0; which < 2; ++which)
        {
            const int position = sample - first - which * offset;
            if (position >= 0 && position < window)
            {
                const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * position / (window - 1));
                for (std::size_t harmonic = 0; harmonic < reference.size(); ++harmonic)
                {
                    auto& sum = projection[harmonic][static_cast<std::size_t>(which)];
                    sum.real += weight * value * real[harmonic];
                    sum.imaginary -= weight * value * imaginary[harmonic];
                    sum.weight += weight;
                }
            }
        }
        for (std::size_t harmonic = 0; harmonic < reference.size(); ++harmonic)
        {
            const double nextReal = real[harmonic] * cosine[harmonic]
                                  - imaginary[harmonic] * sine[harmonic];
            imaginary[harmonic] = imaginary[harmonic] * cosine[harmonic]
                               + real[harmonic] * sine[harmonic];
            real[harmonic] = nextReal;
        }
    }
    for (std::size_t harmonic = 0; harmonic < reference.size(); ++harmonic)
    {
        const auto& a = projection[harmonic][0];
        const auto& b = projection[harmonic][1];
        const double phaseAdvance = std::atan2(b.imaginary * a.real - b.real * a.imaginary,
                                              b.real * a.real + b.imaginary * a.imaginary);
        const double frequency = reference[harmonic] + phaseAdvance / (2.0 * pi * separation);
        result.cents[harmonic] = 1200.0 * std::log2(frequency / reference[harmonic]);
        check(std::hypot(a.real, a.imaginary) / a.weight > 1.0e-6,
              "late course has enough signal for an independent frequency measurement");
    }
    return result;
}

void repeatedStrikes()
{
    double maximumSeedSpread = 0.0, maximumCourseMismatch = 0.0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int note : {60, 72, 84})
        {
            const auto reference = measureFrequency(note, rate, 0x9e3779b9u);
            check(reference.finite && reference.peak < 0.80, "reference piano strike remains finite and bounded");
            for (auto seed : {0x3c6ef372u, 0xdaa66d2bu})
            {
                const auto strike = measureFrequency(note, rate, seed);
                check(strike.finite && strike.peak < 0.80, "alternate piano strike remains finite and bounded");
                for (std::size_t harmonic = 0; harmonic < strike.cents.size(); ++harmonic)
                {
                    const double spread = std::abs(strike.cents[harmonic] - reference.cents[harmonic]);
                    maximumSeedSpread = std::max(maximumSeedSpread, spread);
                    check(spread < 0.01, "repeated strikes cannot retune a piano course");
                    check(strike.cents[harmonic] > 0.5 && strike.cents[harmonic] < 1.8,
                          "both measured partials belong to the same nearby upper course");
                }
                const double mismatch = std::abs(strike.cents[0] - strike.cents[1]);
                maximumCourseMismatch = std::max(maximumCourseMismatch, mismatch);
                check(mismatch < 0.01, "one neighbouring string has coherent tuning across its partials");
            }
        }
    const auto base = measureFrequency(72, 48000.0, 0x9e3779b9u);
    const auto twice = measureFrequency(72, 48000.0, 0x9e3779b9u, 2);
    for (std::size_t harmonic = 0; harmonic < base.cents.size(); ++harmonic)
        check(std::abs(base.cents[harmonic] - twice.cents[harmonic]) < 0.002,
              "course pitch stays stable at the production 2x internal rate");
    std::printf("PIANO COURSE maximum_seed_spread=%.8f cents maximum_partial_mismatch=%.8f cents\n",
                maximumSeedSpread, maximumCourseMismatch);
}

void variedExcitationAndBoundaries()
{
    double minimumDifference = 1.0, maximumBoundaryDifference = 0.0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int note : {24, 36, 60, 84, 108})
        {
            InstrumentBody first, second;
            first.prepare(2.0 * rate, rate);
            second.prepare(2.0 * rate, rate);
            first.noteOn(cs80::body::piano, static_cast<float>(note), 0.75f, 0.65f, 1.0f, 1234u);
            second.noteOn(cs80::body::piano, static_cast<float>(note), 0.75f, 0.65f, 1.0f, 5678u);
            double energy = 0.0, difference = 0.0;
            bool finite = true;
            for (int sample = 0; sample < static_cast<int>(0.12 * rate); ++sample)
            {
                const double a = first.tick(static_cast<float>(hz(note)), 0.65f);
                const double b = second.tick(static_cast<float>(hz(note)), 0.65f);
                finite = finite && std::isfinite(a) && std::isfinite(b) && std::abs(a) < 0.80 && std::abs(b) < 0.80;
                energy += a * a;
                difference += (a - b) * (a - b);
            }
            const double relative = std::sqrt(difference / std::max(energy, 1.0e-20));
            minimumDifference = std::min(minimumDifference, relative);
            check(finite, "varied hammer excitations retain headroom across the keyboard and sample rates");
            check(relative > 0.002, "stable tuning does not remove independently varying hammer excitation");
        }
    for (float boundary : {30.0f, 32.0f, 47.0f, 49.0f, 60.0f})
    {
        InstrumentBody lower, upper;
        lower.prepare(96000.0, 48000.0);
        upper.prepare(96000.0, 48000.0);
        lower.noteOn(cs80::body::piano, boundary - 0.0001f, 0.85f, 0.65f, 1.0f, 9876u);
        upper.noteOn(cs80::body::piano, boundary + 0.0001f, 0.85f, 0.65f, 1.0f, 9876u);
        double energy = 0.0, difference = 0.0;
        // Equal sounding pitch isolates onset geometry from a frequency
        // difference's expected phase accumulation.
        const float frequency = static_cast<float>(hz(boundary));
        for (int sample = 0; sample < 192000; ++sample)
        {
            const double a = lower.tick(frequency, 0.65f);
            const double b = upper.tick(frequency, 0.65f);
            energy += a * a;
            difference += (a - b) * (a - b);
        }
        const double relative = std::sqrt(difference / std::max(energy, 1.0e-20));
        maximumBoundaryDifference = std::max(maximumBoundaryDifference, relative);
        check(relative < 0.0005, "fractional tuning across piano register boundaries has no course discontinuity");
    }
    std::printf("PIANO EXCITATION minimum_relative_strike_difference=%.8f maximum_register_boundary_difference=%.8f\n",
                minimumDifference, maximumBoundaryDifference);
}
}

int main()
{
    repeatedStrikes();
    variedExcitationAndBoundaries();
    std::printf("Piano course geometry: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

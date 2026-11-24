// Electrical geometry and rendered behavior of the EP pickup approximation.
// Numerical quadrature is an independent oracle for the analytic coefficients;
// rendered tests exercise the actual oscillator bank, its gains and rate gate.
// These contracts establish behavior and safety, not recorded-instrument fidelity.
#include "core/ElectricPickup.h"
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description); }
}

double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }

void spatialDerivativeOracle()
{
    // Integrate the time derivative of each spatial proxy itself. This has
    // neither the implementation's complex root nor its harmonic recurrence.
    constexpr int points = 8192;
    double largestError = 0.0;
    for (bool capacitive : {false, true})
        for (double size : {0.5, 1.0, 2.5})
            for (double colour : {0.0, 0.65, 1.0})
                for (double velocity : {0.2, 1.0})
                    for (double motion : {0.1, 1.0})
                    {
                        ElectricPickup pickup;
                        pickup.configure(capacitive, size, velocity);
                        const auto analytic = pickup.coefficients(colour, motion);
                        ElectricPickup::Spectrum measured {};
                        const double amplitude = (capacitive ? 0.38 + 0.35 * colour : 0.42 + 0.46 * colour)
                                               * (0.22 + 0.78 * velocity) * motion;
                        const double offset = 0.665 + 0.11 * (size - 1.0);
                        const double gap = 1.0 + 0.10 * (size - 1.0);
                        for (int point = 0; point < points; ++point)
                        {
                            const double angle = 2.0 * pi * point / points;
                            const double position = offset + amplitude * std::cos(angle);
                            const double distance = gap - amplitude * std::cos(angle);
                            const double sample = capacitive
                                ? -amplitude * std::sin(angle) / (distance * distance)
                                : 2.0 * amplitude * position * std::sin(angle)
                                    / std::pow(1.0 + position * position, 2.0);
                            for (int harmonic = 1; harmonic <= ElectricPickup::harmonicCount; ++harmonic)
                                measured[static_cast<std::size_t>(harmonic - 1)]
                                    += sample * std::sin(harmonic * angle);
                        }
                        for (std::size_t i = 0; i < measured.size(); ++i)
                        {
                            const double error = std::abs(analytic[i] - measured[i] / measured[0]);
                            largestError = std::max(largestError, error);
                            check(error < 2.0e-11, "Signed coefficients match the independently integrated pickup derivative");
                        }
                    }
    std::printf("PICKUP quadrature maximum coefficient error %.3g\n", largestError);
}

void boundedExcursion()
{
    double largestFraction = 0.0;
    for (bool capacitive : {false, true})
        for (int geometry = 0; geometry <= 20; ++geometry)
        {
            const double size = 0.5 + 0.1 * geometry;
            ElectricPickup full;
            full.configure(capacitive, size, 1.0);
            const double bound = full.fullStrikeBound();
            for (int touch = 0; touch <= 10; ++touch)
            {
                ElectricPickup pickup;
                pickup.configure(capacitive, size, 0.1 * touch);
                check(pickup.fullStrikeBound() == bound,
                      "A softer strike never changes the normalization denominator");
                for (int colour = 0; colour <= 10; ++colour)
                    for (double motion : {0.0, 1.0e-30, 1.0e-8, 0.03, 0.15, 0.4, 0.7, 1.0})
                    {
                        const auto coefficients = pickup.coefficients(0.1 * colour, motion);
                        double sum = 0.0;
                        bool finite = true;
                        for (const double coefficient : coefficients)
                        {
                            finite = finite && std::isfinite(coefficient);
                            sum += std::abs(coefficient);
                        }
                        largestFraction = std::max(largestFraction, sum / bound);
                        check(finite && coefficients[0] == 1.0 && sum <= bound + 1.0e-12,
                              "Every allowed pickup spectrum is finite and fits its full-strike bound");
                        if (motion == 0.0)
                            check(sum == 1.0, "Vanishing excursion leaves only the normalized fundamental");
                    }
            }
        }
    for (bool capacitive : {false, true})
    {
        ElectricPickup pickup;
        pickup.configure(capacitive, std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::quiet_NaN());
        const auto coefficients = pickup.coefficients(std::numeric_limits<double>::quiet_NaN(),
                                                      std::numeric_limits<double>::infinity());
        check(coefficients[0] == 1.0 && coefficients[1] == 0.0
                && std::isfinite(pickup.fullStrikeBound()),
              "Invalid geometry, touch and motion inputs have a finite fallback");
    }
    std::printf("PICKUP maximum absolute-spectrum / bound %.6f\n", largestFraction);
}

struct Audio
{
    double rate;
    std::vector<float> samples;
    double peak = 0.0;
    bool finite = true;
};

Audio render(int model, int note, float velocity, float colour, float size,
             double hostRate = 48000.0, double seconds = 1.55, int oversampling = 2)
{
    Audio result {hostRate * oversampling, {}};
    result.samples.resize(static_cast<std::size_t>(std::lround(result.rate * seconds)));
    InstrumentBody instrument;
    instrument.prepare(result.rate, hostRate);
    instrument.noteOn(model, static_cast<float>(note), velocity, colour, 1.0f, 0x7812a3u, size);
    for (float& sample : result.samples)
    {
        sample = instrument.tick(static_cast<float>(hz(note)), colour);
        result.finite = result.finite && std::isfinite(sample);
        result.peak = std::max(result.peak, std::abs(static_cast<double>(sample)));
    }
    return result;
}

struct Measurement
{
    std::array<double, 4> partials {};
    double rms = 0.0;
};

Measurement measure(const Audio& audio, double frequency, double begin, double end)
{
    const int first = static_cast<int>(std::lround(audio.rate * begin));
    const int last = static_cast<int>(std::lround(audio.rate * end));
    std::array<double, 4> real {}, imaginary {};
    double weighted = 0.0, squared = 0.0;
    for (int sample = first; sample < last; ++sample)
    {
        const double value = audio.samples[static_cast<std::size_t>(sample)];
        const double window = 0.5 - 0.5 * std::cos(2.0 * pi * (sample - first) / (last - first - 1));
        weighted += window;
        squared += value * value;
        for (std::size_t harmonic = 0; harmonic < real.size(); ++harmonic)
        {
            const double angle = 2.0 * pi * frequency * (harmonic + 1) * sample / audio.rate;
            real[harmonic] += value * window * std::cos(angle);
            imaginary[harmonic] += value * window * std::sin(angle);
        }
    }
    Measurement result;
    for (std::size_t harmonic = 0; harmonic < real.size(); ++harmonic)
        result.partials[harmonic] = 2.0 * std::hypot(real[harmonic], imaginary[harmonic]) / weighted;
    result.rms = std::sqrt(squared / (last - first));
    return result;
}

void renderedGeometryTouchAndDecay()
{
    double minimumGeometry = 100.0, minimumTouch = 100.0, leastDecay = -100.0, peak = 0.0;
    for (int model : {tine, reed})
        for (int note : {36, 60, 84})
            for (double rate : {44100.0, 48000.0, 96000.0})
            {
                const double frequency = static_cast<float>(hz(note));
                const auto small = render(model, note, 1.0f, 0.65f, 0.5f, rate);
                const auto large = render(model, note, 1.0f, 0.65f, 2.5f, rate);
                const auto soft = render(model, note, 0.2f, 0.65f, 1.0f, rate);
                const auto firm = render(model, note, 1.0f, 0.65f, 1.0f, rate);
                const auto s = measure(small, frequency, 0.04, 0.28);
                const auto l = measure(large, frequency, 0.04, 0.28);
                const auto a = measure(soft, frequency, 0.04, 0.28);
                const auto b = measure(firm, frequency, 0.04, 0.28);
                const auto tail = measure(firm, frequency, 1.25, 1.50);
                // Alignment can reverse an even partial while leaving its
                // magnitude similar. Measure the spectral balance across
                // several harmonics; the signed quadrature oracle above
                // independently checks the phase information as well.
                double geometry = 0.0;
                for (std::size_t harmonic = 1; harmonic < s.partials.size(); ++harmonic)
                    geometry = std::max(geometry, std::abs(db((s.partials[harmonic] / s.partials[0])
                        / (l.partials[harmonic] / l.partials[0]))));
                const double touch = db((b.partials[2] / b.partials[0])
                                         / (a.partials[2] / a.partials[0]));
                const double decay = db((tail.partials[2] / tail.partials[0])
                                         / (b.partials[2] / b.partials[0]));
                minimumGeometry = std::min(minimumGeometry, geometry);
                minimumTouch = std::min(minimumTouch, touch);
                leastDecay = std::max(leastDecay, decay);
                for (const auto* audio : {&small, &large, &soft, &firm})
                {
                    peak = std::max(peak, audio->peak);
                    check(audio->finite && audio->peak < 0.72,
                          "Rendered pickup geometry remains finite and leaves output headroom");
                }
                check(geometry > 1.0, "Size changes measured pickup harmonic balance at fixed playing pitch");
                check(touch > 10.0, "Pickup excursion retains strong matched-level velocity colour");
                check(std::abs(db(a.partials[0] / b.partials[0])) < 0.03,
                      "Soft pickup spectra receive no reciprocal boost of the fundamental");
                check(db(b.rms / a.rms) > -0.02, "Firmer pickup excitation never reduces early RMS");
                check(decay < -6.0, "The pickup's upper tone recedes as excursion decays");
                if (note == 60 && rate == 48000.0)
                    std::printf("PICKUP %-4s geometry %.3f dB touch_h3 %.3f dB decay_h3 %.3f dB peak %.6f\n",
                                model == tine ? "tine" : "reed", geometry, touch, decay, firm.peak);
            }
    std::printf("PICKUP rendered minima: geometry %.3f dB touch %.3f dB; least decay %.3f dB peak %.6f\n",
                minimumGeometry, minimumTouch, leastDecay, peak);
}

double tone(const Audio& audio, double frequency, double begin, double end)
{
    return measure(audio, frequency, begin, end).partials[0];
}

void pitchRateAndNyquist()
{
    double largestRateDifference = 0.0, largestAlias = -300.0;
    for (int model : {tine, reed})
        for (float size : {0.5f, 1.0f, 2.5f})
        {
            const double frequency = static_cast<float>(hz(60));
            const auto reference = render(model, 60, 1.0f, 0.65f, size, 48000.0, 0.32);
            const auto expected = measure(reference, frequency, 0.04, 0.28);
            // An independent local peak search detects an accidental geometry
            // retune; changing amplitude or dividing spectra cannot pass it.
            double loudest = -1.0, peakHz = 0.0;
            for (int step = -8; step <= 8; ++step)
            {
                const double probe = frequency + 0.1 * step;
                const double level = tone(reference, probe, 0.04, 0.28);
                if (level > loudest) { loudest = level; peakHz = probe; }
            }
            check(std::abs(1200.0 * std::log2(peakHz / frequency)) < 0.7,
                  "Pickup geometry preserves the played fundamental pitch");
            for (double rate : {44100.0, 48000.0, 96000.0})
                for (int oversampling : {1, 2, 4})
                {
                    const auto audio = render(model, 60, 1.0f, 0.65f, size, rate, 0.32, oversampling);
                    const auto actual = measure(audio, frequency, 0.04, 0.28);
                    for (std::size_t harmonic = 0; harmonic < expected.partials.size(); ++harmonic)
                    {
                        const double change = std::abs(db(actual.partials[harmonic] / expected.partials[harmonic]));
                        largestRateDifference = std::max(largestRateDifference, change);
                        check(change < 0.08, "Pickup partial levels remain stable across host rate and oversampling");
                    }
                }
        }
    // A high note would fold strong h3/h4/h5 into these non-harmonic bins if
    // the nonlinear pickup were evaluated in the audio path without a gate.
    for (int model : {tine, reed})
        for (double hostRate : {44100.0, 48000.0})
        {
            const auto audio = render(model, 120, 1.0f, 1.0f, 0.5f, hostRate, 0.28, 1);
            const double frequency = static_cast<float>(hz(120));
            const double fundamental = tone(audio, frequency, 0.06, 0.26);
            for (int harmonic : {3, 4, 5, 6})
            {
                const double folded = std::abs(std::remainder(harmonic * frequency, hostRate));
                const double alias = db(tone(audio, folded, 0.06, 0.26) / fundamental);
                largestAlias = std::max(largestAlias, alias);
                check(alias < -75.0, "Above-Nyquist pickup harmonics cannot fold into audible non-harmonic tones");
            }
            check(audio.finite && audio.peak < 0.72, "High-register pickup remains finite and bounded");
        }
    std::printf("PICKUP rate maximum partial difference %.5f dB; largest folded-bin level %.2f dBc\n",
                largestRateDifference, largestAlias);
}
}

int main()
{
    spatialDerivativeOracle();
    boundedExcursion();
    renderedGeometryTouchAndDecay();
    pitchRateAndNyquist();
    std::printf("Electric pickup: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

// Rendered excitation checks, independent of the rank VCA. A quieter strike
// must not receive a reciprocal normalization boost as its upper modes recede.
// These tests describe playing response and headroom, not acoustic equivalence.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;

void check(bool condition, const std::string& description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description.c_str()); }
}

double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }

struct Measurement
{
    double fundamental = 0.0, upper = 0.0, rms = 0.0, peak = 0.0;
    bool finite = true;
};

struct Case { int model; double upper; };
constexpr std::array<Case, 11> struck {{
    {piano, 3.0}, {tine, 3.0}, {reed, 3.0}, {clav, 3.0},
    {nylon, 3.0}, {steel, 3.0}, {bass, 3.0},
    {bell, 2.756}, {vibes, 4.0}, {marimba, 4.0}, {celesta, 2.76}
}};

Measurement measure(Case test, double rate, int note, float velocity, float colour,
                    float size = 1.0f)
{
    const double internalRate = rate * 2.0;
    const float frequency = static_cast<float>(hz(note));
    const int first = static_cast<int>(std::lround(0.04 * internalRate));
    const int last = static_cast<int>(std::lround(0.30 * internalRate));
    InstrumentBody instrument;
    instrument.prepare(internalRate, rate);
    instrument.noteOn(test.model, static_cast<float>(note), velocity, colour, 1.0f, 0x7812a3u, size);
    Measurement result;
    double squared = 0.0, weightSum = 0.0;
    std::array<double, 2> real {1.0, 1.0}, imaginary {}, cosine {}, sine {}, sumReal {}, sumImaginary {};
    for (std::size_t tone = 0; tone < 2; ++tone)
    {
        const double angle = 2.0 * pi * frequency * (tone == 0 ? 1.0 : test.upper) / internalRate;
        cosine[tone] = std::cos(angle);
        sine[tone] = std::sin(angle);
    }
    for (int i = 0; i < last; ++i)
    {
        const double sample = instrument.tick(frequency, colour);
        result.finite = result.finite && std::isfinite(sample);
        result.peak = std::max(result.peak, std::abs(sample));
        if (i >= first)
        {
            const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
            squared += sample * sample;
            weightSum += weight;
            for (std::size_t tone = 0; tone < 2; ++tone)
            {
                sumReal[tone] += sample * weight * real[tone];
                sumImaginary[tone] += sample * weight * imaginary[tone];
            }
        }
        for (std::size_t tone = 0; tone < 2; ++tone)
        {
            const double nextReal = real[tone] * cosine[tone] - imaginary[tone] * sine[tone];
            imaginary[tone] = imaginary[tone] * cosine[tone] + real[tone] * sine[tone];
            real[tone] = nextReal;
        }
    }
    result.fundamental = 2.0 * std::hypot(sumReal[0], sumImaginary[0]) / weightSum;
    result.upper = 2.0 * std::hypot(sumReal[1], sumImaginary[1]) / weightSum;
    result.rms = std::sqrt(squared / (last - first));
    return result;
}

void strikeResponse()
{
    constexpr std::array<float, 4> velocities {0.0f, 0.2f, 0.65f, 1.0f};
    for (const auto test : struck)
    {
        double worstFundamentalStep = 100.0, worstRmsStep = 100.0, peak = 0.0;
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (int note : {36, 60, 84})
                for (float colour : {0.0f, 0.65f, 1.0f})
                {
                    // Both geometry extremes are included without coupling a
                    // geometry change to the velocity comparison itself.
                    const float size = note == 36 ? 2.5f : note == 84 ? 0.5f : 1.0f;
                    std::array<Measurement, velocities.size()> captures;
                    for (std::size_t v = 0; v < velocities.size(); ++v)
                    {
                        captures[v] = measure(test, rate, note, velocities[v], colour, size);
                        const auto& m = captures[v];
                        peak = std::max(peak, m.peak);
                        check(m.finite && m.peak < 0.80 && m.fundamental > 1.0e-5 && m.rms > 1.0e-5,
                              std::string(name(test.model)) + ": every strike remains finite, sounding and bounded");
                        if (v != 0)
                        {
                            const double fundamentalStep = db(m.fundamental / captures[v - 1].fundamental);
                            const double rmsStep = db(m.rms / captures[v - 1].rms);
                            worstFundamentalStep = std::min(worstFundamentalStep, fundamentalStep);
                            worstRmsStep = std::min(worstRmsStep, rmsStep);
                            // Small tolerances cover finite-window leakage,
                            // inharmonic beats and quiet contact noise. The
                            // former normalization defect is many times larger.
                            check(fundamentalStep > -0.04,
                                  std::string(name(test.model)) + ": a softer strike cannot boost the fundamental");
                            check(rmsStep > -0.05,
                                  std::string(name(test.model)) + ": increasing strike cannot reduce body RMS");
                        }
                    }
                    if (colour == 0.65f)
                    {
                        const auto& soft = captures[1];
                        const auto& firm = captures.back();
                        // A ratio of measured partials removes common output
                        // gain, so loudness alone cannot pass this contrast.
                        const double spectralContrast = db((firm.upper / firm.fundamental)
                                                            / (soft.upper / soft.fundamental));
                        check(spectralContrast > 0.25,
                              std::string(name(test.model)) + ": stronger strike still changes matched-level spectral color");
                    }
                }
        std::printf("STRIKE %-20s minimum_fundamental_step=%+.5f dB minimum_rms_step=%+.5f dB peak=%.6f\n",
                    name(test.model), worstFundamentalStep, worstRmsStep, peak);
    }
}

void boundaryHeadroom()
{
    for (const auto test : struck)
        for (double hostRate : {44100.0, 96000.0})
            for (int note : {0, 24, 96, 127})
                for (float colour : {0.0f, 1.0f})
                    for (float velocity : {0.0f, 1.0f})
                    {
                        InstrumentBody instrument;
                        const double rate = 2.0 * hostRate;
                        instrument.prepare(rate, hostRate);
                        instrument.noteOn(test.model, static_cast<float>(note), velocity, colour,
                                          colour == 0.0f ? 0.25f : 4.0f, 321,
                                          colour == 0.0f ? 0.5f : 2.5f);
                        bool valid = true;
                        for (int sample = 0; sample < static_cast<int>(0.06 * rate); ++sample)
                        {
                            const float value = instrument.tick(static_cast<float>(hz(note)), 1.0f - colour);
                            valid = valid && std::isfinite(value) && std::abs(value) < 0.80f;
                        }
                        check(valid, std::string(name(test.model)) + ": register/decay/geometry endpoints retain headroom");
                    }
}

void fixedTouchControls()
{
    for (int model : {organ, harpsichord})
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (int note : {36, 60, 84})
            {
                InstrumentBody soft, firm;
                soft.prepare(rate * 2.0, rate);
                firm.prepare(rate * 2.0, rate);
                soft.noteOn(model, static_cast<float>(note), 0.0f, 0.65f, 1.0f, 123);
                firm.noteOn(model, static_cast<float>(note), 1.0f, 0.65f, 1.0f, 456);
                bool identical = true;
                for (int i = 0; i < static_cast<int>(rate * 0.6); ++i)
                    identical = identical && soft.tick(static_cast<float>(hz(note)), 0.65f)
                                           == firm.tick(static_cast<float>(hz(note)), 0.65f);
                check(identical, std::string(name(model)) + ": velocity and caller seed remain exactly inert");
            }
}
}

int main()
{
    strikeResponse();
    boundaryHeadroom();
    fixedTouchControls();
    std::printf("Struck-body dynamics: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

// A held pluck crosses a stationary body response during pitch motion.
// The oracle uses independent analytic excitation/decay amplitudes, including
// the captured seed and stiffness. A fresh destination pluck is not equivalent.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
constexpr std::uint32_t seed = 0x7812a3u;
constexpr float velocity = .83f, colour = .64f, decay = 1.7f;
constexpr double begin = .17, end = .43;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; if (failures <= 20) std::printf("FAIL: %s\n", message); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }

// Explicit authored profile, independent of production's fixedFormant helper.
double response(int model, double frequency, double size)
{
    const auto peak = [frequency, size](double center, double width)
    {
        const double d = (frequency * size - center) / width;
        return 1.0 / (1.0 + d * d);
    };
    if (model == clav) return .70 + .35 * peak(270, 220) + .18 * peak(1400, 900);
    if (model == harpsichord) return .72 + .30 * peak(300, 210) + .23 * peak(1700, 1000);
    return .65 + .45 * peak(model == bass ? 130 : 210, 130) + .25 * peak(620, 370);
}
double ratio(int model, int harmonic)
{
    const double h = harmonic, b = model == steel ? .000012 : model == harpsichord ? .000025 : 0;
    return h * std::sqrt((1 + b * h * h) / (1 + b));
}
double offset()
{
    std::uint32_t state = seed;
    // First draw controls contact onset; second captures the pluck position.
    for (int i = 0; i < 2; ++i) { state ^= state << 13; state ^= state >> 17; state ^= state << 5; }
    return .006 * (static_cast<double>(state >> 8) * (2.0 / 16777216.0) - 1.0);
}
double sourceWeight(int model, int harmonic)
{
    const double h = harmonic;
    double comb, power, dark, bright;
    if (model == clav)
    {
        comb = .18 + .82 * std::abs(std::sin(pi * h * .235)); power = -.95;
        dark = 4 + 4 * velocity; bright = dark + 10;
    }
    else if (model == harpsichord)
    {
        comb = .10 + .90 * std::abs(std::sin(pi * h * .168)); power = -.72;
        dark = 5; bright = 21;
    }
    else
    {
        const double position = (model == nylon ? .22 : model == bass ? .19 : .135) + offset();
        comb = std::abs(std::sin(pi * h * position)) / std::sin(pi * position);
        power = model == nylon ? -1.75 : model == bass ? -1.6 : -1.32;
        dark = 3 + 5 * velocity; bright = dark + 12;
    }
    const double a = std::exp(-(h - 1) / dark), b = std::exp(-(h - 1) / bright);
    return comb * std::pow(h, power) * (a + colour * (b - a));
}
double loss(int model, int harmonic, double onsetHz)
{
    const double h = harmonic - 1;
    const double reg = std::clamp(std::pow(onsetHz / 261.625565, .35), .55, 2.4);
    double rate = model == clav ? .30 + .46 * h + .045 * h * h
                : model == harpsichord ? .18 + .22 * h + .025 * h * h
                : model == nylon ? .38 + .38 * h + .048 * h * h
                : model == steel ? .16 + .21 * h + .027 * h * h
                : .24 + .46 * h + .060 * h * h;
    return rate * reg / (harmonic == 1 ? std::pow(decay, .35) : decay);
}
// Closed-form Hann-window mean of the captured exponential decay. The tiny
// discrete-window difference is covered by the spectral measurement tolerance.
double envelopeMean(double rate)
{
    const double duration = end - begin, w = 2 * pi / duration;
    return std::exp(-rate * begin) * -std::expm1(-rate * duration) * w * w
         / (duration * rate * (rate * rate + w * w));
}
double expected(int model, int harmonic, double from, double to, float size, bool fixed)
{
    const double original = hz(from), sounding = static_cast<float>(hz(to));
    double amplitude = sourceWeight(model, harmonic) * response(model, original * harmonic, size)
                     * envelopeMean(loss(model, harmonic, original));
    if (fixed && from != to)
        amplitude *= response(model, sounding * ratio(model, harmonic), size)
                   / response(model, original * ratio(model, harmonic), size);
    return amplitude;
}
struct Audio
{
    double rate;
    std::vector<float> samples;
};
Audio render(int model, double from, double to, float size, double hostRate, bool returnHome = false)
{
    Audio audio {2 * hostRate, {}};
    audio.samples.resize(static_cast<std::size_t>(std::lround(.45 * audio.rate)));
    InstrumentBody body;
    body.prepare(audio.rate, hostRate);
    body.noteOn(model, static_cast<float>(from), velocity, colour, decay, seed, size);
    double peak = 0;
    bool finite = true;
    for (std::size_t i = 0; i < audio.samples.size(); ++i)
    {
        const double time = i / audio.rate;
        const double bend = std::clamp((time - .025) / .035, 0.0, 1.0)
                          * (returnHome ? 1.0 - std::clamp((time - .09) / .035, 0.0, 1.0) : 1.0);
        const float x = body.tick(static_cast<float>(hz(from + (to - from) * bend)), colour);
        audio.samples[i] = x;
        finite = finite && std::isfinite(x); peak = std::max(peak, std::abs(static_cast<double>(x)));
    }
    check(finite && peak < .75, "string bend stays finite inside modal plus contact headroom");
    return audio;
}
struct Spectrum
{
    double rate, weightSum = 0;
    std::vector<double> window;
    explicit Spectrum(const Audio& audio) : rate(audio.rate)
    {
        const int first = static_cast<int>(std::lround(begin * rate));
        const int last = static_cast<int>(std::lround(end * rate));
        window.resize(static_cast<std::size_t>(last - first));
        for (int i = first; i < last; ++i)
        {
            const double w = .5 - .5 * std::cos(2 * pi * (i - first) / (last - first - 1));
            weightSum += w;
            window[static_cast<std::size_t>(i - first)] = audio.samples[static_cast<std::size_t>(i)] * w;
        }
    }
    double amplitude(double frequency) const
    {
        const double c = std::cos(2 * pi * frequency / rate), s = std::sin(2 * pi * frequency / rate);
        double r = 1, im = 0, realSum = 0, imaginarySum = 0;
        for (double x : window)
        {
            realSum += x * r; imaginarySum += x * im;
            const double next = r * c - im * s; im = im * c + r * s; r = next;
        }
        return 2 * std::hypot(realSum, imaginarySum) / weightSum;
    }
};

void analyticTracking()
{
    double worstFixed = 0, largestMovingError = 0, squaredFixed = 0, squaredMoving = 0;
    int measured = 0, meaningfulBends = 0;
    for (double hostRate : {44100.0, 48000.0, 96000.0})
        for (int model : {clav, harpsichord, nylon, steel, bass})
            for (float size : {.65f, 1.0f, 1.75f})
                for (double from : {45.0, 57.0, 69.0})
                    for (double bend : {-12.0, -2.0, .35, 12.0})
                    {
                        const double to = from + bend, pitch = static_cast<float>(hz(to));
                        const Spectrum spectrum(render(model, from, to, size, hostRate));
                        const double fundamental = spectrum.amplitude(pitch);
                        check(fundamental > .005, "analytic comparison has a measurable fundamental");
                        double localFixed = 0, localMoving = 0;
                        int localMeasured = 0;
                        for (int h : {2, 3, 4, 5, 7, 9})
                        {
                            const double wanted = expected(model, h, from, to, size, true)
                                                / expected(model, 1, from, to, size, true);
                            if (wanted < .003 || pitch * ratio(model, h) >= .38 * hostRate) continue;
                            const double moving = expected(model, h, from, to, size, false)
                                                / expected(model, 1, from, to, size, false);
                            const double actual = spectrum.amplitude(pitch * ratio(model, h)) / fundamental;
                            const double fixedError = std::abs(db(actual / wanted));
                            const double movingError = std::abs(db(actual / moving));
                            worstFixed = std::max(worstFixed, fixedError);
                            largestMovingError = std::max(largestMovingError, movingError);
                            squaredFixed += fixedError * fixedError; squaredMoving += movingError * movingError;
                            localFixed += fixedError * fixedError; localMoving += movingError * movingError;
                            check(fixedError < .10, "bent spectrum matches fixed-Hz response with original pluck and loss");
                            ++measured; ++localMeasured;
                        }
                        check(localMeasured >= 3, "each register, size and bend measures several partials");
                        if (std::abs(bend) >= 2)
                        {
                            check(localFixed < .04 * localMoving,
                                  "fixed body prediction improves at least fivefold over transposed-body prediction");
                            ++meaningfulBends;
                        }
                    }
    std::printf("STRING fixed-body worst %.6f dB, RMS %.6f dB; moving-body worst %.6f dB, RMS %.6f dB; %d partials, %d meaningful bends\n",
                worstFixed, std::sqrt(squaredFixed / measured), largestMovingError,
                std::sqrt(squaredMoving / measured), measured, meaningfulBends);
}

void returnAndLifecycle()
{
    double worst = 0;
    for (double hostRate : {44100.0, 48000.0, 96000.0})
        for (int model : {clav, harpsichord, nylon, steel, bass})
            for (float size : {.5f, 2.5f})
            {
                const Spectrum returned(render(model, 57, 69, size, hostRate, true));
                const Spectrum steady(render(model, 57, 57, size, hostRate));
                const double pitch = static_cast<float>(hz(57));
                for (int h : {2, 3, 4, 5, 7})
                {
                    const double a = returned.amplitude(pitch * ratio(model, h)) / returned.amplitude(pitch);
                    const double b = steady.amplitude(pitch * ratio(model, h)) / steady.amplitude(pitch);
                    if (b < .003) continue;
                    const double error = std::abs(db(a / b)); worst = std::max(worst, error);
                    check(error < .10, "returning restores original spectrum without restarting decay or onset");
                }
                InstrumentBody used, fresh;
                used.prepare(2 * hostRate, hostRate); fresh.prepare(2 * hostRate, hostRate);
                used.noteOn(steel, 40, .9f, .8f, 2, 177, 1, standard, banjo);
                for (int i = 0; i < 3000; ++i) (void) used.tick(400, .8f);
                used.noteOn(model, 57, velocity, colour, decay, seed, size);
                fresh.noteOn(model, 57, velocity, colour, decay, seed, size);
                bool equal = true;
                for (int i = 0; i < 12000; ++i)
                {
                    const float f = static_cast<float>(hz(57 + 4 * std::sin(i * .001)));
                    equal = (used.tick(f, colour) == fresh.tick(f, colour)) && equal;
                }
                check(equal, "a new note resets response history and repeats captured seed exactly");
            }
    std::printf("STRING return-to-onset worst harmonic error %.6f dB\n", worst);
}

void extremeMotion()
{
    for (double hostRate : {44100.0, 48000.0, 96000.0})
        for (int model : {clav, harpsichord, nylon, steel, bass})
            for (float size : {.5f, 2.5f})
            {
                InstrumentBody body;
                body.prepare(2 * hostRate, hostRate);
                body.noteOn(model, 96, 1, 1, 4, seed, size);
                bool finite = true; double peak = 0;
                for (int i = 0; i < static_cast<int>(hostRate); ++i)
                {
                    const double t = i / (2 * hostRate);
                    const double note = 60 + 60 * std::sin(2 * pi * 7 * t);
                    const float x = body.tick(static_cast<float>(hz(note)), (i / 16) % 2 ? 0.0f : 1.0f);
                    finite = finite && std::isfinite(x); peak = std::max(peak, std::abs(static_cast<double>(x)));
                }
                check(finite && peak < .75, "wide ribbon motion and changing colour retain the absolute-mode-sum ceiling");
            }
}
}

int main()
{
    analyticTracking(); returnAndLifecycle(); extremeMotion();
    std::printf("String body formants: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

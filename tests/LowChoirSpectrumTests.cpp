// PCM checks for low-register choir coverage. The vowel profiles remain
// authored source/filter colours, not measurements of human singers.
#include "core/InstrumentBody.h"

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
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
constexpr std::uint32_t seed = 0x237165eu;
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
struct Audio { double rate; std::vector<float> samples; };
Audio render(int variant, double from, double to, float size = 1.0f,
             double host = 48000, int multiple = 2, double seconds = 1.5,
             float velocity = .85f, int model = organ)
{
    Audio a {host * multiple, {}};
    a.samples.resize(static_cast<std::size_t>(std::lround(seconds * a.rate)));
    InstrumentBody body;
    body.prepare(a.rate, host);
    body.noteOn(model, static_cast<float>(from), velocity, 1, 1, seed, size, variant);
    double peak = 0;
    bool finite = true;
    for (std::size_t i = 0; i < a.samples.size(); ++i)
    {
        const double slide = std::clamp((i / a.rate - .15) / .20, 0.0, 1.0);
        const float x = body.tick(static_cast<float>(hz(from + (to - from) * slide)), 1);
        a.samples[i] = x;
        peak = std::max(peak, std::abs(static_cast<double>(x)));
        finite = finite && std::isfinite(x);
    }
    check(finite && peak < .76, "choir PCM stays finite inside modal and air headroom");
    return a;
}
struct Spectrum
{
    double rate, weights = 0;
    std::vector<double> window;
    explicit Spectrum(const Audio& a) : rate(a.rate)
    {
        const int first = static_cast<int>(std::lround(.8 * rate));
        const int last = static_cast<int>(std::lround(1.5 * rate));
        window.resize(static_cast<std::size_t>(last - first));
        for (int i = first; i < last; ++i)
        {
            const double w = .5 - .5 * std::cos(2 * pi * (i - first) / (last - first - 1));
            weights += w;
            window[static_cast<std::size_t>(i - first)] = a.samples[static_cast<std::size_t>(i)] * w;
        }
    }
    double tone(double frequency) const
    {
        const double c = std::cos(2 * pi * frequency / rate), s = std::sin(2 * pi * frequency / rate);
        double r = 1, im = 0, realSum = 0, imaginarySum = 0;
        for (double x : window)
        {
            realSum += x * r; imaginarySum += x * im;
            const double next = r * c - im * s; im = im * c + r * s; r = next;
        }
        return 2 * std::hypot(realSum, imaginarySum) / weights;
    }
};

void upperCoverage()
{
    // These groups were absent under the previous 24-mode source. A tone at
    // the harmonic must exceed both the fundamental-relative audibility
    // floor and adjacent, nonharmonic breath energy; broadband noise fails.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int variant : {choirAh, choirOo})
            for (int note : {36, 40, 48})
            {
                const Spectrum s(render(variant, note, note, 1, rate));
                const double pitch = static_cast<float>(hz(note));
                double group = 0, floor = 0;
                const auto partials = note == 48 ? std::array<int, 3>{25, 28, 31}
                                                 : std::array<int, 3>{31, 37, 43};
                for (int h : partials)
                {
                    const double amplitude = s.tone(h * pitch);
                    group += amplitude * amplitude;
                    const double off = s.tone((h + .5) * pitch);
                    floor += off * off;
                }
                const double contrast = db(std::sqrt(group / floor));
                const double relative = db(std::sqrt(group) / s.tone(pitch));
                check(contrast > 25, "new upper harmonic group exceeds nearby breath floor by 25 dB");
                check(relative > -42, "new upper harmonic group remains audible relative to fundamental");
                check(s.tone(49 * pitch) < .001 * s.tone(pitch),
                      "no 49th choir mode is admitted beyond the bounded bank");
                std::printf("COVERAGE %s note=%d rate=%.0f group/floor=%+.3f dB group/h1=%+.3f dB\n",
                            variantName(variant), note, rate, contrast, relative);
            }
}

void vowelAndSize()
{
    for (float size : {.75f, 1.0f, 1.5f, 2.0f})
    {
        const double note = 40, pitch = static_cast<float>(hz(note));
        const Spectrum ah(render(choirAh, note, note, size));
        const Spectrum oo(render(choirOo, note, note, size));
        const int low = static_cast<int>(std::lround(330 / (size * pitch)));
        const int middle = static_cast<int>(std::lround(760 / (size * pitch)));
        const double vowelContrast = db((ah.tone(middle * pitch) / ah.tone(low * pitch))
                                     / (oo.tone(middle * pitch) / oo.tone(low * pitch)));
        check(vowelContrast > 15, "Ah and Oo keep distinct low vowel regions across body sizes");
        const int upper = std::min(43, static_cast<int>(std::lround(2800 / (size * pitch))));
        const double added = ah.tone(upper * pitch);
        check(added > 20 * ah.tone((upper + .5) * pitch),
              "size-adaptive budget supplies the upper Ah region below the bank ceiling");
        std::printf("VOWEL size=%.2f Ah/Oo contrast=%+.3f dB upper_h=%d\n", size, vowelContrast, upper);
    }
}

void bendTracking()
{
    double worst = 0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int variant : {choirAh, choirOo})
            for (float size : {.8f, 1.0f, 1.4f})
                for (const auto notes : {std::array<double, 2>{36, 40}, {40, 36}, {42, 41.6}})
                {
                    const Spectrum moved(render(variant, notes[0], notes[1], size, rate));
                    const Spectrum direct(render(variant, notes[1], notes[1], size, rate));
                    const double pitch = static_cast<float>(hz(notes[1]));
                    const double a1 = moved.tone(pitch), b1 = direct.tone(pitch);
                    // Only compare modes fully represented at both onsets.
                    // A bend retains its captured bank; it never adds modes.
                    const double coverage = std::min({48.0, 4400 / (size * hz(notes[0])),
                                                           4400 / (size * hz(notes[1]))});
                    int compared = 0;
                    for (int h : {25, 28, 31, 34, 37, 40, 43, 46})
                    {
                        if (h > coverage) continue;
                        const double actual = moved.tone(h * pitch) / a1;
                        const double expected = direct.tone(h * pitch) / b1;
                        if (expected < .004) continue;
                        const double error = std::abs(db(actual / expected));
                        worst = std::max(worst, error);
                        check(error < .65, "added harmonics follow fixed-Hz tract during bends");
                        ++compared;
                    }
                    check(compared >= 2, "each bend verifies multiple added harmonics");
                }
    std::printf("BEND worst normalized upper-harmonic error %.6f dB\n", worst);
}

double relativeDifference(const Audio& a, const Audio& b)
{
    double error = 0, energy = 0;
    for (std::size_t i = 0; i < a.samples.size(); ++i)
    {
        const double difference = a.samples[i] - b.samples[i];
        error += difference * difference;
        energy += static_cast<double>(a.samples[i]) * a.samples[i];
    }
    return std::sqrt(error / energy);
}
void continuity()
{
    double worst = 0;
    for (int variant : {choirAh, choirOo})
        for (int boundary : {24, 25, 32, 40, 47, 48})
        {
            const double size = 4400 / (boundary * hz(48));
            const auto across = [&](double epsilon)
            {
                const auto below = render(variant, 48, 48, static_cast<float>(size * (1 - epsilon)),
                                          48000, 2, .3);
                const auto above = render(variant, 48, 48, static_cast<float>(size * (1 + epsilon)),
                                          48000, 2, .3);
                return relativeDifference(below, above);
            };
            const double fine = across(1.e-6), coarse = across(1.e-4);
            worst = std::max(worst, fine);
            check(fine < 2.e-4, "fractional mode boundary preserves waveform and breath continuity");
            check(fine < .08 * coarse, "budget-boundary PCM difference converges with parameter distance");
        }
    std::printf("CONTINUITY worst relative PCM difference %.9g at size +/-1ppm\n", worst);
}

void boundsAndRepeatability()
{
    for (double host : {8000.0, 44100.0, 48000.0, 96000.0})
        for (int multiple : {1, 2, 4})
            for (int variant : {choirAh, choirOo})
                for (int note : {0, 24, 36, 48, 60, 96, 127})
                    for (float size : {.5f, 2.5f})
                        for (float velocity : {0.0f, 1.0f})
                            (void) render(variant, note, std::min(note + 24, 127), size,
                                          host, multiple, .39, velocity);
    for (int variant : {choirAh, choirOo})
    {
        const auto a = render(variant, 36, 48, .8f, 48000, 2, .5);
        const auto b = render(variant, 36, 48, .8f, 48000, 2, .5);
        check(a.samples == b.samples, "expanded bank repeats exactly with the same seed and controls");
    }
}

void fingerprints()
{
    for (int model = piano; model <= maxModel; ++model)
        for (int variant : {standard, choirAh, choirOo})
        {
            if (variant != standard && model != organ) continue;
            const auto a = render(variant, 60, 60, 1, 48000, 2, .5, .85f, model);
            std::uint64_t hash = 1469598103934665603ull;
            for (float x : a.samples)
            {
                hash ^= std::bit_cast<std::uint32_t>(x);
                hash *= 1099511628211ull;
            }
            std::printf("PCM model=%d variant=%d hash=%016llx\n", model, variant,
                        static_cast<unsigned long long>(hash));
        }
}
void benchmark()
{
    for (int note : {36, 48, 60})
    {
        std::array<double, 5> elapsed {};
        double receipt = 0;
        for (double& result : elapsed)
        {
            InstrumentBody body;
            body.prepare(96000, 48000);
            body.noteOn(organ, note, .85f, 1, 1, seed, 1, choirAh);
            const float frequency = static_cast<float>(hz(note));
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < 384000; ++i) receipt += body.tick(frequency, 1);
            result = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count()
                   / 384000;
        }
        std::sort(elapsed.begin(), elapsed.end());
        std::printf("BENCH note=%d median_ns/sample=%.3f receipt=%.9g\n", note, elapsed[2], receipt);
    }
}
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--fingerprints") == 0) { fingerprints(); return failures ? 1 : 0; }
    if (argc == 2 && std::strcmp(argv[1], "--benchmark") == 0) { benchmark(); return 0; }
    upperCoverage(); vowelAndSize(); bendTracking(); continuity(); boundsAndRepeatability();
    std::printf("LowChoirSpectrumTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

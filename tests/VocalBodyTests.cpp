// Rendered vowel identity and fixed-tract behavior for the optional choir
// variants. These are authored source/filter voices, not sampled singers.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
struct Audio
{
    double rate;
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const int first = (int) std::lround(begin * rate), last = (int) std::lround(end * rate);
        double sum = 0.0;
        for (int i = first; i < last; ++i) sum += (double) samples[(size_t) i] * samples[(size_t) i];
        return std::sqrt(sum / (last - first));
    }
};
Audio render(int variant, double from, double to, double hostRate = 48000.0,
             float size = 1.0f, float velocity = .85f, float colour = .5f,
             float pressure = 0.0f, double seconds = 1.55)
{
    Audio a {2.0 * hostRate, {}};
    a.samples.resize((size_t) std::lround(seconds * a.rate));
    InstrumentBody body;
    body.prepare(a.rate, hostRate);
    body.noteOn(organ, (float) from, velocity, colour, 1.0f, 0x237165eu, size, variant);
    double peak = 0.0; bool finite = true;
    for (size_t i = 0; i < a.samples.size(); ++i)
    {
        const double time = i / a.rate;
        const double slide = std::clamp((time - .25) / .35, 0.0, 1.0);
        const float pitch = (float) hz(from + (to - from) * slide);
        const float value = body.tick(pitch, colour, time > .25 ? pressure : 0.0f);
        a.samples[i] = value;
        finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value));
    }
    check(finite && peak < .76, "vocal excitation stays finite and inside the mode-sum bound");
    return a;
}
struct Spectrum
{
    double rate, weights = 0.0;
    std::vector<double> window;
    Spectrum(const Audio& a, double begin = .90, double end = 1.50) : rate(a.rate)
    {
        const int first = (int) std::lround(begin * rate), last = (int) std::lround(end * rate);
        window.resize((size_t) (last - first));
        for (int i = first; i < last; ++i)
        {
            const double w = .5 - .5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
            weights += w; window[(size_t) (i - first)] = a.samples[(size_t) i] * w;
        }
    }
    double tone(double frequency) const
    {
        const double angle = 2.0 * pi * frequency / rate;
        const double c = std::cos(angle), s = std::sin(angle);
        double r = 1.0, im = 0.0, realSum = 0.0, imaginarySum = 0.0;
        for (double value : window)
        {
            realSum += r * value; imaginarySum += im * value;
            const double next = r * c - im * s; im = im * c + r * s; r = next;
        }
        return 2.0 * std::hypot(realSum, imaginarySum) / weights;
    }
};

void vowels()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        // Dense A2 harmonics let the test resolve vowel peaks independently
        // of the fundamental. A plain organ plus a low-pass is a deliberately
        // deficient control: gain alone cannot create those peaks.
        const auto ah = render(choirAh, 45, 45, rate), oo = render(choirOo, 45, 45, rate);
        auto filtered = render(standard, 45, 45, rate);
        const double coefficient = 1.0 - std::exp(-2.0 * pi * 1300.0 / filtered.rate);
        double first = 0.0, second = 0.0;
        for (auto& value : filtered.samples)
        {
            first += coefficient * (value - first); second += coefficient * (first - second);
            value = (float) second;
        }
        const Spectrum a(ah), o(oo), control(filtered);
        const double ahCrest = db(a.tone(770.0) / a.tone(440.0));
        const double ooCrest = db(o.tone(330.0) / o.tone(110.0));
        check(ahCrest > 6.0, "ah has an audible first vowel crest above the lower valley");
        check(ooCrest > 4.0, "oo places its strong first vowel crest in the low register");
        check(control.tone(770.0) < .001 * control.tone(440.0),
              "low-pass organ negative control cannot supply the ah vowel crest");
        check(control.tone(330.0) < .5 * control.tone(110.0),
              "low-pass organ negative control cannot supply the oo vowel crest");
        check(a.tone(1210.0) > 4.0 * a.tone(1760.0),
              "ah retains an audible second vowel region above the first crest");
        check(o.tone(880.0) > 1.4 * o.tone(550.0),
              "oo has a separate second vowel region rather than a single low-pass tilt");
        std::printf("VOWEL rate=%.0f ah_770_over_440=%+.3f dB oo_330_over_110=%+.3f dB\n",
                    rate, ahCrest, ooCrest);
        for (int note : {57, 69})
        {
            const auto highAh = render(choirAh, note, note, rate);
            const auto highOo = render(choirOo, note, note, rate);
            const Spectrum aHigh(highAh), oHigh(highOo);
            const int partial = note == 57 ? 3 : 2;
            const double pitch = hz(note);
            const double contrast = db((aHigh.tone(partial * pitch) / aHigh.tone(pitch))
                                     / (oHigh.tone(partial * pitch) / oHigh.tone(pitch)));
            check(contrast > 6.0, "ah and oo retain distinct relative spectra across singing registers");
        }
    }
}

void fixedTract()
{
    double worst = 0.0;
    for (int variant : {choirAh, choirOo})
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (float size : {.8f, 1.0f, 1.4f})
                for (const auto pair : {std::array<double,2>{45,57}, {57,45}, {60,60.4}, {60,59.6}})
                {
                    const auto moved = render(variant, pair[0], pair[1], rate, size);
                    const auto direct = render(variant, pair[1], pair[1], rate, size);
                    const Spectrum a(moved), b(direct);
                    const double pitch = hz(pair[1]);
                    const double ar = a.tone(pitch), br = b.tone(pitch);
                    check(ar > 1.e-4 && br > 1.e-4, "formant tracking compares audible voiced captures");
                    int compared = 0;
                    for (int harmonic : {2,3,4,5,7,9,11})
                    {
                        const double target = b.tone(harmonic * pitch) / br;
                        if (target < .02) continue;
                        const double actual = a.tone(harmonic * pitch) / ar;
                        const double error = std::abs(db(actual / target));
                        worst = std::max(worst, error);
                        check(error < .20, "pitch changes keep the target vowel's harmonic ratios instead of transposing its tract");
                        ++compared;
                    }
                    check(compared >= 2, "vowel tracking includes several measurable partials");
                }
    std::printf("VOWEL tracking worst normalized harmonic error %.6f dB\n", worst);
}

void effortAndAir()
{
    for (int variant : {choirAh, choirOo})
    {
        const auto soft = render(variant, 57, 57, 48000, 1.0f, .2f);
        const auto firm = render(variant, 57, 57, 48000, 1.0f, 1.0f);
        const double onsetSoft = soft.rms(.0,.025) / soft.rms(.30,.55);
        const double onsetFirm = firm.rms(.0,.025) / firm.rms(.30,.55);
        check(onsetFirm > 1.25 * onsetSoft, "firmer voiced effort develops earlier after level normalization");
        const Spectrum s(soft), f(firm);
        const double velocityColour = db((f.tone(1760.0)/f.tone(220.0)) / (s.tone(1760.0)/s.tone(220.0)));
        check(velocityColour > 1.0, "voiced effort changes glottal spectral tilt independently of overall gain");
        const auto pressed = render(variant, 57, 57, 48000, 1.0f, .85f, .4f, 1.0f);
        const auto plain = render(variant, 57, 57, 48000, 1.0f, .85f, .4f, 0.0f);
        const Spectrum p(pressed), zero(plain);
        const double pressureColour = db((p.tone(1760.0)/p.tone(220.0)) / (zero.tone(1760.0)/zero.tone(220.0)));
        check(pressureColour > 2.0, "routed pressure opens the held glottal spectrum");
        const double center = p.tone(220.0);
        check(center > 8.0 * p.tone(217.0) && center > 8.0 * p.tone(223.0),
              "effort and pressure preserve the played pitch without a new vibrato source");
        std::printf("VOWEL %s onset ratio firm/soft %.4f; velocity tilt %+.3f dB; pressure tilt %+.3f dB\n",
                    variantName(variant), onsetFirm/onsetSoft, velocityColour, pressureColour);
    }
}

void boundaries()
{
    for (int variant : {choirAh, choirOo})
        for (double rate : {44100.0,48000.0,96000.0})
            for (float size : {.5f,2.5f})
                for (int note : {0,24,48,72,96,127})
                    for (float velocity : {0.0f,1.0f})
                        (void) render(variant, note, std::clamp(note+24,0,127), rate, size, velocity, 1.0f, 1.0f, .7);
    for (int variant : {choirAh, choirOo})
    {
        const auto a=render(variant,108,108,44100);
        const Spectrum s(a);
        const double pitch=hz(108);
        check(s.tone(pitch*6.0) < .00015*s.tone(pitch),
              "vocal partials above host Nyquist are suppressed at the oversampled rate");
        InstrumentBody body; body.prepare(96000,48000);
        body.noteOn(organ,60,.8f,.5f,1,37,1,variant);
        check(body.tick(std::numeric_limits<float>::quiet_NaN(),.5f)==0.0f
                  && body.tick(-1,.5f)==0.0f,
              "invalid pitch inputs remain silent in the vocal variants");
    }
}
}
int main()
{
    vowels(); fixedTract(); effortAndAir(); boundaries();
    std::printf("VocalBodyTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

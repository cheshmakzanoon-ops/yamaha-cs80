// Rendered material identity, dynamics and compatibility checks. These
// establish distinct authored resonances, not equivalence to recordings.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double value) { return 20.0 * std::log10(std::max(value, 1.0e-15)); }

struct Audio
{
    double rate;
    std::vector<float> samples;
    double rms(double begin, double end) const
    {
        const int first = static_cast<int>(std::lround(begin * rate));
        const int last = std::min(static_cast<int>(samples.size()), static_cast<int>(std::lround(end * rate)));
        double sum = 0.0;
        for (int i = first; i < last; ++i)
            sum += static_cast<double>(samples[static_cast<std::size_t>(i)]) * samples[static_cast<std::size_t>(i)];
        return last > first ? std::sqrt(sum / (last - first)) : 0.0;
    }
};

Audio render(int model, int variant, int note = 60, float velocity = 0.85f,
             float colour = 0.65f, double hostRate = 48000.0,
             double seconds = 1.05, float decay = 1.0f, int factor = 2)
{
    Audio audio {hostRate * factor, {}};
    audio.samples.resize(static_cast<std::size_t>(std::lround(seconds * audio.rate)));
    InstrumentBody body;
    body.prepare(audio.rate, hostRate);
    body.noteOn(model, static_cast<float>(note), velocity, colour, decay, 0x8123ab7u, 1.0f, variant);
    bool finite = true;
    double peak = 0.0;
    for (auto& sample : audio.samples)
    {
        sample = body.tick(static_cast<float>(hz(note)), colour);
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite && peak < 0.80, std::string(variantName(variant)) + ": finite excitation with headroom");
    if (model != off) check(peak > 1.0e-6, "enabled material remains audible");
    return audio;
}

// An arbitrary-frequency, Hann-windowed DFT measures the actual samples.
// Relative partial ratios remove common gain and therefore cannot pass just
// because a material or a harder strike was made louder.
struct Spectrum
{
    double rate, weightSum = 0.0;
    std::vector<double> samples;
    Spectrum(const Audio& audio, double begin, double end) : rate(audio.rate)
    {
        const int first = static_cast<int>(std::lround(begin * rate));
        const int last = std::min(static_cast<int>(audio.samples.size()), static_cast<int>(std::lround(end * rate)));
        const int count = last - first;
        samples.resize(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * i / std::max(1, count - 1));
            samples[static_cast<std::size_t>(i)] = weight * audio.samples[static_cast<std::size_t>(first + i)];
            weightSum += weight;
        }
    }
    double tone(double frequency) const
    {
        const double angle = 2.0 * pi * frequency / rate;
        const double cosine = std::cos(angle), sine = std::sin(angle);
        double real = 1.0, imaginary = 0.0, sumReal = 0.0, sumImaginary = 0.0;
        for (double sample : samples)
        {
            sumReal += sample * real;
            sumImaginary += sample * imaginary;
            const double nextReal = real * cosine - imaginary * sine;
            imaginary = imaginary * cosine + real * sine;
            real = nextReal;
        }
        return 2.0 * std::hypot(sumReal, sumImaginary) / weightSum;
    }
    double peakFrequency(double center) const
    {
        double peak = -1.0, result = center;
        for (int step = -16; step <= 16; ++step)
        {
            const double frequency = center + step * 0.25;
            const double amplitude = tone(frequency);
            if (amplitude > peak) { peak = amplitude; result = frequency; }
        }
        return result;
    }
};

void distinctResonances()
{
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int note : {48, 60, 72})
        {
            const double fundamental = hz(note);
            const auto wood = render(marimba, xylophone, note, 0.85f, 0.65f, rate);
            const auto marimbaAudio = render(marimba, standard, note, 0.85f, 0.65f, rate);
            const auto shell = render(vibes, steelpan, note, 0.85f, 0.65f, rate);
            const auto vibesAudio = render(vibes, standard, note, 0.85f, 0.65f, rate);
            const Spectrum x(wood, 0.035, 0.27), m(marimbaAudio, 0.035, 0.27);
            const Spectrum p(shell, 0.035, 0.27), v(vibesAudio, 0.035, 0.27);
            const double xThird = x.tone(3.0 * fundamental), xFourth = x.tone(4.0 * fundamental);
            const double mThird = m.tone(3.0 * fundamental), mFourth = m.tone(4.0 * fundamental);
            check(db(xThird / xFourth) > 25.0, "xylophone has a tuned third, not a marimba fourth");
            check(db(mFourth / mThird) > 25.0, "standard marimba retains its tuned fourth");
            check(xThird / x.tone(fundamental) > 0.07, "xylophone third is an audible material feature");
            const double pSecond = p.tone(2.0 * fundamental) / p.tone(fundamental);
            const double pThird = p.tone(3.0 * fundamental) / p.tone(fundamental);
            check(pSecond > 0.08 && pThird > 0.04, "steelpan has audible octave and twelfth shell modes");
            check(pSecond > 20.0 * v.tone(2.0 * fundamental) / v.tone(fundamental),
                  "steelpan octave is distinct from the vibraphone body at matched fundamental");
            check(pThird > 20.0 * v.tone(3.0 * fundamental) / v.tone(fundamental),
                  "steelpan twelfth is distinct from the vibraphone body at matched fundamental");
            check(std::abs(x.peakFrequency(fundamental) - fundamental) < 0.51
                    && std::abs(p.peakFrequency(fundamental) - fundamental) < 0.51,
                  "changing material preserves the played fundamental");
            check(std::abs(x.peakFrequency(3.0 * fundamental) - 3.0 * fundamental) < 0.51,
                  "independent spectral peak search locates the xylophone tuned third");
            check(std::abs(p.peakFrequency(2.0 * fundamental) - 2.0 * fundamental) < 0.51
                    && std::abs(p.peakFrequency(3.0 * fundamental) - 3.0 * fundamental) < 0.51,
                  "independent spectral peak search locates steelpan octave and twelfth");
            const Spectrum xLate(wood, 0.55, 0.95), pLate(shell, 0.55, 0.95);
            const double xSoftening = db((xLate.tone(3.0 * fundamental) / xLate.tone(fundamental))
                                        / (xThird / x.tone(fundamental)));
            const double pSoftening = db((pLate.tone(3.0 * fundamental) / pLate.tone(fundamental))
                                        / (p.tone(3.0 * fundamental) / p.tone(fundamental)));
            check(xSoftening < -8.0, "xylophone upper knock fades faster than its pitched body");
            check(pSoftening < -4.0, "steelpan shell softens as higher tuned modes lose energy");
            check(wood.rms(0.70, 0.95) / wood.rms(0.04, 0.20)
                    < shell.rms(0.70, 0.95) / shell.rms(0.04, 0.20),
                  "wooden material has a more compact decay than the metal shell");
            std::printf("MATERIAL rate=%.0f note=%d xylo_third_over_fourth=%+.2f dB "
                        "pan_second=%+.2f dB pan_third=%+.2f dB xylo_softening=%+.2f dB pan_softening=%+.2f dB\n",
                        rate, note, db(xThird / xFourth), db(pSecond), db(pThird), xSoftening, pSoftening);
        }
}

void touchAndRates()
{
    for (int variant : {xylophone, steelpan})
    {
        const int model = variant == xylophone ? marimba : vibes;
        for (int note : {36, 60, 84})
            for (float colour : {0.0f, 0.65f, 1.0f})
            {
                double referenceFundamental = 0.0, referenceUpper = 0.0;
                for (double rate : {48000.0, 44100.0, 96000.0})
                {
                    const auto soft = render(model, variant, note, 0.20f, colour, rate, 0.35);
                    const auto firm = render(model, variant, note, 1.0f, colour, rate, 0.35);
                    const Spectrum a(soft, 0.04, 0.30), b(firm, 0.04, 0.30);
                    const double fundamental = hz(note);
                    const double fundA = a.tone(fundamental), fundB = b.tone(fundamental);
                    const double upperA = a.tone(3.0 * fundamental) / fundA;
                    const double upperB = b.tone(3.0 * fundamental) / fundB;
                    check(std::abs(db(fundB / fundA)) < 0.025,
                          "quieter material excitation receives no normalization boost");
                    check(db(firm.rms(0.04, 0.30) / soft.rms(0.04, 0.30)) > -0.025,
                          "firmer material excitation does not reduce early RMS");
                    check(db(upperB / upperA) > 6.0,
                          "harder mallet changes spectral balance independently of level");
                    if (rate == 48000.0) { referenceFundamental = fundB; referenceUpper = upperB; }
                    else
                    {
                        check(std::abs(db(fundB / referenceFundamental)) < 0.035,
                              "fundamental energy remains consistent across host sample rates");
                        check(std::abs(db(upperB / referenceUpper)) < 0.05,
                              "material overtone balance remains consistent across host sample rates");
                    }
                }
            }
    }
}

void defaultsAndCompatibility()
{
    for (int model = off; model <= maxModel; ++model)
        for (int variant : std::array<int, 6> {-1, standard, xylophone, steelpan, 3, 99})
        {
            if (compatibleVariant(model, variant) != standard) continue;
            InstrumentBody before, after;
            before.prepare(96000.0, 48000.0);
            after.prepare(96000.0, 48000.0);
            before.noteOn(model, 60.0f, 0.62f, 0.43f, 1.3f, 711, 1.1f);
            after.noteOn(model, 60.0f, 0.62f, 0.43f, 1.3f, 711, 1.1f, variant);
            bool identical = true;
            for (int i = 0; i < 12000; ++i)
            {
                const float frequency = static_cast<float>(hz(60.0) * (1.0 + 0.002 * std::sin(i * 0.0003)));
                const float colour = i < 6000 ? 0.43f : 0.8f;
                const float a = before.tick(frequency, colour, 0.35f);
                const float b = after.tick(frequency, colour, 0.35f);
                identical = identical && a == b;
            }
            check(identical, "standard/default/invalid/incompatible material leaves exact samples unchanged");
        }
    // Reusing a generator must clear a previously selected material.
    InstrumentBody reused, fresh;
    reused.prepare(96000.0, 48000.0);
    fresh.prepare(96000.0, 48000.0);
    reused.noteOn(marimba, 60.0f, 0.8f, 0.65f, 1.0f, 711, 1.0f, xylophone);
    for (int i = 0; i < 1000; ++i) (void) reused.tick(261.625565f, 0.65f);
    reused.noteOn(marimba, 60.0f, 0.8f, 0.65f, 1.0f, 711);
    fresh.noteOn(marimba, 60.0f, 0.8f, 0.65f, 1.0f, 711);
    bool identical = true;
    for (int i = 0; i < 12000; ++i)
        identical = (reused.tick(261.625565f, 0.65f) == fresh.tick(261.625565f, 0.65f)) && identical;
    check(identical, "new standard strike clears any previously selected material");
}

void endpointHeadroom()
{
    for (int variant : {xylophone, steelpan})
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (int note : {0, 24, 96, 127})
                for (float velocity : {0.0f, 1.0f})
                    for (float colour : {0.0f, 1.0f})
                        for (int factor : {1, 2})
                            (void) render(variant == xylophone ? marimba : vibes, variant, note,
                                          velocity, colour, rate, 0.055,
                                          colour == 0.0f ? 0.25f : 4.0f, factor);
}
}

int main()
{
    distinctResonances();
    touchAndRates();
    defaultsAndCompatibility();
    endpointHeadroom();
    std::printf("Instrument body variants: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

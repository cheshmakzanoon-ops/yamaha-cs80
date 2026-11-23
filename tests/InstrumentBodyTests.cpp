// Audible-behavior contracts for the optional instrument excitation layer.
// These tests measure rendered samples, not private coefficients or factory
// parameter literals. They establish useful musical distinctions and numeric
// safety, not similarity to a recorded physical instrument. The rank VCA owns
// note-off, pedal and the overall amplitude envelope; Engine integration tests
// cover those contracts separately.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace
{
using namespace cs80::body;
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
std::uint64_t generatedSamples = 0;

void check(bool ok, const std::string& message)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
double hz(int note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double magnitude) { return 20.0 * std::log10(std::max(magnitude, 1.0e-15)); }
double relativeDb(double a, double b) { return db(a / std::max(b, 1.0e-15)); }

struct Audio
{
    double rate;
    std::vector<float> samples;
    int at(double seconds) const
    {
        return std::clamp(static_cast<int>(std::llround(seconds * rate)),
                          0, static_cast<int>(samples.size()));
    }
    double rms(double begin, double end) const
    {
        const int first = at(begin), last = at(end);
        double sum = 0.0;
        for (int i = first; i < last; ++i)
            sum += static_cast<double>(samples[static_cast<std::size_t>(i)])
                 * samples[static_cast<std::size_t>(i)];
        return last > first ? std::sqrt(sum / (last - first)) : 0.0;
    }
};

Audio render(int model, int note, double seconds, float velocity = 0.85f,
             float colour = 0.65f, float decay = 1.0f, double hostRate = 48000.0,
             std::uint32_t seed = 0x7812a3u)
{
    Audio result {hostRate * 2.0, {}};
    result.samples.resize(static_cast<std::size_t>(std::llround(seconds * result.rate)));
    InstrumentBody instrument;
    instrument.prepare(result.rate, hostRate);
    instrument.noteOn(model, static_cast<float>(note), velocity, colour, decay, seed);
    bool finite = true;
    double peak = 0.0;
    for (float& sample : result.samples)
    {
        sample = instrument.tick(static_cast<float>(hz(note)), colour);
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    generatedSamples += result.samples.size();
    check(finite && peak < 4.0, std::string(name(model)) + ": finite, bounded excitation");
    if (model != off) check(peak > 1.0e-4, std::string(name(model)) + ": sounding capture is not silence");
    return result;
}

// A windowed, arbitrary-frequency DFT. This avoids FFT-bin coincidences and
// permits measurement of stretched strings and non-harmonic metal-bar modes.
// Samples and Hann weights are computed once for each measurement window.
struct Spectrum
{
    double rate, weightSum = 0.0;
    std::vector<double> windowed;
    Spectrum(const Audio& audio, double begin, double end) : rate(audio.rate)
    {
        const int first = audio.at(begin), count = audio.at(end) - first;
        windowed.resize(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * i / std::max(1, count - 1));
            weightSum += weight;
            windowed[static_cast<std::size_t>(i)] = weight * audio.samples[static_cast<std::size_t>(first + i)];
        }
    }
    double tone(double frequency) const
    {
        const double angle = -2.0 * pi * frequency / rate;
        const double cosine = std::cos(angle), sine = std::sin(angle);
        double real = 1.0, imaginary = 0.0, sumReal = 0.0, sumImaginary = 0.0;
        for (double sample : windowed)
        {
            sumReal += real * sample;
            sumImaginary += imaginary * sample;
            const double next = real * cosine - imaginary * sine;
            imaginary = imaginary * cosine + real * sine;
            real = next;
        }
        return 2.0 * std::hypot(sumReal, sumImaginary) / std::max(weightSum, 1.0);
    }
    double near(double frequency, double relativeWidth = 0.005) const
    {
        double maximum = 0.0;
        for (int i = -3; i <= 3; ++i)
            maximum = std::max(maximum, tone(frequency * (1.0 + relativeWidth * i / 3.0)));
        return maximum;
    }
    double peakFrequency(double center, double width, double step) const
    {
        double maximum = -1.0, result = center;
        for (double frequency = center - width; frequency <= center + width; frequency += step)
        {
            const double amplitude = tone(frequency);
            if (amplitude > maximum) { maximum = amplitude; result = frequency; }
        }
        return result;
    }
};

void registerAndRateCoverage()
{
    // Common MIDI registers, every body, and both gentle and firm strikes.
    // This also checks that host Nyquist suppression does not erase a high
    // note's fundamental while removing its unusable upper modes.
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int model = piano; model <= celesta; ++model)
            for (int note : {24, 60, 96})
                for (float velocity : {0.2f, 0.9f})
                {
                    const auto audio = render(model, note, 0.12, velocity, 0.65f, 1.0f, rate);
                    check(audio.rms(0.02, 0.12) > 1.0e-4,
                          std::string(name(model)) + ": audible low/middle/high register at each host rate");
                }
}

void spectralDecay()
{
    struct Case { int model; double upper; };
    for (const auto test : {Case {piano, 5.0}, Case {tine, 3.0}, Case {reed, 4.0},
                           Case {nylon, 4.0}, Case {steel, 5.0}, Case {bass, 4.0},
                           Case {vibes, 4.0}, Case {marimba, 4.0}, Case {celesta, 2.76}})
    {
        const auto audio = render(test.model, 60, 1.6);
        const Spectrum early(audio, 0.03, 0.18), late(audio, 1.25, 1.55);
        const double initialFundamental = early.near(hz(60));
        const double finalFundamental = late.near(hz(60));
        const double initialUpper = early.near(test.upper * hz(60));
        const double finalUpper = late.near(test.upper * hz(60));
        const double fall = relativeDb(finalUpper, finalFundamental)
                          - relativeDb(initialUpper, initialFundamental);
        check(relativeDb(initialUpper, initialFundamental) > -55.0,
              std::string(name(test.model)) + ": early overtone is measurable");
        check(relativeDb(finalFundamental, initialFundamental) > -12.0,
              std::string(name(test.model)) + ": base decay remains available to the external VCA");
        check(fall < -6.0, std::string(name(test.model)) + ": upper mode decays independently of fundamental");
        std::printf("DECAY %-20s upper/fundamental change %+7.2f dB\n", name(test.model), fall);
    }
    // A static harmonic registration is a negative control for this test:
    // mere elapsed time must not be reported as spectral damping.
    const auto held = render(organ, 60, 1.6);
    const Spectrum first(held, 0.03, 0.18), last(held, 1.25, 1.55);
    const double stationary = relativeDb(last.tone(4.0 * hz(60)), last.tone(hz(60)))
                            - relativeDb(first.tone(4.0 * hz(60)), first.tone(hz(60)));
    check(std::abs(stationary) < 0.3, "Organ: stationary spectrum rejects a false decay diagnosis");

    const auto shortLoss = render(steel, 60, 1.0, 0.85f, 0.7f, 0.25f);
    const auto longLoss = render(steel, 60, 1.0, 0.85f, 0.7f, 4.0f);
    const Spectrum shortModes(shortLoss, 0.7, 1.0), longModes(longLoss, 0.7, 1.0);
    const double shortRatio = relativeDb(shortModes.near(5.0 * hz(60)), shortModes.near(hz(60)));
    const double longRatio = relativeDb(longModes.near(5.0 * hz(60)), longModes.near(hz(60)));
    check(longRatio - shortRatio > 15.0, "Partial-decay control changes overtone persistence substantially");
    check(std::abs(relativeDb(longModes.near(hz(60)), shortModes.near(hz(60)))) < 3.0,
          "Partial-decay control does not substitute a global volume change");
}

void instrumentIdentity()
{
    for (int note : {48, 60, 72})
    {
        const auto t = render(tine, note, 0.45), r = render(reed, note, 0.45);
        const Spectrum ts(t, 0.04, 0.30), rs(r, 0.04, 0.30);
        const double tineEven = relativeDb(ts.tone(2.0 * hz(note)), ts.tone(hz(note)));
        const double reedEven = relativeDb(rs.tone(2.0 * hz(note)), rs.tone(hz(note)));
        check(reedEven - tineEven > 15.0, "Reed and tine EP have distinct even-harmonic spectra across register");
        check(relativeDb(ts.tone(3.0 * hz(note)), ts.tone(2.0 * hz(note))) > 12.0,
              "Tine's upper pickup harmonic is distinct from the octave");

        const auto clar = render(clarinet, note, 0.45), ob = render(oboe, note, 0.45);
        const Spectrum cs(clar, 0.10, 0.40), os(ob, 0.10, 0.40);
        const double clarEven = relativeDb(cs.tone(2.0 * hz(note)), cs.tone(3.0 * hz(note)));
        const double oboeEven = relativeDb(os.tone(2.0 * hz(note)), os.tone(3.0 * hz(note)));
        check(clarEven < -10.0, "Clarinet retains a predominantly odd-harmonic low-register body");
        check(oboeEven - clarEven > 10.0, "Double reed is spectrally distinct from the clarinet body");
    }

    // Find the acoustic-style piano's spectral peaks independently. The
    // fundamental stays on the played pitch, while a high partial stretches
    // above an exact multiple. A harmonic organ is the zero-stiffness control.
    const auto p = render(piano, 84, 0.40, 0.95f, 0.8f);
    const auto o = render(organ, 84, 0.40, 0.95f, 0.8f);
    const Spectrum ps(p, 0.02, 0.36), os(o, 0.02, 0.36);
    const double fundamental = ps.peakFrequency(hz(84), 5.0, 0.25);
    const double upper = ps.peakFrequency(6.0 * hz(84), 55.0, 0.5);
    const double harmonic = os.peakFrequency(6.0 * hz(84), 55.0, 0.5);
    check(std::abs(1200.0 * std::log2(fundamental / hz(84))) < 4.0,
          "Piano fundamental stays on concert pitch despite unison beating");
    check(upper - 6.0 * hz(84) > 8.0 && upper - 6.0 * hz(84) < 50.0,
          "Piano upper string partial is measurably stretched, without an arbitrary pitch shift");
    check(std::abs(harmonic - 6.0 * hz(84)) < 2.0,
          "Harmonic control rejects a spurious string-stiffness result");
    std::printf("PIANO fundamental %.3f Hz, sixth %.3f Hz, harmonic control %.3f Hz\n",
                fundamental, upper, harmonic);
}

void struckBarsTrackNotes()
{
    struct Case { int model; double ratio; };
    for (const auto test : {Case {bell, 2.756}, Case {vibes, 4.0},
                           Case {marimba, 4.0}, Case {celesta, 2.76}})
        for (int note : {48, 60, 72})
            for (double rate : {44100.0, 48000.0, 96000.0})
            {
                const auto audio = render(test.model, note, 0.34, 0.85f, 0.75f, 1.0f, rate);
                const Spectrum spectrum(audio, 0.02, 0.30);
                const double main = spectrum.near(hz(note));
                const double mode = spectrum.near(test.ratio * hz(note), 0.002);
                const double wrongOctave = spectrum.near(2.0 * hz(note), 0.002);
                check(relativeDb(mode, main) > -40.0,
                      std::string(name(test.model)) + ": upper resonant mode follows the played note and host rate");
                check(relativeDb(mode, wrongOctave) > 22.0,
                      std::string(name(test.model)) + ": mode is not an ordinary octave oscillator");
            }
}

void malletDecayIdentity()
{
    // Both tuned bars contain a fourth partial, so its presence alone cannot
    // distinguish a wooden marimba attack from a ringing metal vibraphone.
    // Compare the measured overtone/fundamental decay across register and
    // touch. A model alias would preserve all shared-mode checks above but
    // must fail this difference in the complete rendered decay behavior.
    for (int note : {48, 60, 72})
        for (float velocity : {0.30f, 0.85f})
        {
            const auto metal = render(vibes, note, 1.1, velocity);
            const auto wood = render(marimba, note, 1.1, velocity);
            const auto upperDecay = [note](const Audio& audio)
            {
                const Spectrum early(audio, 0.03, 0.18), late(audio, 0.80, 1.05);
                return relativeDb(late.near(4.0 * hz(note)), late.near(hz(note)))
                     - relativeDb(early.near(4.0 * hz(note)), early.near(hz(note)));
            };
            const double metalDecay = upperDecay(metal), woodDecay = upperDecay(wood);
            check(metalDecay - woodDecay > 12.0,
                  "Marimba upper mode damps substantially faster than the vibraphone across register and touch");
            std::printf("MALLET note %d velocity %.2f: vibraphone %+.2f dB, marimba %+.2f dB upper/fundamental decay\n",
                        note, velocity, metalDecay, woodDecay);
        }
}

void brightnessAndTouch()
{
    for (int model : {flute, brass, organ, tine})
    {
        const double upper = model == flute ? 3.0 : model == brass ? 6.0 : model == organ ? 8.0 : 3.0;
        const auto dark = render(model, 60, 0.30, 0.8f, 0.05f);
        const auto bright = render(model, 60, 0.30, 0.8f, 0.95f);
        const Spectrum ds(dark, 0.04, 0.28), bs(bright, 0.04, 0.28);
        const double darkRatio = relativeDb(ds.near(upper * hz(60)), ds.near(hz(60)));
        const double brightRatio = relativeDb(bs.near(upper * hz(60)), bs.near(hz(60)));
        check(brightRatio - darkRatio > 4.0, std::string(name(model)) + ": brightness changes spectral balance, not just gain");
    }
    for (int model : {piano, tine})
    {
        const double upper = model == piano ? 10.0 : 3.0;
        const auto soft = render(model, 60, 0.20, 0.08f, 0.55f);
        const auto firm = render(model, 60, 0.20, 1.0f, 0.55f);
        const Spectrum ss(soft, 0.02, 0.18), fs(firm, 0.02, 0.18);
        const double softRatio = relativeDb(ss.near(upper * hz(60)), ss.near(hz(60)));
        const double firmRatio = relativeDb(fs.near(upper * hz(60)), fs.near(hz(60)));
        check(firmRatio - softRatio > 5.0, std::string(name(model)) + ": firm strikes excite more upper partials");
    }
    for (int model : {organ, harpsichord})
    {
        const auto soft = render(model, 60, 0.30, 0.1f, 0.65f, 1.0f, 48000.0, 13);
        const auto firm = render(model, 60, 0.30, 0.95f, 0.65f, 1.0f, 48000.0, 999);
        check(soft.samples == firm.samples,
              std::string(name(model)) + ": key velocity and caller seed cannot create a hammer response");
    }
}

void pitchMotionAndNyquist()
{
    for (double host : {44100.0, 48000.0, 96000.0})
    {
        Audio glide {2.0 * host, {}};
        glide.samples.resize(static_cast<std::size_t>(0.65 * glide.rate));
        InstrumentBody instrument;
        instrument.prepare(glide.rate, host);
        instrument.noteOn(organ, 60.0f, 0.8f, 0.7f, 1.0f, 123);
        bool finite = true;
        for (std::size_t i = 0; i < glide.samples.size(); ++i)
        {
            const double time = i / glide.rate;
            const double semitones = 12.0 * std::clamp((time - 0.15) / 0.20, 0.0, 1.0);
            const float frequency = static_cast<float>(hz(60) * std::exp2(semitones / 12.0));
            glide.samples[i] = instrument.tick(frequency, 0.7f);
            finite = finite && std::isfinite(glide.samples[i]) && std::abs(glide.samples[i]) < 4.0f;
        }
        generatedSamples += glide.samples.size();
        const Spectrum before(glide, 0.03, 0.14), after(glide, 0.40, 0.63);
        check(finite, "Continuous octave bend remains finite at each host rate");
        check(before.tone(hz(60)) > 0.01 && after.tone(2.0 * hz(60)) > 0.01,
              "Body follows the actual frequency input through an octave bend");
        check(relativeDb(after.tone(hz(60)), after.tone(2.0 * hz(60))) < -45.0,
              "Body does not retain the old fundamental after pitch changes");

        Audio high {2.0 * host, {}};
        high.samples.resize(static_cast<std::size_t>(0.14 * high.rate));
        instrument.prepare(high.rate, host);
        instrument.noteOn(organ, 96.0f, 0.8f, 1.0f, 1.0f, 456);
        const double frequency = 0.24 * host;
        for (float& sample : high.samples) sample = instrument.tick(static_cast<float>(frequency), 1.0f);
        generatedSamples += high.samples.size();
        const Spectrum highSpectrum(high, 0.03, 0.13);
        const double fundamental = highSpectrum.tone(frequency);
        check(fundamental > 0.01, "Nyquist guard retains an audible high fundamental");
        check(relativeDb(highSpectrum.tone(2.0 * frequency), fundamental) < -70.0
           && relativeDb(highSpectrum.tone(3.0 * frequency), fundamental) < -70.0,
              "Modes above host Nyquist are removed even at a higher internal sample rate");
        // All organ modes are now above the host passband. Its lack of contact
        // noise makes exact silence a useful rejection control for aliasing.
        for (int i = 0; i < 256; ++i) instrument.tick(static_cast<float>(0.49 * host), 1.0f);
        double rejected = 0.0;
        for (int i = 0; i < 4096; ++i)
            rejected = std::max(rejected, std::abs(static_cast<double>(instrument.tick(static_cast<float>(0.49 * host), 1.0f))));
        check(rejected < 1.0e-12, "Out-of-passband body cannot fold into an audible low pitch");
    }
}

void resetAndLongTermSafety()
{
    InstrumentBody instrument;
    instrument.prepare(96000.0, 48000.0);
    bool offSilent = true;
    for (int i = 0; i < 512; ++i) offSilent = offSilent && instrument.tick(440.0f, 0.5f) == 0.0f;
    check(offSilent && !instrument.enabled(), "Default body is exactly silent");
    instrument.noteOn(piano, 60.0f, 0.9f, 0.7f, 1.0f, 123);
    for (int i = 0; i < 4096; ++i) instrument.tick(261.625565f, 0.7f);
    instrument.reset();
    bool resetSilent = true;
    for (int i = 0; i < 512; ++i) resetSilent = resetSilent && instrument.tick(440.0f, 1.0f) == 0.0f;
    check(resetSilent && !instrument.enabled(), "Reset removes the prior strike and every modal tail");

    const auto first = render(piano, 60, 0.2, 0.8f, 0.6f, 1.0f, 48000.0, 422);
    const auto repeat = render(piano, 60, 0.2, 0.8f, 0.6f, 1.0f, 48000.0, 422);
    check(first.samples == repeat.samples, "Identical private excitation seed reproduces every sample");

    // Thirty seconds at a periodic frequency. Compare matching cycle windows
    // near the beginning/end; this catches recurrence drift which a peak-only
    // check could miss without asserting internal renormalization details.
    instrument.prepare(96000.0, 48000.0);
    instrument.noteOn(organ, 69.0f, 0.8f, 0.7f, 1.0f, 123);
    std::vector<float> initial(76800);
    double referenceEnergy = 0.0, differenceEnergy = 0.0, lastSum = 0.0;
    bool stable = true;
    for (int i = 0; i < 30 * 96000; ++i)
    {
        const float sample = instrument.tick(440.0f, 0.7f);
        stable = stable && std::isfinite(sample) && std::abs(sample) < 4.0f;
        if (i >= 9600 && i < 86400) initial[static_cast<std::size_t>(i - 9600)] = sample;
        if (i >= 29 * 96000 + 9600 && i < 29 * 96000 + 86400)
        {
            const double reference = initial[static_cast<std::size_t>(i - 29 * 96000 - 9600)];
            referenceEnergy += reference * reference;
            differenceEnergy += (sample - reference) * (sample - reference);
            lastSum += sample;
        }
    }
    generatedSamples += 30 * 96000;
    check(stable && referenceEnergy > 1.0, "Thirty-second organ note remains finite and sounding");
    check(differenceEnergy < 1.0e-10 * referenceEnergy,
          "Thirty-second sustain does not accumulate phase or amplitude drift");
    check(std::abs(lastSum / initial.size()) < 1.0e-5, "Long held body has no accumulating DC offset");

    bool safe = true;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (int model = piano; model <= celesta; ++model)
    {
        instrument.prepare(192000.0, 96000.0);
        instrument.noteOn(model, 127.0f, 1.0f, 1.0f, 4.0f, 77);
        for (int i = 0; i < 8192; ++i)
        {
            const float frequency = i < 2048 ? 0.01f : i < 4096 ? 12543.854f : i < 6144 ? 1000000.0f : 440.0f;
            const float sample = instrument.tick(frequency, i & 1 ? nan : infinity, i & 1 ? infinity : nan);
            safe = safe && std::isfinite(sample) && std::abs(sample) < 4.0f;
        }
        safe = safe && instrument.tick(nan, 0.5f) == 0.0f
                    && instrument.tick(infinity, 0.5f) == 0.0f
                    && instrument.tick(-440.0f, 0.5f) == 0.0f;
    }
    check(safe, "Extreme bends and malformed numeric controls cannot create nonfinite audio");
}
} // namespace

int main()
{
    registerAndRateCoverage();
    spectralDecay();
    instrumentIdentity();
    struckBarsTrackNotes();
    malletDecayIdentity();
    brightnessAndTouch();
    pitchMotionAndNyquist();
    resetAndLongTermSafety();
    std::printf("Instrument bodies: %d checks, %d failures; %llu generated samples\n",
                checks, failures, static_cast<unsigned long long>(generatedSamples));
    return failures == 0 ? 0 : 1;
}

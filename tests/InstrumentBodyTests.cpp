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
double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
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

Audio render(int model, double note, double seconds, float velocity = 0.85f,
             float colour = 0.65f, float decay = 1.0f, double hostRate = 48000.0,
             std::uint32_t seed = 0x7812a3u, float bodySize = 1.0f)
{
    Audio result {hostRate * 2.0, {}};
    result.samples.resize(static_cast<std::size_t>(std::llround(seconds * result.rate)));
    InstrumentBody instrument;
    instrument.prepare(result.rate, hostRate);
    instrument.noteOn(model, static_cast<float>(note), velocity, colour, decay, seed, bodySize);
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
        if (note <= 60)
        {
            check(clarEven < -10.0, "Clarinet retains a predominantly odd-harmonic low-register body");
            check(oboeEven - clarEven > 10.0, "Double reed is spectrally distinct from the low clarinet body");
        }
        else
        {
            check(clarEven > -5.0, "Clarinet upper register admits strong even harmonics");
            check(std::abs(oboeEven - clarEven) > 0.5,
                  "Upper clarinet and double reed retain different formant balances");
        }
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

void struckRegisterAndExcitation()
{
    // A lower string/longer bar must not simply replay a transposed copy of
    // one middle-register decay. These tests isolate the body from the VCA.
    for (int model : {tine, reed, nylon, steel, bass, vibes, marimba, celesta})
    {
        double decay[2] {};
        int slot = 0;
        for (int note : {48, 84})
        {
            const auto a = render(model, note, 1.75, 0.8f, 0.6f);
            const Spectrum early(a, 0.05, 0.20), late(a, 1.40, 1.65);
            decay[slot++] = relativeDb(late.near(hz(note)), early.near(hz(note)));
        }
        check(decay[0] - decay[1] > 0.45,
              std::string(name(model)) + ": treble fundamental damps faster than the bass body");
        std::printf("REGISTER DECAY %-20s low %+.3f dB high %+.3f dB\n",
                    name(model), decay[0], decay[1]);
    }

    // Low piano attacks need pitched detail above the old 24-harmonic ceiling.
    // Check the stretched peak against the neighboring off-harmonic frequency
    // so broadband contact noise cannot satisfy the missing-mode regression.
    const auto low = render(piano, 36, 0.18, 0.95f, 0.8f);
    const Spectrum bassAttack(low, 0.02, 0.14);
    const double h = 29.0;
    const double partial = hz(36) * h * std::sqrt((1.0 + 0.000045 * h * h) / 1.000045);
    const double pitched = bassAttack.near(partial, 0.002);
    const double adjacent = bassAttack.near(partial + 0.46 * hz(36), 0.002);
    check(relativeDb(pitched, bassAttack.near(hz(36))) > -47.0,
          "Bass piano has significant pitched upper detail beyond harmonic 24");
    check(relativeDb(pitched, adjacent) > 12.0,
          "Bass piano high detail is a string mode, not just added contact hiss");

    // A prompt fundamental should settle into a slower aftersound. Three
    // equally spaced windows distinguish that from one exponential envelope.
    const auto pianoTail = render(piano, 36, 3.4, 0.85f, 0.6f);
    const Spectrum p0(pianoTail, 0.15, 0.35), p1(pianoTail, 1.65, 1.85), p2(pianoTail, 3.15, 3.35);
    const double prompt = relativeDb(p1.near(hz(36)), p0.near(hz(36)));
    const double aftersound = relativeDb(p2.near(hz(36)), p1.near(hz(36)));
    check(aftersound - prompt > 0.5,
          "Piano prompt decay settles into a slower coupled aftersound");
    std::printf("PIANO TWO-STAGE prompt %+.3f dB, aftersound %+.3f dB\n", prompt, aftersound);

    // Pickup harmonic strength follows shrinking excursion. The upper/f0
    // decay consequently rounds over time instead of remaining a fixed
    // exponential chosen independently for every audible harmonic.
    for (int model : {tine, reed})
    {
        const auto a = render(model, 60, 1.5, 0.95f, 0.65f);
        const Spectrum s0(a, 0.05, 0.15), s1(a, 0.65, 0.75), s2(a, 1.25, 1.35);
        const auto ratio = [](const Spectrum& s) {
            return relativeDb(s.near(3.0 * hz(60)), s.near(hz(60)));
        };
        const double curve = ratio(s2) - 2.0 * ratio(s1) + ratio(s0);
        check(curve > 0.35, std::string(name(model)) + ": pickup bark evolves with decaying excursion");
        std::printf("PICKUP CURVATURE %-20s %+.3f dB\n", name(model), curve);
    }

    // Body size moves acoustic weighting without transposing the strings.
    const auto small = render(steel, 48, 0.45, 0.8f, 0.65f, 1.0f, 48000.0, 123, 0.7f);
    const auto large = render(steel, 48, 0.45, 0.8f, 0.65f, 1.0f, 48000.0, 123, 1.8f);
    const Spectrum ss(small, 0.04, 0.40), ls(large, 0.04, 0.40);
    const double smallColour = relativeDb(ss.near(2.0 * hz(48)), ss.near(hz(48)));
    const double largeColour = relativeDb(ls.near(2.0 * hz(48)), ls.near(hz(48)));
    check(std::abs(smallColour - largeColour) > 2.0,
          "String-body size changes resonance balance independently of brightness");
    check(std::abs(ss.peakFrequency(hz(48), 2.0, 0.1) - ls.peakFrequency(hz(48), 2.0, 0.1)) < 0.2,
          "Changing body size preserves the played fundamental pitch");
}

void pianoFractionalRegisterContinuity()
{
    // Scala/MTS, tuning reference and fine tune can place a new strike on
    // either side of a fractional register boundary. Hold playback frequency
    // fixed to isolate the onset's spectrum/phase from ordinary pitch drift.
    // A tiny newly admitted partial must not reshuffle the coupled strings.
    // Course detuning/strength contours must also remain continuous through
    // their nominal boundaries and the ends of their short transition ranges.
    const auto capture = [](double onsetNote, double playingNote, double hostRate,
                            std::uint32_t seed) {
        Audio audio {2.0 * hostRate, {}};
        audio.samples.resize(static_cast<std::size_t>(0.6 * audio.rate));
        InstrumentBody body;
        body.prepare(audio.rate, hostRate);
        body.noteOn(piano, static_cast<float>(onsetNote), 0.95f, 1.0f, 1.0f, seed);
        bool finite = true;
        for (float& sample : audio.samples)
        {
            sample = body.tick(static_cast<float>(hz(playingNote)), 1.0f);
            finite = finite && std::isfinite(sample) && std::abs(sample) < 4.0f;
        }
        generatedSamples += audio.samples.size();
        check(finite && audio.rms(0.02, 0.58) > 1.0e-4,
              "Fractional-register piano capture is finite, bounded and sounding");
        return audio;
    };
    const auto matchedResidual = [](const Audio& first, const Audio& second) {
        const double scale = first.rms(0.02, 0.58) / std::max(second.rms(0.02, 0.58), 1.0e-15);
        double reference = 0.0, error = 0.0;
        for (int i = first.at(0.02); i < first.at(0.58); ++i)
        {
            const double a = first.samples[static_cast<std::size_t>(i)];
            const double difference = a - scale * second.samples[static_cast<std::size_t>(i)];
            reference += a * a;
            error += difference * difference;
        }
        return db(std::sqrt(error / std::max(reference, 1.0e-15)));
    };
    double worst = -300.0;
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (double note : {60.0, 56.0, 40.0, 60.0 - 17.0 / 0.75,
                            30.0, 31.0, 32.0, 47.0, 48.0, 49.0})
            for (std::uint32_t seed : {812u, 317u})
            {
                double previous = 0.0;
                for (double epsilon : {0.001, 0.0001})
                {
                    const auto below = capture(note - epsilon, note, rate, seed);
                    const auto above = capture(note + epsilon, note, rate, seed);
                    const double residual = matchedResidual(below, above);
                    worst = std::max(worst, residual);
                    check(residual < (epsilon == 0.001 ? -60.0 : -80.0),
                          "Tiny fractional piano tuning changes preserve onset voicing");
                    if (epsilon < 0.001)
                        check(previous - residual > 15.0,
                              "Piano onset difference converges as the tuning interval shrinks");
                    previous = residual;
                }
            }
    const auto first = capture(56.0, 56.0, 48000.0, 812u);
    const auto differentStrike = capture(56.0, 56.0, 48000.0, 317u);
    check(matchedResidual(first, differentStrike) > -60.0,
          "Register continuity preserves independent strike-to-strike variation");
    std::printf("PIANO REGISTER CONTINUITY worst matched residual %+.3f dB\n", worst);
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

void sustainedEffortAndOnset()
{
    struct Case { int model, upper; };
    for (const auto test : {Case {bowed, 6}, Case {flute, 3}, Case {clarinet, 5},
                           Case {oboe, 6}, Case {bassoon, 5}, Case {brass, 6}})
    {
        // Render at identical pitch, authored brightness and random seed.
        // Matching the audio RMS removes the external velocity/VCA law from
        // the comparison: a louder copy cannot satisfy the spectral test.
        auto soft = render(test.model, 60, 0.45, 0.10f, 0.60f, 1.0f, 48000.0, 317);
        auto firm = render(test.model, 60, 0.45, 0.95f, 0.60f, 1.0f, 48000.0, 317);
        for (auto* audio : {&soft, &firm})
        {
            const double scale = 0.1 / audio->rms(0.20, 0.42);
            for (float& sample : audio->samples) sample = static_cast<float>(sample * scale);
        }
        const Spectrum ss(soft, 0.20, 0.42), fs(firm, 0.20, 0.42);
        const double softRatio = relativeDb(ss.tone(test.upper * hz(60)), ss.tone(hz(60)));
        const double firmRatio = relativeDb(fs.tone(test.upper * hz(60)), fs.tone(hz(60)));
        check(std::abs(soft.rms(0.20, 0.42) - firm.rms(0.20, 0.42)) < 1.0e-7,
              std::string(name(test.model)) + ": velocity comparison has matched audio RMS");
        check(firmRatio - softRatio > 3.0,
              std::string(name(test.model)) + ": initial effort changes upper spectrum at equal RMS");

        const auto onset = render(test.model, 60, 0.45, 0.65f, 0.60f, 1.0f, 48000.0, 317);
        const Spectrum initial(onset, 0.004, 0.026), held(onset, 0.20, 0.42);
        const double onsetChange = relativeDb(held.tone(test.upper * hz(60)), held.tone(hz(60)))
                                 - relativeDb(initial.tone(test.upper * hz(60)), initial.tone(hz(60)));
        check(onsetChange > 1.5,
              std::string(name(test.model)) + ": upper partials develop after the initial fundamental");
        check(onset.rms(0.008, 0.035) > 0.15 * onset.rms(0.20, 0.42),
              std::string(name(test.model)) + ": relative modal growth retains a prompt playable onset");
        std::printf("SUSTAINED %-20s equal-RMS effort %+.3f dB; onset growth %+.3f dB\n",
                    name(test.model), firmRatio - softRatio, onsetChange);
    }
}

void sustainedSizeAndRegisters()
{
    struct Case { int model; float size; double minimum; };
    for (const auto test : {Case {bowed, 2.1f, 2.0}, Case {flute, 1.8f, 0.25},
                           Case {clarinet, 2.0f, 3.0}, Case {oboe, 1.35f, 2.0},
                           Case {bassoon, 1.8f, 2.0}, Case {brass, 1.8f, 1.0}})
    {
        // Same played pitch, velocity, brightness and seed. Body-size effects
        // must be visible in partial balances without shifting the oscillator.
        const int note = test.model == clarinet ? 60 : 48;
        const auto normal = render(test.model, note, 0.60, 0.8f, 0.65f, 1.0f, 48000.0, 311, 1.0f);
        const auto large = render(test.model, note, 0.60, 0.8f, 0.65f, 1.0f, 48000.0, 311, test.size);
        const Spectrum ns(normal, 0.20, 0.58), ls(large, 0.20, 0.58);
        double maximumDifference = 0.0;
        for (int harmonic = 2; harmonic <= (test.model == flute ? 4 : 12); ++harmonic)
        {
            const double a = relativeDb(ns.tone(harmonic * hz(note)), ns.tone(hz(note)));
            const double b = relativeDb(ls.tone(harmonic * hz(note)), ls.tone(hz(note)));
            maximumDifference = std::max(maximumDifference, std::abs(a - b));
        }
        check(maximumDifference > test.minimum,
              std::string(name(test.model)) + ": body size moves relative spectral weighting");
        check(std::abs(ns.peakFrequency(hz(note), 1.0, 0.05)
                     - ls.peakFrequency(hz(note), 1.0, 0.05)) < 0.15,
              std::string(name(test.model)) + ": body size does not transpose the played pitch");
        std::printf("BODY SIZE %-20s size %.2f max partial-balance change %.3f dB\n",
                    name(test.model), test.size, maximumDifference);
    }

    // Concert-pitch transition in the ordinary clarinet; the corresponding
    // bass-clarinet transition moves down an octave when its size doubles.
    // Half-semitone steps guard the whole transition, including its edges.
    std::array<double, 19> normalRatios {}, bassRatios {};
    for (int i = 0; i < 19; ++i)
        for (int sized = 0; sized < 2; ++sized)
        {
            const double note = 61.0 + 0.5 * i - 12.0 * sized;
            const auto audio = render(clarinet, note, 0.42, 0.8f, 0.60f,
                                      1.0f, 48000.0, 812, sized ? 2.0f : 1.0f);
            const Spectrum spectrum(audio, 0.16, 0.40);
            (sized ? bassRatios : normalRatios)[static_cast<std::size_t>(i)] =
                relativeDb(spectrum.tone(2.0 * hz(note)), spectrum.tone(3.0 * hz(note)));
        }
    for (const auto* ratios : {&normalRatios, &bassRatios})
    {
        check(ratios->front() < -10.0 && ratios->back() > -5.0,
              "Clarinet transitions from odd-dominant chalumeau to mixed upper-register harmonics");
        check(ratios->back() - ratios->front() > 10.0,
              "Clarinet upper-register even harmonics rise substantially relative to odd harmonics");
        double maximumStep = 0.0;
        for (std::size_t i = 1; i < ratios->size(); ++i)
            maximumStep = std::max(maximumStep, std::abs((*ratios)[i] - (*ratios)[i - 1]));
        check(maximumStep < 2.5, "Clarinet register transition has no half-semitone timbre jump");
        std::printf("CLARINET register even/odd %.3f to %.3f dB; max half-step %.3f dB\n",
                    ratios->front(), ratios->back(), maximumStep);
    }
    double geometryDifference = 0.0;
    for (std::size_t i = 0; i < normalRatios.size(); ++i)
        geometryDifference = std::max(geometryDifference, std::abs(normalRatios[i] - bassRatios[i]));
    check(geometryDifference < 0.15,
          "Bass-clarinet size moves the register law and formants together by an octave");
}

void sustainedLiveExpression()
{
    const auto withPressure = [](int model, bool enabled)
    {
        Audio audio {96000.0, std::vector<float>(96000)};
        InstrumentBody instrument;
        instrument.prepare(audio.rate, audio.rate * 0.5);
        instrument.noteOn(model, 60.0f, 0.7f, 0.40f, 1.0f, 515);
        bool finite = true;
        for (std::size_t i = 0; i < audio.samples.size(); ++i)
        {
            const float pressure = enabled && i >= 38400 && i < 67200 ? 1.0f : 0.0f;
            const float sample = instrument.tick(static_cast<float>(hz(60)), 0.40f, pressure);
            audio.samples[i] = sample;
            finite = finite && std::isfinite(sample) && std::abs(sample) < 4.0f;
        }
        generatedSamples += audio.samples.size();
        check(finite, std::string(name(model)) + ": live pressure capture remains finite and bounded");
        return audio;
    };
    for (int model : {bowed, flute, clarinet, oboe, bassoon, brass})
    {
        const auto unpressured = withPressure(model, false), pressed = withPressure(model, true);
        const Spectrum normal(unpressured, 0.50, 0.66), firm(pressed, 0.50, 0.66);
        const double upper = model == flute ? 3.0 : model == clarinet ? 5.0 : 6.0;
        const double spectralChange = relativeDb(firm.tone(upper * hz(60)), firm.tone(hz(60)))
                                    - relativeDb(normal.tone(upper * hz(60)), normal.tone(hz(60)));
        check(spectralChange > 0.5,
              std::string(name(model)) + ": live pressure changes timbre during an existing held note");
        double initialDifference = 0.0, steadyDifference = 0.0, recoveredDifference = 0.0;
        for (std::size_t i = 0; i < pressed.samples.size(); ++i)
        {
            const double difference = pressed.samples[i] - unpressured.samples[i];
            if (i >= 38400 && i < 38592) initialDifference += difference * difference;
            if (i >= 48000 && i < 57600) steadyDifference += difference * difference;
            if (i >= 86400) recoveredDifference += difference * difference;
        }
        const double initialRms = std::sqrt(initialDifference / 192.0);
        const double steadyRms = std::sqrt(steadyDifference / 9600.0);
        check(initialRms < 0.20 * steadyRms,
              std::string(name(model)) + ": pressure step is smoothed before the modal gain ramp");
        check(recoveredDifference < steadyDifference * 1.0e-8,
              std::string(name(model)) + ": releasing pressure returns to the unpressured timbre");
        check(std::equal(pressed.samples.begin(), pressed.samples.begin() + 38400,
                         unpressured.samples.begin()),
              std::string(name(model)) + ": zero authorized pressure is exactly the unpressured path");
        std::printf("LIVE PRESSURE %-20s spectrum %+.3f dB; first-2ms/settled difference %.4f\n",
                    name(model), spectralChange, initialRms / std::max(steadyRms, 1.0e-15));
    }

    // Organ/harpsichord are controls for an accidental global expression path.
    for (int model : {organ, harpsichord})
        check(withPressure(model, false).samples == withPressure(model, true).samples,
              std::string(name(model)) + ": pressure cannot impose a breath or bow response");

    const auto moving = render(brass, 60, 5.4, 0.8f, 0.55f, 1.0f, 48000.0, 993);
    const auto repeat = render(brass, 60, 5.4, 0.8f, 0.55f, 1.0f, 48000.0, 993);
    check(moving.samples == repeat.samples, "Sustained timbre motion is deterministic for a private seed");
    double minimum = 1000.0, maximum = -1000.0, previous = 0.0, maximumStep = 0.0;
    for (int i = 0; i < 22; ++i)
    {
        const double begin = 0.8 + 0.2 * i;
        const Spectrum spectrum(moving, begin, begin + 0.18);
        const double ratio = relativeDb(spectrum.tone(6.0 * hz(60)), spectrum.tone(hz(60)));
        minimum = std::min(minimum, ratio); maximum = std::max(maximum, ratio);
        if (i != 0) maximumStep = std::max(maximumStep, std::abs(ratio - previous));
        previous = ratio;
    }
    check(maximum - minimum > 0.015 && maximum - minimum < 0.6,
          "Held breath timbre develops restrained ongoing spectral motion after onset");
    check(maximumStep < 0.15, "Slow breath-timbre movement remains smooth between analysis windows");
    const Spectrum early(moving, 0.8, 1.5), late(moving, 4.5, 5.2);
    check(std::abs(early.peakFrequency(hz(60), 0.5, 0.025)
                 - late.peakFrequency(hz(60), 0.5, 0.025)) < 0.05,
          "Slow spectral motion does not wobble the sustained fundamental pitch");
    std::printf("TIMBRE MOTION brass %.4f dB held range, %.4f dB maximum 200ms step\n",
                maximum - minimum, maximumStep);
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
    struckRegisterAndExcitation();
    pianoFractionalRegisterContinuity();
    brightnessAndTouch();
    sustainedEffortAndOnset();
    sustainedSizeAndRegisters();
    sustainedLiveExpression();
    pitchMotionAndNyquist();
    resetAndLongTermSafety();
    std::printf("Instrument bodies: %d checks, %d failures; %llu generated samples\n",
                checks, failures, static_cast<unsigned long long>(generatedSamples));
    return failures == 0 ? 0 : 1;
}

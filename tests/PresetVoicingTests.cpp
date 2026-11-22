// Behavioral regressions for a small, deliberately chosen part of the curated
// library. These are software-rendering checks of the named synthesizer voices,
// not claims of acoustic-instrument or hardware equivalence. LibraryAuditTests
// remains responsible for the complete bank's level, range and Legacy gates.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
constexpr double sampleRate = 44100.0;
constexpr int blockSize = 256;
constexpr double pi = 3.14159265358979323846;

// Broad, audible distinctions rather than snapshots of the current parameters:
// a disappearing component loses at least 20 dB; a sustaining body stays within
// 6 dB; a wide velocity response gains at least 6 dB. A lower-register partial
// must be at least 26 dB below an intentionally high wind fundamental.
constexpr double maxDecayedRatio = 0.10;
constexpr double minSustainedRatio = 0.50;
constexpr double minVelocityRatio = 2.0;
constexpr double maxLowerPartialRatio = 0.05;
constexpr double audibleRms = 0.001; // -60 dBFS, comfortably below audition level.
constexpr double diagnosticRms = 0.0001;
int failures = 0;
int assertions = 0;
double generatedSeconds = 0.0;

void check(bool passed, const std::string& message)
{
    ++assertions;
    if (!passed)
    {
        ++failures;
        std::printf("FAIL: %s\n", message.c_str());
    }
}

double db(double value)
{
    return 20.0 * std::log10(std::max(value, 1.0e-12));
}

double ratio(double numerator, double denominator)
{
    return numerator / std::max(denominator, 1.0e-12);
}

const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(),
        [name](const FactoryPreset& entry) { return entry.name == name; });
    if (found == bank.end())
        throw std::runtime_error(std::string("Missing curated program: ") + name);
    return *found;
}

struct Audio
{
    std::vector<float> stereo;

    int frames() const { return static_cast<int>(stereo.size() / 2); }

    int frame(double seconds) const
    {
        return std::clamp(static_cast<int>(std::llround(seconds * sampleRate)), 0, frames());
    }

    // Stereo power avoids cancellation from the authored pans and chorus.
    double rms(double begin, double end) const
    {
        const int first = frame(begin), last = frame(end);
        if (last <= first)
            throw std::runtime_error("Empty audio measurement window");
        double energy = 0.0;
        for (int i = first; i < last; ++i)
        {
            const double left = stereo[static_cast<size_t>(2 * i)];
            const double right = stereo[static_cast<size_t>(2 * i + 1)];
            energy += 0.5 * (left * left + right * right);
        }
        return std::sqrt(energy / (last - first));
    }

    double onsetRms() const
    {
        double loudest = 0.0;
        for (int window = 0; window < 5; ++window)
            loudest = std::max(loudest, rms(window * 0.05, (window + 1) * 0.05));
        return loudest;
    }
};

struct PressureEvent
{
    double seconds;
    float amount;
};

// Notes remain held for the entire render. In particular, a short articulation
// cannot pass by being released by the fixture. Full patches retain every
// factory setting, including gain, noise, drift, chorus and touch destinations.
Audio render(const ParamValues& values, const std::string& label, int note,
             float velocity, double seconds, bool fullPatch = true,
             std::initializer_list<PressureEvent> pressure = {})
{
    Engine engine;
    engine.prepare(sampleRate);
    engine.setParams(values);
    check(engine.getRenderPolicy() == RenderPolicy::sourcePerformance,
          label + ": uses the curated circuit render policy");
    engine.noteOn(note, velocity, 1);

    Audio audio;
    audio.stereo.resize(static_cast<size_t>(2 * std::llround(seconds * sampleRate)));
    auto event = pressure.begin();
    for (int done = 0; done < audio.frames();)
    {
        while (event != pressure.end() && audio.frame(event->seconds) <= done)
        {
            engine.channelPressure(1, event->amount);
            ++event;
        }
        int count = std::min(blockSize, audio.frames() - done);
        if (event != pressure.end())
            count = std::min(count, audio.frame(event->seconds) - done);
        engine.render(audio.stereo.data() + 2 * done, count);
        done += count;
    }

    bool finite = true;
    double peak = 0.0;
    for (const float sample : audio.stereo)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite, label + ": every rendered sample is finite");
    if (fullPatch)
        check(peak < 1.0, label + ": held single note stays below 0 dBFS");
    generatedSeconds += audio.frames() / sampleRate;
    return audio;
}

Audio render(const FactoryPreset& voice, int note, float velocity, double seconds)
{
    return render(voice.values, voice.name, note, velocity, seconds);
}

Audio rankOnly(const FactoryPreset& voice, int rank, int note, double seconds,
               float velocity = 0.9f)
{
    auto values = voice.values;
    // This is the only preset override used by the diagnostics. Keep both
    // ranks running and change only the musician-facing rank balance.
    values[P_mix] = rank == 0 ? 0.0f : 1.0f;
    return render(values, voice.name + (rank == 0 ? " rank I" : " rank II"),
                  note, velocity, seconds, false);
}

void mixedOnset(const FactoryPreset& voice, const Audio& full, const Audio& body,
                const Audio& transient, double begin, double end, double minRatio)
{
    // An audible isolated rank can still disappear at the authored balance.
    // Reapply that balance to measure its contribution relative to the body.
    const double mix = voice.values[P_mix];
    const double bodyRms = (1.0 - mix) * body.rms(begin, end);
    const double transientRms = mix * transient.rms(begin, end);
    check(bodyRms > audibleRms && transientRms > minRatio * bodyRms,
          voice.name + ": transient remains present at the actual rank balance");

    // Validate this diagnostic against the unmodified patch. These voices have
    // effectively linear output; a future nonlinear effect or shared signal
    // must not silently make the isolated-rank estimate misrepresent the mix.
    const int first = full.frame(begin), last = full.frame(end);
    double error = 0.0;
    for (int frame = first; frame < last; ++frame)
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto i = static_cast<size_t>(2 * frame + channel);
            const double residual = full.stereo[i]
                - ((1.0 - mix) * body.stereo[i] + mix * transient.stereo[i]);
            error += 0.5 * residual * residual;
        }
    const double residualRms = std::sqrt(error / (last - first));
    check(residualRms < 0.01 * full.rms(begin, end),
          voice.name + ": weighted ranks reconstruct the full-patch onset within 1 percent RMS");
    std::printf("MIX ONSET %-19s transient/body %7.1f dB; reconstruction error %7.1f dB relative\n",
                voice.name.c_str(), db(ratio(transientRms, bodyRms)),
                db(ratio(residualRms, full.rms(begin, end))));
}

// Hann-windowed Goertzel, searched within +/-30 cents. The search tolerates
// authored analog dispersion/drift and avoids requiring a frequency to fall on
// an FFT bin. Separate stereo magnitudes prevent pan-related cancellation.
double toneNear(const Audio& audio, double begin, double end, double nominalHz)
{
    const int first = audio.frame(begin), last = audio.frame(end), count = last - first;
    if (count < 2)
        throw std::runtime_error("Empty spectral measurement window");
    std::vector<std::array<double, 2>> windowed(static_cast<size_t>(count));
    double weightSum = 0.0;
    for (int i = 0; i < count; ++i)
    {
        const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * i / (count - 1));
        weightSum += weight;
        for (int ch = 0; ch < 2; ++ch)
            windowed[static_cast<size_t>(i)][static_cast<size_t>(ch)] =
                audio.stereo[static_cast<size_t>(2 * (first + i) + ch)] * weight;
    }
    double strongest = 0.0;
    for (int cents = -30; cents <= 30; cents += 5)
    {
        const double hz = nominalHz * std::exp2(cents / 1200.0);
        const double coefficient = 2.0 * std::cos(2.0 * pi * hz / sampleRate);
        std::array<double, 2> previous {}, beforePrevious {};
        for (const auto& sample : windowed)
            for (int ch = 0; ch < 2; ++ch)
            {
                const size_t c = static_cast<size_t>(ch);
                const double next = sample[c] + coefficient * previous[c] - beforePrevious[c];
                beforePrevious[c] = previous[c];
                previous[c] = next;
            }
        double stereoPower = 0.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            const size_t c = static_cast<size_t>(ch);
            stereoPower += std::max(0.0, previous[c] * previous[c]
                + beforePrevious[c] * beforePrevious[c]
                - coefficient * previous[c] * beforePrevious[c]);
        }
        strongest = std::max(strongest, 2.0 * std::sqrt(0.5 * stereoPower) / weightSum);
    }
    return strongest;
}

double noteHz(int midiNote)
{
    return 440.0 * std::exp2((midiNote - 69) / 12.0);
}

void independentTransients()
{
    struct Case { const char* name; int note; float velocity; };
    for (const auto& test : { Case { "Pan Flute", 67, 0.8f }, Case { "Sine Bass", 48, 0.9f } })
    {
        const auto& voice = preset(test.name);
        const auto full = render(voice, test.note, test.velocity, 1.0);
        const auto body = rankOnly(voice, 0, test.note, 1.0, test.velocity);
        const auto transient = rankOnly(voice, 1, test.note, 1.0, test.velocity);
        const double initialTransient = transient.rms(0.004, 0.070);
        const double lateTransient = transient.rms(0.65, 0.90);
        const double initialBody = body.rms(0.10, 0.25);
        const double lateBody = body.rms(0.65, 0.90);
        check(initialTransient > diagnosticRms, voice.name + ": separate transient is audible at onset");
        check(lateTransient < maxDecayedRatio * initialTransient,
              voice.name + ": independent transient dies on a held key");
        check(lateBody > audibleRms && lateBody > minSustainedRatio * initialBody,
              voice.name + ": pitched body survives the transient");
        check(full.rms(0.65, 0.90) > audibleRms, voice.name + ": unmodified full patch remains audible");
        if (voice.name == "Pan Flute")
            // A chiff more than 20 dB below its body at the immediate onset
            // can satisfy isolated decay checks while vanishing in the mix.
            mixedOnset(voice, full, body, transient, 0.004, 0.020, 0.10);
        std::printf("TRANSIENT %-19s transient late/onset %7.1f dB; body late/early %+6.1f dB; full late %6.1f dBFS\n",
                    test.name, db(ratio(lateTransient, initialTransient)),
                    db(ratio(lateBody, initialBody)), db(full.rms(0.65, 0.90)));
    }
}

void pluckFingerTransient()
{
    const auto& voice = preset("Nylon Pluck");
    const auto full = render(voice, 60, 0.9f, 0.25);
    const auto body = rankOnly(voice, 0, 60, 0.25);
    const auto transient = rankOnly(voice, 1, 60, 0.25);
    // A mellow supporting edge may sit 30 dB below the body; it must still
    // contribute to the first 10 ms and disappear while the key remains held.
    mixedOnset(voice, full, body, transient, 0.0, 0.010, 0.03);
    check(transient.rms(0.10, 0.20) < maxDecayedRatio * transient.rms(0.0, 0.010),
          voice.name + ": finger transient dies promptly on a held key");
}

void octavePercussion()
{
    const auto& voice = preset("Percussive Organ");
    constexpr int note = 60;
    const double fundamental = noteHz(note);
    const auto full = render(voice, note, 0.9f, 1.5);
    const auto body = rankOnly(voice, 0, note, 1.3);
    const auto percussion = rankOnly(voice, 1, note, 1.3);
    const double upper = toneNear(percussion, 0.025, 0.20, 2.0 * fundamental);
    const double otherOctaves = std::max(toneNear(percussion, 0.025, 0.20, fundamental),
                                       toneNear(percussion, 0.025, 0.20, 4.0 * fundamental));
    check(upper > diagnosticRms && upper > 10.0 * otherOctaves,
          voice.name + ": percussion is the octave above the 8-foot body");
    check(percussion.rms(0.95, 1.25) < maxDecayedRatio * percussion.rms(0.025, 0.20),
          voice.name + ": isolated percussion decays while held");
    check(body.rms(0.95, 1.25) > minSustainedRatio * body.rms(0.025, 0.20),
          voice.name + ": isolated fundamental sustains");

    const double earlyRoot = toneNear(full, 0.025, 0.20, fundamental);
    const double earlyOctave = toneNear(full, 0.025, 0.20, 2.0 * fundamental);
    const double lateRoot = toneNear(full, 0.95, 1.35, fundamental);
    const double lateOctave = toneNear(full, 0.95, 1.35, 2.0 * fundamental);
    check(earlyOctave > 0.08 * earlyRoot, voice.name + ": octave percussion is present in the full patch");
    check(lateOctave < maxDecayedRatio * earlyOctave, voice.name + ": full-patch octave percussion fades");
    check(lateRoot > audibleRms && lateRoot > minSustainedRatio * earlyRoot,
          voice.name + ": full-patch fundamental remains held");
    std::printf("PERCUSSION octave/other %6.1f dB; full octave/root onset %6.1f dB; octave late/early %6.1f dB; root late/early %+5.1f dB\n",
                db(ratio(upper, otherOctaves)), db(ratio(earlyOctave, earlyRoot)),
                db(ratio(lateOctave, earlyOctave)), db(ratio(lateRoot, earlyRoot)));
}

void highWindRegistration()
{
    struct Case { const char* name; double registerRatio; };
    for (const auto& test : { Case { "Piccolo", 4.0 }, Case { "Ocarina", 2.0 }, Case { "Whistle", 2.0 } })
    {
        constexpr int note = 60;
        const auto& voice = preset(test.name);
        const auto audio = render(voice, note, 0.8f, 1.0);
        const double intendedHz = noteHz(note) * test.registerRatio;
        const double intended = toneNear(audio, 0.35, 0.90, intendedHz);
        double lower = 0.0;
        // Include all lower octave registrations down to 16-foot, rather than
        // checking only the immediately adjacent octave.
        for (double registration = test.registerRatio * 0.5; registration >= 0.5; registration *= 0.5)
            lower = std::max(lower, toneNear(audio, 0.35, 0.90, noteHz(note) * registration));
        check(intended > audibleRms, voice.name + ": intended high fundamental is audible");
        check(lower < maxLowerPartialRatio * intended,
              voice.name + ": no prominent unintended lower-octave pitch");
        std::printf("REGISTER  %-19s intended %6.1f dBFS; strongest lower octave %7.1f dB relative\n",
                    test.name, db(intended), db(ratio(lower, intended)));
    }
}

void velocityResponse()
{
    for (const char* name : { "Velocity Keys", "Velocity Lead" })
    {
        const auto& voice = preset(name);
        const auto soft = render(voice, 60, 0.2f, 0.8);
        const auto hard = render(voice, 60, 1.0f, 0.8);
        const double softAttack = soft.rms(0.015, 0.25), hardAttack = hard.rms(0.015, 0.25);
        const double softBody = soft.rms(0.35, 0.70), hardBody = hard.rms(0.35, 0.70);
        check(softAttack > diagnosticRms && softBody > diagnosticRms,
              voice.name + ": soft strike still produces a usable body");
        check(hardAttack > minVelocityRatio * softAttack,
              voice.name + ": hard attack is substantially louder than soft attack");
        check(hardBody > minVelocityRatio * softBody,
              voice.name + ": wide velocity dynamics persist beyond the onset");
        std::printf("VELOCITY  %-19s hard/soft attack %+6.1f dB; body %+6.1f dB; soft body %6.1f dBFS\n",
                    name, db(ratio(hardAttack, softAttack)), db(ratio(hardBody, softBody)), db(softBody));
    }
}

void pressureRequiredLead()
{
    const auto& voice = preset("Pressure Lead");
    // Exercise a single held note: silence, full channel pressure, then letting
    // pressure go again. Windows allow the engine's pressure smoothing to settle.
    const auto audio = render(voice.values, voice.name, 67, 0.9f, 2.2, true,
                              { { 0.4, 1.0f }, { 1.3, 0.0f } });
    const double unpressed = audio.rms(0.10, 0.35);
    const double pressed = audio.rms(0.90, 1.20);
    const double released = audio.rms(1.95, 2.20);
    check(unpressed < diagnosticRms, voice.name + ": no-pressure held key is at the silence/noise floor");
    check(pressed > audibleRms && pressed > 100.0 * unpressed,
          voice.name + ": full pressure brings out a clearly audible tone");
    check(released < 0.03 * pressed, voice.name + ": releasing pressure fades the held tone");
    std::printf("PRESSURE  unpressed %7.1f dBFS; pressed %6.1f dBFS; released/pressed %7.1f dB\n",
                db(unpressed), db(pressed), db(ratio(released, pressed)));
}

void heldArticulation()
{
    for (const char* name : { "Brass Stab", "Pizzicato Strings", "Spiccato Strings", "Poly Stab", "Flute Organ 8+4" })
    {
        const auto& voice = preset(name);
        const auto audio = render(voice, 60, 0.9f, 1.8);
        const double early = audio.onsetRms(), late = audio.rms(1.35, 1.75);
        check(early > audibleRms, voice.name + ": held articulation begins audibly");
        if (voice.name == "Flute Organ 8+4")
            check(late > minSustainedRatio * early, voice.name + ": sustained organ keeps its held body");
        else
            check(late < maxDecayedRatio * early, voice.name + ": short articulation dies without note-off");
        std::printf("HELD      %-19s late/onset %7.1f dB; onset %6.1f dBFS\n",
                    name, db(ratio(late, early)), db(early));
    }
}

void longSwells()
{
    struct Case { const char* name; double seconds, bloomBegin, bloomEnd; };
    // Each hold extends beyond the named voice's slow amplitude/filter opening;
    // Sweep Pad specifically continues past its approximately four-second rise.
    // No pressure ramp manufactures an apparent time-based swell.
    for (const auto& test : { Case { "Slow Pad", 3.0, 1.7, 2.2 },
                              Case { "Slow Strings", 2.6, 1.4, 1.9 },
                              Case { "Bowed Swell Strings", 3.2, 1.6, 2.1 },
                              Case { "Brass Pad Swell", 3.2, 1.7, 2.2 },
                              Case { "Sweep Pad", 4.8, 4.0, 4.5 } })
    {
        const auto& voice = preset(test.name);
        const auto audio = render(voice, 60, 0.9f, test.seconds);
        const double opening = audio.rms(0.025, 0.125);
        const double bloom = audio.rms(test.bloomBegin, test.bloomEnd);
        const double held = audio.rms(test.seconds - 0.5, test.seconds);
        check(bloom > audibleRms && bloom > 2.0 * opening,
              voice.name + ": long opening grows into an audible swell");
        check(held > audibleRms && held > 0.35 * bloom,
              voice.name + ": body remains after the full opening has elapsed");
        std::printf("SWELL     %-19s bloom/opening %+6.1f dB; final/bloom %+6.1f dB; held %.1f s\n",
                    test.name, db(ratio(bloom, opening)), db(ratio(held, bloom)), test.seconds);
    }
}
} // namespace

int main()
{
    try
    {
        independentTransients();
        pluckFingerTransient();
        octavePercussion();
        highWindRegistration();
        velocityResponse();
        pressureRequiredLead();
        heldArticulation();
        longSwells();
    }
    catch (const std::exception& error)
    {
        check(false, error.what());
    }
    std::printf("Preset voicing regressions: %d/%d checks passed; %.1f seconds generated.\n",
                assertions - failures, assertions, generatedSeconds);
    return failures == 0 ? 0 : 1;
}

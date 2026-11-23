// Audio contracts for the 2026.11.2 named-voice corrections. These establish
// specific spectral and articulation roles, not acoustic-instrument parity.
// Every role also renders an intentionally defective control: a threshold that
// accepts the old mistake must fail this test, even if today's recipe passes.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
int checks = 0, failures = 0;
double generatedSeconds = 0.0;

void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL: %s\n", message.c_str());
    }
}

double db(double magnitude) { return 20.0 * std::log10(std::max(magnitude, 1.0e-15)); }
double relativeDb(double numerator, double denominator)
{
    return db(numerator / std::max(denominator, 1.0e-15));
}
double noteHz(int note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }

const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(),
        [name](const auto& item) { return item.name == name; });
    if (found == bank.end()) throw std::runtime_error(std::string("Missing preset: ") + name);
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
    double rms(double begin, double end) const
    {
        const int first = frame(begin), last = frame(end);
        if (last <= first) throw std::runtime_error("Empty measurement window");
        double energy = 0.0;
        for (int i = first * 2; i < last * 2; ++i)
        {
            const double value = stereo[static_cast<size_t>(i)];
            energy += value * value;
        }
        return std::sqrt(energy / (2.0 * (last - first)));
    }
};

Audio render(const ParamValues& values, int note, float velocity, double seconds,
             const std::string& label)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(sampleRate);
    engine->setParams(values);
    check(engine->getRenderPolicy() == RenderPolicy::sourcePerformance,
          label + ": actual circuit render policy");
    engine->noteOn(note, velocity, 1);
    Audio audio;
    audio.stereo.resize(static_cast<size_t>(2 * std::llround(seconds * sampleRate)));
    for (int at = 0; at < audio.frames();)
    {
        const int count = std::min(128, audio.frames() - at);
        engine->render(audio.stereo.data() + 2 * at, count);
        at += count;
    }
    bool finite = true;
    double peak = 0.0;
    for (float sample : audio.stereo)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(static_cast<double>(sample)));
    }
    check(finite && peak < 1.0, label + ": complete voice is finite and below full scale");
    check(peak > 0.001, label + ": sounding capture cannot pass as silence");
    generatedSeconds += seconds;
    return audio;
}

// Integrate Hann-windowed DFT bins around independently specified note
// harmonics. A band includes detuned neighbors and avoids exact-bin dependence.
struct Spectrum
{
    std::vector<double> mono;
    Spectrum(const Audio& audio, double begin, double end)
    {
        const int first = audio.frame(begin), count = audio.frame(end) - first;
        if (count < 2) throw std::runtime_error("Short spectrum window");
        mono.resize(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
        {
            const auto at = static_cast<size_t>(2 * (first + i));
            const double window = 0.5 - 0.5 * std::cos(2.0 * pi * i / (count - 1));
            mono[static_cast<size_t>(i)] = window * 0.5 * (audio.stereo[at] + audio.stereo[at + 1]);
        }
    }
    double bandPower(double center) const
    {
        const int count = static_cast<int>(mono.size());
        const double width = std::max(8.0, 0.005 * center);
        const int low = std::max(1, static_cast<int>(std::ceil((center - width) * count / sampleRate)));
        const int high = std::min(count / 2 - 1,
            static_cast<int>(std::floor((center + width) * count / sampleRate)));
        double power = 0.0;
        for (int bin = low; bin <= high; ++bin)
        {
            const auto step = std::polar(1.0, -2.0 * pi * bin / count);
            std::complex<double> phase(1.0, 0.0), sum(0.0, 0.0);
            for (double sample : mono) { sum += phase * sample; phase *= step; }
            power += std::norm(sum);
        }
        return power;
    }
    double harmonicDb(double fundamental, int harmonic) const
    {
        return relativeDb(std::sqrt(bandPower(harmonic * fundamental)),
                          std::sqrt(bandPower(fundamental)));
    }
};

void vibraphoneMode()
{
    const auto& voice = preset("Vibraphone");
    auto oldOctave = voice.values;
    oldOctave[chParam(1, CP_feet)] = 3.0f; // Defective prior 4' upper registration.
    for (int note : {48, 60, 72}) for (float velocity : {0.30f, 0.85f})
    {
        const auto full = render(voice.values, note, velocity, 0.8, "Vibraphone");
        const auto negative = render(oldOctave, note, velocity, 0.8, "Vibraphone octave control");
        const Spectrum current(full, 0.08, 0.68), previous(negative, 0.08, 0.68);
        const double hz = noteHz(note);
        const double fourth = current.harmonicDb(hz, 4);
        const double second = current.harmonicDb(hz, 2);
        const double wrongFourth = previous.harmonicDb(hz, 4);
        // The upper mode must be present in the complete mix, follow the key,
        // and remain a supporting mode rather than replacing the fundamental.
        const auto hasFourthMode = [](double h4, double h2)
        { return h4 > -36.0 && h4 < -14.0 && h4 - h2 > 8.0; };
        check(hasFourthMode(fourth, second), "Vibraphone: tracked fourth mode is present above the octave artifact");
        check(!hasFourthMode(wrongFourth, previous.harmonicDb(hz, 2)),
              "Vibraphone: old octave voicing fails the same fourth-mode criterion");
        check(fourth - wrongFourth > 24.0, "Vibraphone: fourth-mode correction exceeds a small spectral fluctuation");
        std::printf("VIBRAPHONE note=%d velocity=%.2f H4/H1=%.3f H2/H1=%.3f oldH4/H1=%.3f dB\n",
                    note, velocity, fourth, second, wrongFourth);
    }
}

// Least-squares fit of log-RMS against offset, linear envelope trend and the
// named tremolo frequency. The trend prevents a decaying attack or slow swell
// from being mistaken for rhythmic amplitude modulation.
double fittedPulse(const Audio& audio, double frequency)
{
    std::array<std::array<double, 5>, 4> normal {};
    for (int window = 100; window < 340; ++window)
    {
        const double start = window * 0.02, center = start + 0.01;
        const std::array<double, 4> basis {1.0, center - 4.4,
            std::sin(2.0 * pi * frequency * center), std::cos(2.0 * pi * frequency * center)};
        const double level = db(audio.rms(start, start + 0.02));
        for (size_t row = 0; row < 4; ++row)
        {
            for (size_t col = 0; col < 4; ++col) normal[row][col] += basis[row] * basis[col];
            normal[row][4] += basis[row] * level;
        }
    }
    for (size_t pivot = 0; pivot < 4; ++pivot)
    {
        size_t best = pivot;
        for (size_t row = pivot + 1; row < 4; ++row)
            if (std::abs(normal[row][pivot]) > std::abs(normal[best][pivot])) best = row;
        std::swap(normal[pivot], normal[best]);
        const double divisor = normal[pivot][pivot];
        if (std::abs(divisor) < 1.0e-12) throw std::runtime_error("Singular pulse fit");
        for (size_t col = pivot; col < 5; ++col) normal[pivot][col] /= divisor;
        for (size_t row = 0; row < 4; ++row) if (row != pivot)
        {
            const double scale = normal[row][pivot];
            for (size_t col = pivot; col < 5; ++col) normal[row][col] -= scale * normal[pivot][col];
        }
    }
    return 2.0 * std::hypot(normal[2][4], normal[3][4]);
}

void namedTremolo()
{
    struct Case { const char* name; double frequency; };
    for (const auto& test : {Case {"Tremolo Organ", 3.2}, Case {"Tremolo Pad", 3.8}, Case {"Tremolo Poly", 5.4}})
    {
        const auto& voice = preset(test.name);
        auto wetOnly = voice.values;
        wetOnly[P_subVca] = 0.0f; // Preserve the BBD color and all other controls.
        for (int note : {48, 60, 72})
        {
            const auto full = render(voice.values, note, 0.85f, 7.0, test.name);
            const auto negative = render(wetOnly, note, 0.85f, 7.0, std::string(test.name) + " wet-only control");
            const double pulse = fittedPulse(full, test.frequency), oldPulse = fittedPulse(negative, test.frequency);
            check(full.rms(2.0, 6.8) > 0.001, std::string(test.name) + ": held body stays above the silence floor");
            check(pulse > 1.5 && pulse < 4.5, std::string(test.name) + ": named frequency produces gentle full-body level motion");
            check(oldPulse < 1.0, std::string(test.name) + ": wet-only control fails the named full-body pulse criterion");
            check(pulse - oldPulse > 1.25, std::string(test.name) + ": rhythmic motion is not merely existing rank beating");
            std::printf("TREMOLO %-14s note=%d frequency=%.1f pulse=%.4f wetOnly=%.4f dB peak-to-peak\n",
                        test.name, note, test.frequency, pulse, oldPulse);
        }
    }
}

void bassDecay()
{
    const auto& rubber = preset("Rubber Bass");
    const auto& picked = preset("Picked Bass");
    auto rubberFloor = rubber.values, pickedShort = picked.values;
    rubberFloor[chParam(0, CP_vS)] = 0.08f;
    pickedShort[chParam(0, CP_vD)] = 3.1f;
    for (int note : {36, 48, 60})
    {
        const auto full = render(rubber.values, note, 0.85f, 4.0, "Rubber Bass");
        const auto negative = render(rubberFloor, note, 0.85f, 4.0, "Rubber Bass sustain-floor control");
        const double late = relativeDb(full.rms(3.5, 3.9), full.rms(0.02, 0.08));
        const double oldLate = relativeDb(negative.rms(3.5, 3.9), negative.rms(0.02, 0.08));
        check(full.rms(0.02, 0.08) > 0.001, "Rubber Bass: the pluck has a non-silent onset");
        check(late < -80.0, "Rubber Bass: a held key fully damps instead of sustaining a permanent bed");
        check(oldLate > -40.0, "Rubber Bass: old sustain floor fails the same damping criterion");

        const auto string = render(picked.values, note, 0.85f, 4.0, "Picked Bass");
        const auto shortString = render(pickedShort, note, 0.85f, 4.0, "Picked Bass short-body control");
        const double body = relativeDb(string.rms(0.8, 1.1), string.rms(0.02, 0.08));
        const double oldBody = relativeDb(shortString.rms(0.8, 1.1), shortString.rms(0.02, 0.08));
        check(body > -14.0 && body < -3.0, "Picked Bass: the body rings after the pick while continuing to decay");
        check(oldBody < -15.0, "Picked Bass: shortened old body fails the ringing-body criterion");
        check(body - oldBody > 5.0, "Picked Bass: retained body is not simply a louder onset");
        check(relativeDb(string.rms(3.5, 3.9), string.rms(0.02, 0.08)) < -30.0,
              "Picked Bass: longer ringing body still damps on a held key");
        std::printf("BASS note=%d Rubber late/onset=%.2f old=%.2f; Picked body/onset=%.2f old=%.2f dB\n",
                    note, late, oldLate, body, oldBody);
    }
}

void nylonHarmonics()
{
    const auto& voice = preset("Nylon Pluck");
    auto sineDominant = voice.values;
    sineDominant[chParam(0, CP_sine)] = 0.56f;
    sineDominant[chParam(0, CP_vcfLevel)] = 0.34f;
    sineDominant[chParam(0, CP_lpfCutoff)] = 920.0f;
    for (int note : {48, 60, 72}) for (float velocity : {0.30f, 0.85f})
    {
        const auto full = render(voice.values, note, velocity, 0.4, "Nylon Pluck");
        const auto negative = render(sineDominant, note, velocity, 0.4, "Nylon Pluck sine-dominant control");
        const auto overtones = [note](const Audio& audio)
        {
            const Spectrum spectrum(audio, 0.1, 0.3);
            const double fundamental = noteHz(note);
            double upperPower = 0.0;
            for (int harmonic = 2; harmonic <= 12; ++harmonic)
                upperPower += spectrum.bandPower(harmonic * fundamental);
            return relativeDb(std::sqrt(upperPower), std::sqrt(spectrum.bandPower(fundamental)));
        };
        const double current = overtones(full), previous = overtones(negative);
        check(current > -12.0 && current < -3.0, "Nylon Pluck: string harmonics support a dominant fundamental");
        check(previous < -13.0, "Nylon Pluck: old sine-dominant body fails the harmonic-presence criterion");
        check(current - previous > 4.0, "Nylon Pluck: harmonic improvement survives register and velocity changes");
        std::printf("NYLON note=%d velocity=%.2f overtones/body=%.3f old=%.3f dB\n",
                    note, velocity, current, previous);
    }
}
} // namespace

int main()
{
    try
    {
        vibraphoneMode();
        namedTremolo();
        bassDecay();
        nylonHarmonics();
        std::printf("Preset precision: %d checks, %d failures; %.1f seconds of actual circuit audio\n",
                    checks, failures, generatedSeconds);
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "Preset precision error: %s\n", error.what());
        return 2;
    }
}

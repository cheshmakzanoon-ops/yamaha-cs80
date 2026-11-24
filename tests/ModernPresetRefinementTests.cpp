// Played response and effect articulation for the eighteen existing modern
// factory programs. These are rendered behavior contracts, not acoustic parity.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
constexpr double rate = 48000.0;
int checks = 0, failures = 0;
constexpr std::array<const char*, 18> names {
    "Delay Brass", "Plate Pad", "Phaser Strings", "Drive Lead", "Arp Poly", "Arp Bells",
    "Arp Bass", "LFO Sweep Pad", "LFO Pan Keys", "PWM LFO Strings", "Stereo Delay Pluck",
    "Ambient Bells", "Ensemble Plus", "Space Lead", "Random Arp", "Drive Bass",
    "Phased Polysynth", "Tremolo LFO Organ"
};
void check(bool condition, const std::string& label)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-15)); }
const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto it = std::find_if(bank.begin(), bank.end(), [name](const auto& p) { return p.name == name; });
    if (it == bank.end()) throw std::runtime_error(std::string("Missing preset: ") + name);
    return *it;
}
struct Audio
{
    std::vector<float> samples;
    // The normalized first-difference energy tracks broad harmonic brightness
    // independently of gain. Its ratio to a frozen-modulation control isolates
    // the named spectral movement from the patch's amplitude envelope.
    double brightness(double start, double end) const
    {
        const int first = std::max(1, (int) std::llround(start * rate));
        const int last = (int) std::llround(end * rate);
        double difference = 0.0, energy = 0.0;
        for (int i = first * 2; i < last * 2; ++i)
        {
            const double value = samples[(size_t) i];
            const double step = value - samples[(size_t) (i - 2)];
            difference += step * step; energy += value * value;
        }
        return std::sqrt(difference / std::max(energy, 1.e-24));
    }
    // Duty-cycle motion chiefly changes even/odd partial balance, which can
    // remain nearly invisible to a broad spectral centroid. Project the first
    // four harmonics through a Hann window, retaining stereo channel energy.
    double evenToOdd(double start, double end) const
    {
        const int first = (int) std::llround(start * rate), last = (int) std::llround(end * rate);
        constexpr double pi = 3.14159265358979323846;
        double even = 0.0, odd = 0.0;
        for (int harmonic = 1; harmonic <= 4; ++harmonic)
        {
            const double omega = 2.0 * pi * 261.6255653005986 * harmonic / rate;
            const double stepCos = std::cos(omega), stepSin = std::sin(omega);
            double cosine = 1.0, sine = 0.0, real[2] {}, imaginary[2] {};
            for (int i = first; i < last; ++i)
            {
                const double window = .5 - .5 * std::cos(2.0 * pi * (i - first) / (last - first - 1));
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double value = samples[(size_t) (2 * i + channel)] * window;
                    real[channel] += value * cosine; imaginary[channel] += value * sine;
                }
                const double nextCos = cosine * stepCos - sine * stepSin;
                sine = sine * stepCos + cosine * stepSin; cosine = nextCos;
            }
            const double energy = real[0] * real[0] + imaginary[0] * imaginary[0]
                                + real[1] * real[1] + imaginary[1] * imaginary[1];
            if (harmonic % 2 == 0) even += energy; else odd += energy;
        }
        return std::sqrt(std::max(even, 1.e-24) / std::max(odd, 1.e-24));
    }
    double rms(double start, double end, int channel = -1) const
    {
        const int first = (int) std::llround(start * rate), last = (int) std::llround(end * rate);
        double sum = 0;
        for (int i = first; i < last; ++i)
            if (channel < 0)
                sum += .5 * ((double) samples[(size_t) (2 * i)] * samples[(size_t) (2 * i)]
                           + (double) samples[(size_t) (2 * i + 1)] * samples[(size_t) (2 * i + 1)]);
            else sum += (double) samples[(size_t) (2 * i + channel)] * samples[(size_t) (2 * i + channel)];
        return std::sqrt(sum / (last - first));
    }
};
Audio render(const FactoryPreset& p, int note, float velocity, double held, double tail = 0.0)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate); engine->setParams(p.values); engine->noteOn(note, velocity, 1);
    const int release = (int) std::llround(held * rate), frames = (int) std::llround((held + tail) * rate);
    Audio a; a.samples.resize((size_t) frames * 2);
    for (int at = 0; at < frames;)
    {
        if (at == release) engine->noteOff(note, 1);
        const int count = std::min(128, (at < release ? release : frames) - at);
        engine->render(a.samples.data() + 2 * at, count); at += count;
    }
    bool finite = true; double peak = 0;
    for (float value : a.samples) { finite = finite && std::isfinite(value); peak = std::max(peak, std::abs((double) value)); }
    check(finite && peak > .00001 && peak < 1.0, p.name + ": finite audible complete render with headroom");
    return a;
}
FactoryPreset dry(const FactoryPreset& p)
{
    auto result = p;
    result.values[P_fx1Mix] = result.values[P_fx2Mix] = result.values[P_fx3Mix] = 0.0f;
    return result;
}
void measureTouch(const char* name, int note)
{
    const auto& p = preset(name);
    const bool struck = p.category == "Keys" || p.category == "Bells" || p.category == "Plucked";
    const auto soft = render(p, note, .25f, 1.6), firm = render(p, note, .85f, 1.6);
    const double first = struck ? 0.0 : .25, last = struck ? .35 : 1.5;
    const double contrast = db(firm.rms(first, last) / soft.rms(first, last));
    if (p.category == "Organ")
        check(std::abs(contrast) < .1, p.name + ": organ registration remains independent of key velocity");
    else
        check(contrast > 7.2 && contrast < 18.0, p.name + ": useful touch contrast survives its complete effect chain");
    std::printf("TOUCH %-20s note %d %.3f dB\n", name, note, contrast);
}
}
int main()
{
    try
    {
        for (const auto* name : names) measureTouch(name, std::string(name) == "Drive Bass" ? 48 : 60);
        for (const char* name : { "Phaser Strings", "PWM LFO Strings", "Ensemble Plus", "LFO Pan Keys", "Stereo Delay Pluck" })
            for (int note : { 48, 72 }) measureTouch(name, note);

        // Each full struck main body may continue its own physical-style loss
        // law after the first attack. An otherwise identical zero-sustain
        // outer envelope must be audibly distinguishable from that aftersound.
        for (const char* name : { "LFO Pan Keys", "Stereo Delay Pluck", "Ambient Bells" })
        {
            const auto& p = preset(name);
            const auto full = render(p, 60, .85f, 5.0);
            auto choked = p; choked.values[chParam(0, CP_vS)] = 0.0f;
            const auto control = render(choked, 60, .85f, 5.0);
            const double continuation = db(full.rms(4.5, 5.0) / control.rms(4.5, 5.0));
            const double decayed = db(full.rms(4.5, 5.0) / full.rms(0.0, .2));
            check(continuation > 3.0, p.name + ": held aftersound distinguishes a choked-envelope control");
            check(decayed < -12.0 && decayed > -45.0, p.name + ": held body continues to decay rather than becoming a static pad");
            std::printf("BODY %-20s aftersound %.3f dB; improvement over choked control %.3f dB\n", name, decayed, continuation);
        }
        // Dry releases clear a phrase while the echo remains deliberately
        // audible. Removing all insert wet paths is a causal negative control.
        for (const char* name : { "Delay Brass", "Space Lead", "Stereo Delay Pluck" })
        {
            const auto& p = preset(name);
            const auto wet = render(p, 60, .85f, .16, 1.0), direct = render(dry(p), 60, .85f, .16, 1.0);
            const double echo = db(wet.rms(.42, .62) / direct.rms(.42, .62));
            check(wet.rms(.42, .62) > .000001 && echo > 15.0,
                  p.name + ": separately audible echo survives the compact dry release");
            std::printf("ECHO %-20s wet versus dry %.3f dB\n", name, echo);
        }
        // The pan motion remains a played effect, not an unrelated second
        // pitched layer. Energy is measured in both channels without summing.
        const auto pan = render(preset("LFO Pan Keys"), 60, .85f, 2.2);
        double smallest = 1.0, largest = -1.0;
        for (double t = .4; t < 1.9; t += .025)
        {
            const double left = pan.rms(t, t + .025, 0), right = pan.rms(t, t + .025, 1);
            const double balance = (right * right - left * left) / (right * right + left * left);
            smallest = std::min(smallest, balance); largest = std::max(largest, balance);
        }
        check(smallest < -.35 && largest > .35, "LFO Pan Keys: body audibly crosses both sides of the stereo center");

        // Freeze only the named modulation route; the same notes, envelopes,
        // detuning, chorus and room remain. A moving gain alone cannot pass a
        // normalized spectral-brightness comparison.
        for (const char* name : { "LFO Sweep Pad", "PWM LFO Strings", "Phaser Strings", "Phased Polysynth" })
        {
            const auto& p = preset(name);
            auto frozen = p;
            if (std::string(name).find("Phas") != std::string::npos) frozen.values[P_fx1Mix] = 0.0f;
            else frozen.values[P_lfo1Depth] = 0.0f;
            const auto moving = render(p, 60, .85f, 10.0), stationary = render(frozen, 60, .85f, 10.0);
            double low = 100.0, high = -100.0, sum = 0.0, sumSquares = 0.0;
            int windows = 0;
            for (double t = 3.0; t < 9.5; t += .25)
            {
                const bool pwm = std::string(name) == "PWM LFO Strings";
                const double change = std::log(pwm
                    ? moving.evenToOdd(t, t + .15) / stationary.evenToOdd(t, t + .15)
                    : moving.brightness(t, t + .15) / stationary.brightness(t, t + .15));
                low = std::min(low, change); high = std::max(high, change);
                sum += change; sumSquares += change * change; ++windows;
            }
            const double deviation = std::sqrt(std::max(0.0, sumSquares / windows - (sum / windows) * (sum / windows)));
            check(high - low > .15 && deviation > .04, p.name + ": named spectral motion survives the complete audio path");
            std::printf("MOTION %-20s spectral ratio span %.3f; log standard deviation %.3f\n", name, std::exp(high - low), deviation);
        }

        const auto& organPreset = preset("Tremolo LFO Organ");
        const auto organ = render(organPreset, 60, .85f, 1.0, .3);
        const double ending = db(organ.rms(1.075, 1.105) / organ.rms(.75, .95));
        check(ending < -35.0, "Tremolo LFO Organ: coordinated key damping clears its two sustained stops");
        std::printf("ORGAN key damping %.3f dB\n", ending);
        // With 4 Hz modulation, separated quarter-second windows repeat the
        // amplitude pattern, despite different waveform sample phases.
        const auto heldOrgan = render(organPreset, 60, .85f, 2.0);
        double periodError = 0.0, halfError = 0.0, envelopeEnergy = 0.0;
        for (double t = .5; t < 1.45; t += .01)
        {
            const double a = heldOrgan.rms(t, t + .01);
            const double b = heldOrgan.rms(t + .25, t + .26);
            const double c = heldOrgan.rms(t + .125, t + .135);
            periodError += (a - b) * (a - b);
            halfError += (a - c) * (a - c); envelopeEnergy += a * a;
        }
        check(periodError < .015 * envelopeEnergy && halfError > 6.0 * periodError,
              "Tremolo LFO Organ: full audio preserves the named four-pulse-per-second pattern");
    }
    catch (const std::exception& e) { check(false, e.what()); }
    std::printf("ModernPresetRefinementTests: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

// Relative, rendered contracts for finite piano contact and early radiation.
// These establish boundedness, timing, bandwidth and rate consistency, not
// acoustic equivalence to a measured piano. No exact voicing coefficient is
// duplicated as an expected waveform.
#include "core/PianoHammer.h"
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
constexpr double pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", description); }
}

double hz(double note) { return 440.0 * std::exp2((note - 69.0) / 12.0); }
double db(double amplitude) { return 20.0 * std::log10(std::max(amplitude, 1.0e-16)); }

struct Contact
{
    double rate = 0.0, halfTime = 0.0, endTime = 0.0, impulse = 0.0, peak = 0.0;
    bool finite = true, positive = true;
    std::vector<double> force, board;
};

Contact renderContact(double rate, double host, int note, double velocity,
                      double size = 1.0, double seconds = 0.16)
{
    cs80::body::PianoHammer hammer;
    hammer.noteOn(rate, host, hz(note), velocity, size);
    Contact result;
    result.rate = rate;
    const auto count = static_cast<std::size_t>(std::llround(seconds * rate));
    result.force.resize(count);
    result.board.resize(count);
    double previous = 0.0;
    for (std::size_t sample = 0; sample < count; ++sample)
    {
        const auto frame = hammer.tick();
        const double impulse = frame.onset - previous;
        result.force[sample] = impulse;
        result.board[sample] = frame.board;
        result.finite = result.finite && std::isfinite(frame.onset) && std::isfinite(frame.board);
        result.positive = result.positive && impulse >= 0.0 && frame.onset >= 0.0 && frame.onset <= 1.0;
        result.peak = std::max(result.peak, std::abs(frame.board));
        result.impulse += impulse;
        const double time = static_cast<double>(sample + 1) / rate;
        if (result.halfTime == 0.0 && frame.onset >= 0.5) result.halfTime = time;
        if (result.endTime == 0.0 && frame.onset == 1.0) result.endTime = time;
        previous = frame.onset;
    }
    check(result.finite && result.positive, "contact has finite nonnegative force and bounded monotone onset");
    check(std::abs(result.impulse - 1.0) < 1.0e-12 && result.endTime > 0.0 && result.endTime < 0.01,
          "contact delivers unit impulse and releases in finite time");
    check(result.peak > 1.0e-6 && result.peak < 0.03,
          "soundboard response is audible and retains its independent absolute bound");
    return result;
}

double projection(const std::vector<double>& signal, double rate, double frequency,
                  double firstSeconds = 0.0, double lastSeconds = 0.0)
{
    const auto first = static_cast<std::size_t>(std::llround(firstSeconds * rate));
    const auto last = lastSeconds > 0.0
        ? std::min(signal.size(), static_cast<std::size_t>(std::llround(lastSeconds * rate)))
        : signal.size();
    const double angle = 2.0 * pi * frequency / rate;
    const double cosine = std::cos(angle), sine = std::sin(angle);
    double real = 1.0, imaginary = 0.0, sumReal = 0.0, sumImaginary = 0.0;
    for (std::size_t sample = first; sample < last; ++sample)
    {
        sumReal += signal[sample] * real;
        sumImaginary += signal[sample] * imaginary;
        const double nextReal = real * cosine - imaginary * sine;
        imaginary = imaginary * cosine + real * sine;
        real = nextReal;
    }
    return std::hypot(sumReal, sumImaginary);
}

void contactDynamics()
{
    for (double host : {44100.0, 48000.0, 96000.0})
        for (int note : {24, 36, 60, 84, 108})
        {
            const auto soft = renderContact(2.0 * host, host, note, 0.20);
            const auto hard = renderContact(2.0 * host, host, note, 0.95);
            check(hard.halfTime < 0.85 * soft.halfTime && hard.endTime < 0.85 * soft.endTime,
                  "firmer felt contact delivers and releases its force sooner");
            // Compare equal-impulse contact spectra inside the softer pulse's
            // main lobe, independent of both the level law and FFT bins.
            const double probe = 0.55 / soft.endTime;
            const double softBand = projection(soft.force, soft.rate, probe);
            const double hardBand = projection(hard.force, hard.rate, probe);
            check(hardBand > 1.10 * softBand,
                  "shorter firm contact has greater normalized excitation bandwidth");
            check(soft.force.back() == 0.0 && hard.force.back() == 0.0,
                  "released contact never continues as a noise or force tail");
            std::printf("HAMMER host=%.0f note=%d soft_t50=%.4f hard_t50=%.4f ms bandwidth_delta=%.3f dB peak=%.6f\n",
                        host, note, 1000.0 * soft.halfTime, 1000.0 * hard.halfTime,
                        db(hardBand / softBand), hard.peak);
        }
    for (double velocity : {0.20, 0.60, 0.95})
    {
        const auto bass = renderContact(96000.0, 48000.0, 36, velocity);
        const auto middle = renderContact(96000.0, 48000.0, 60, velocity);
        const auto treble = renderContact(96000.0, 48000.0, 84, velocity);
        check(bass.halfTime > 1.15 * middle.halfTime && middle.halfTime > 1.15 * treble.halfTime,
              "contact becomes shorter continuously through the piano register");
    }
}

double strongestFrequency(const Contact& contact, double first, double last)
{
    double bestFrequency = first, bestMagnitude = 0.0;
    for (double frequency = first; frequency <= last; frequency += 1.0)
    {
        const double magnitude = projection(contact.board, contact.rate, frequency, 0.012, 0.15);
        if (magnitude > bestMagnitude) { bestMagnitude = magnitude; bestFrequency = frequency; }
    }
    return bestFrequency;
}

void fixedBoardAndRateSafety()
{
    const auto bass = renderContact(96000.0, 48000.0, 36, 0.8);
    const auto treble = renderContact(96000.0, 48000.0, 84, 0.8);
    const auto large = renderContact(96000.0, 48000.0, 60, 0.8, 2.0);
    const double bassPeak = strongestFrequency(bass, 140.0, 230.0);
    const double treblePeak = strongestFrequency(treble, 140.0, 230.0);
    const double largePeak = strongestFrequency(large, 65.0, 125.0);
    check(std::abs(bassPeak - treblePeak) < 4.0,
          "board resonance stays at fixed Hz when the key moves four octaves");
    check(largePeak / bassPeak > 0.46 && largePeak / bassPeak < 0.54,
          "existing body-size control lowers the board response independently of string pitch");

    for (int note : {24, 60, 108})
        for (double velocity : {0.2, 0.95})
        {
            const auto low = renderContact(48000.0, 48000.0, note, velocity);
            const auto high = renderContact(192000.0, 48000.0, note, velocity);
            double energy = 0.0, error = 0.0;
            for (std::size_t i = 0; i < low.board.size(); ++i)
            {
                const double reference = high.board[4 * i + 3];
                energy += reference * reference;
                error += (low.board[i] - reference) * (low.board[i] - reference);
            }
            const double relative = std::sqrt(error / std::max(energy, 1.0e-30));
            check(std::abs(low.halfTime - high.halfTime) <= 1.0 / low.rate,
                  "contact timing is invariant across internal-rate changes to one sample");
            check(relative < 0.005,
                  "exact contact impulse integration keeps the early board consistent across 1x and 4x rates");
            std::printf("HAMMER RATE note=%d velocity=%.2f relative_board_error=%.8f\n", note, velocity, relative);
        }

    // The smallest supported host and body expose the strongest risk of
    // a board mode crossing the output Nyquist limit. Measure actual output
    // above that limit at 4x internal rate, rather than inspecting a cutoff.
    const auto lowHost = renderContact(32000.0, 8000.0, 108, 1.0, 0.5);
    double passEnergy = 0.0, stopEnergy = 0.0;
    for (double frequency = 100.0; frequency < 15000.0; frequency += 50.0)
    {
        const double magnitude = projection(lowHost.board, lowHost.rate, frequency);
        if (frequency < 3500.0) passEnergy += magnitude * magnitude;
        else if (frequency >= 4000.0) stopEnergy += magnitude * magnitude;
    }
    const double leakageDb = 10.0 * std::log10(stopEnergy / passEnergy);
    check(leakageDb < -45.0, "host-Nyquist leakage of finite contact radiation remains negligible at the worst supported host rate");
    std::printf("HAMMER BOARD bass_peak=%.1f treble_peak=%.1f large_peak=%.1f Hz 8k_host_stopband=%.2f dB\n",
                bassPeak, treblePeak, largePeak, leakageDb);
}

struct Piano
{
    double peak = 0.0, first = 0.0, earlyEnergy = 0.0, attackEnergy = 0.0;
    bool finite = true;
};

Piano renderPiano(double host, int note, float velocity)
{
    cs80::body::InstrumentBody instrument;
    const double rate = 2.0 * host;
    instrument.prepare(rate, host);
    instrument.noteOn(cs80::body::piano, static_cast<float>(note), velocity, 0.65f, 1.0f, 0x7812a3u);
    Piano result;
    for (int sample = 0; sample < static_cast<int>(0.04 * rate); ++sample)
    {
        const double value = instrument.tick(static_cast<float>(hz(note)), 0.65f);
        if (sample == 0) result.first = std::abs(value);
        result.finite = result.finite && std::isfinite(value);
        result.peak = std::max(result.peak, std::abs(value));
        if (sample < 0.0005 * rate) result.earlyEnergy += value * value;
        if (sample < 0.01 * rate) result.attackEnergy += value * value;
    }
    check(result.finite && result.peak > 1.0e-4 && result.peak < 0.80,
          "integrated piano attack sounds and retains its modal headroom");
    check(result.first < result.peak * 2.0e-5,
          "piano begins with smooth finite contact rather than an immediate noise sample");
    return result;
}

void integratedOnset()
{
    for (double host : {44100.0, 48000.0, 96000.0})
        for (int note : {36, 60, 84})
        {
            const auto soft = renderPiano(host, note, 0.2f);
            const auto hard = renderPiano(host, note, 0.95f);
            const double softFraction = soft.earlyEnergy / soft.attackEnergy;
            const double hardFraction = hard.earlyEnergy / hard.attackEnergy;
            check(hardFraction > 1.25 * softFraction,
                  "firmer integrated piano strike develops a greater fraction of attack energy in its first half millisecond");
            std::printf("PIANO ATTACK host=%.0f note=%d first_sample_ratio=%.9g soft_early_fraction=%.9g hard_early_fraction=%.9g peak=%.6f\n",
                        host, note, hard.first / hard.peak, softFraction, hardFraction, hard.peak);
        }

    cs80::body::PianoHammer fresh, reused;
    fresh.noteOn(96000.0, 48000.0, hz(84), 0.25, 1.4);
    reused.noteOn(96000.0, 48000.0, hz(24), 0.95, 0.5);
    for (int i = 0; i < 400; ++i) (void) reused.tick();
    reused.noteOn(96000.0, 48000.0, hz(84), 0.25, 1.4);
    bool identical = true;
    for (int i = 0; i < 2000; ++i)
    {
        const auto a = fresh.tick(), b = reused.tick();
        identical = identical && a.onset == b.onset && a.board == b.board;
    }
    check(identical, "voice reuse clears the preceding contact and board state exactly");

    cs80::body::PianoHammer invalid;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    invalid.noteOn(nan, nan, nan, nan, nan);
    bool finite = true;
    for (int i = 0; i < 100000; ++i)
    {
        const auto frame = invalid.tick();
        finite = finite && std::isfinite(frame.onset) && std::isfinite(frame.board);
    }
    check(finite && invalid.tick().board == 0.0, "nonfinite setup is sanitized and the board reaches exact silence");
    invalid.reset();
    const auto reset = invalid.tick();
    check(reset.onset == 0.0 && reset.board == 0.0, "reset cancels contact and board memory");
}
}

int main()
{
    contactDynamics();
    fixedBoardAndRateSafety();
    integratedOnset();
    std::printf("Piano hammer contact: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

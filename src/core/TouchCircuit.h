#pragma once

// TWS/TRG touch circuitry (IMPROVEMENT_PLAN 2.4), circuit engine only.
// Evidence: docs/INITIAL_TOUCH_CIRCUIT_EVIDENCE.md, INITIAL_PITCH_TRANSIENT.md,
// SHARED_PRESSURE_SPEED.md. Every constant below is either a component product
// read from TWS17/TWS18/TRG25 (marked SOURCE) or a declared inference (marked
// DECLARED) with its alternatives in docs/workstreams/ws3-keyboard.md.
//
// Per voice (TWS17), from the amplified touch-sensor signal:
//  a  aftertouch: 100k + 1 uF into a follower, a 100 ms low-pass. SOURCE.
//  b  initial touch: AC-coupled sensor signal sampled by FET1 onto a 0.33 uF
//     hold capacitor while the trigger path keeps the FET on, then held.
//     MIDI supplies no sensor waveform, so the declared acquisition is: the
//     window opens at TR; for the first kContactSeconds (key contact before the
//     sensor responds) the coupled signal is at rest (0); then it carries the
//     strike value; the hold node follows it with the FET on-resistance time
//     constant until the window closes, then holds. DECLARED.
//  C  pitch transient: buffered b through 1 uF into 100k || 100k (50 ms high-
//     pass, SOURCE), inverted; the clamp diode on that stage is taken as ideal
//     and stops C going positive, which also resets the coupling memory when b
//     falls at the start of an acquisition. DECLARED (the clamp's conduction
//     law is not established). With a fresh strike this reproduces the former
//     reset-to-velocity onset, now as a circuit consequence.
// Shared (TRG25): every a feeds a 1S1555 anode; the cathodes form one bus with
// 47k to ground into IC17 (high impedance) through 330k. SOURCE topology. The
// diode law uses a 1N4148-class small-signal model and a declared 5 V full-scale
// a level; the bus is normalised so a single full-pressure key reads 1.

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::touch
{
inline constexpr double kAPathSeconds = 100.0e3 * 1.0e-6;          // SOURCE: 0.1 s
inline constexpr double kCPathSeconds = 1.0e-6 * (100.0e3 * 100.0e3 / 200.0e3); // SOURCE: 50 ms
inline constexpr double kContactSeconds = 0.002;   // DECLARED: contact-to-sensor lead (alt 0-10 ms)
inline constexpr double kAcquireSeconds = 1.0e3 * 0.33e-6; // DECLARED: ~1k 2SK30 on-resistance * 0.33 uF
inline constexpr double kWindowSeconds = 0.01e-6 * 1.0e6;  // DECLARED: 0.01 uF * 1 M trigger differentiator
inline constexpr double kHoldLeakSeconds = 0.0;    // DECLARED nominal: no b droop (alt 3.3 s = 10M * 0.33 uF)

struct Coefficients
{
    double a = 0.0, c = 0.0, acquire = 0.0, leak = 1.0;
    // The contact and window instants fall inside samples contactSample and
    // windowSample; the acquisition is split exactly at them.
    int contactSample = 0, windowSample = 1;
    double contactBefore = 1.0, contactAfter = 1.0, windowPart = 1.0;
    void prepare(double rate, double holdLeakSeconds = kHoldLeakSeconds) noexcept
    {
        a = std::exp(-1.0 / (kAPathSeconds * rate));
        c = std::exp(-1.0 / (kCPathSeconds * rate));
        acquire = std::exp(-1.0 / (kAcquireSeconds * rate));
        leak = holdLeakSeconds > 0.0 ? std::exp(-1.0 / (holdLeakSeconds * rate)) : 1.0;
        const double contact = kContactSeconds * rate, window = kWindowSeconds * rate;
        contactSample = static_cast<int>(std::floor(contact));
        windowSample = std::max(contactSample + 1, static_cast<int>(std::floor(window)));
        const double cf = contact - contactSample, wf = window - std::floor(window);
        contactBefore = std::exp(-cf / (kAcquireSeconds * rate));
        contactAfter = std::exp(-(1.0 - cf) / (kAcquireSeconds * rate));
        windowPart = std::exp(-wf / (kAcquireSeconds * rate));
    }
};

// One voice's a/b/C nodes, normalised (1 = a full-scale strike / pressure).
class TouchVoice
{
public:
    struct Output { double a = 0.0, b = 0.0, c = 0.0; };

    void reset() noexcept { a = b = memory = 0.0; strike = 0.0; elapsed = 1 << 30; }
    // TR edge of a new assignment: the acquisition window opens.
    void trigger(double strikeValue) noexcept
    {
        strike = std::clamp(strikeValue, 0.0, 1.0);
        elapsed = 0;
    }
    // One sample with `pressure` the key's present pressure (0 once released).
    Output tick(const Coefficients& k, double pressure) noexcept
    {
        a = pressure + (a - pressure) * k.a;
        if (elapsed <= k.windowSample)
        {
            if (elapsed < k.contactSample) b *= k.acquire;          // coupled input at rest
            else if (elapsed == k.contactSample)
            {
                b *= k.contactBefore;                                 // rest until contact
                b = strike + (b - strike) * k.contactAfter;           // then the strike value
            }
            else if (elapsed < k.windowSample) b = strike + (b - strike) * k.acquire;
            else b = strike + (b - strike) * k.windowPart;          // FET opens mid-sample
            ++elapsed;
        }
        else b *= k.leak;
        double y = b - memory;   // high-pass output, high-frequency gain 1
        if (y < 0.0)             // ideal clamp: C may not go positive
        {
            memory = b;
            y = 0.0;
        }
        memory = b + (memory - b) * k.c;
        // Snap negligible states to zero well before denormals.
        if (std::abs(a) < 1.0e-12) a = 0.0;
        if (std::abs(b) < 1.0e-12) b = 0.0;
        if (std::abs(memory) < 1.0e-12) memory = 0.0;
        return { a, b, -y };
    }
    Output state() const noexcept { return { a, b, -(std::max(0.0, b - memory)) }; }

private:
    double a = 0.0, b = 0.0, memory = 0.0, strike = 0.0;
    int elapsed = 1 << 30;
};

// ---- TRG5 diode-OR pressure bus ---------------------------------------------
inline constexpr double kBusResistorOhms = 47.0e3;   // SOURCE: bus to ground
inline constexpr double kFullScaleVolts = 5.0;       // DECLARED: a at full pressure (alt 2-10 V)
inline constexpr double kDiodeSaturationAmps = 2.52e-9; // DECLARED 1S1555 stand-in (1N4148 model)
inline constexpr double kDiodeEmissionThermal = 1.752 * 0.025852; // n * kT/q at 300 K

// Bus voltage for normalised sources a_i (each scaled by kFullScaleVolts),
// solving sum Is (exp((Va_i - V)/nVt) - 1) = V / 47k by Newton iteration from
// `guess`. `iterations` bounds the work (realtime: few, truth: many).
template <std::size_t N>
double busVolts(const std::array<double, N>& a, double guess, int iterations) noexcept
{
    double top = 0.0;
    for (double x : a) top = std::max(top, x * kFullScaleVolts);
    if (top <= 0.0) return 0.0;
    // Newton on an exponential climbs only ~nVt per step from far below, so
    // start from the single-diode drop estimate unless the guess is close.
    const double estimate = std::max(0.0, top - kDiodeEmissionThermal
        * std::log1p(top / kBusResistorOhms / kDiodeSaturationAmps));
    double v = std::abs(guess - estimate) < 0.1 ? std::clamp(guess, 0.0, top) : estimate;
    for (int i = 0; i < iterations; ++i)
    {
        double current = 0.0, slope = 0.0;
        for (double x : a)
        {
            // A diode 0.9 V below conduction passes < 1e-17 A: skip it.
            if (x * kFullScaleVolts < v - 0.9) continue;
            const double e = std::exp((x * kFullScaleVolts - v) / kDiodeEmissionThermal);
            current += kDiodeSaturationAmps * (e - 1.0);
            slope += kDiodeSaturationAmps * e / kDiodeEmissionThermal;
        }
        const double f = current - v / kBusResistorOhms;
        const double step = f / (slope + 1.0 / kBusResistorOhms);
        v = std::clamp(v + step, 0.0, top);
        if (std::abs(step) < 1.0e-12) break;
    }
    return v;
}

// Bus voltage of one full-pressure key alone: the normalisation of the shared
// speed drive (a single key at full pressure drives the SUB speed fully).
inline double fullScaleBusVolts() noexcept
{
    static const double full = busVolts(std::array<double, 1> { 1.0 }, kFullScaleVolts * 0.9, 64);
    return full;
}
} // namespace cs80::touch

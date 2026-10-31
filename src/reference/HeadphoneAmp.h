#pragma once
// OFFLINE reference for the CS-80 headphone amplifier listening model, in double
// precision straight from the stage equations. No production audio dependencies.
//
// Source account (docs/OUTPUT_LEVEL_SOURCE_REVIEW.md, "Stereo, GENERAL and
// headphone boundaries"): HL/HR leave the LEFT/RIGHT preamp outputs BEFORE the
// output coupling capacitors and the HIGH/LOW switch. SUB29 (service manual
// p29) shows two IC11 feedback amplifiers with 100 k inputs and 150 k feedback,
// discrete complementary output sections Tr7-10 / Tr11-14 with diode biasing and
// 22 ohm output resistors; PN5 (p67) carries RO/LO to the headphone jack and
// shows a direct-wired arrangement (delta-marked) and a capacitor-coupled
// alternative with 470 uF / 25 V capacitors and 10 k bias resistors.
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace cs80::reference::headphone
{
enum class Coupling { direct, capacitor };

struct Profile
{
    double inputOhms = 100.0e3;      // source-derived (SUB29)
    double feedbackOhms = 150.0e3;   // source-derived (SUB29)
    double outputOhms = 22.0;        // source-derived (SUB29)
    double couplingFarads = 470.0e-6; // source-derived (PN5 alternative)
    double biasOhms = 10.0e3;        // source-derived (PN5 alternative)
    // INFERRED: the delta-marked direct-wired drawing is taken as the later
    // revision. Alternative: the capacitor-coupled arrangement (about 6 Hz
    // high-pass into 32 ohm).
    Coupling coupling = Coupling::direct;
    // INFERRED: a typical modern 32 ohm headphone. Alternatives 8, 300, 600 ohm.
    double loadOhms = 32.0;
    // INFERRED topology: inverting gain -Rf/Rin with the feedback taken before
    // the 22 ohm resistor, so that resistor divides against the load. The
    // alternative (feedback after it) has no load division.
    // INFERRED large-signal law: the output stage shares the preamp's rails, so
    // it limits at the same model-unit ceiling as the preamp driver
    // (circuitOutputDriver: C1 tanh knee from -1 dB of the ceiling). Current
    // limiting and crossover distortion are not modelled.
    double ceiling = 1.0;
    double knee = 0.891250938; // -1 dB of the ceiling
};

inline double gain(const Profile& p) noexcept { return -p.feedbackOhms / p.inputOhms; }

// Rail-limited output-stage node for an ideal amplifier output x.
inline double limit(double x, const Profile& p) noexcept
{
    const double m = std::abs(x), k = p.knee * p.ceiling;
    if (!(m > k)) return x;
    const double span = p.ceiling - k;
    return std::copysign(k + span * std::tanh((m - k) / span), x);
}

inline double jackShunt(const Profile& p) noexcept
{
    return p.coupling == Coupling::capacitor ? p.loadOhms * p.biasOhms / (p.loadOhms + p.biasOhms) : p.loadOhms;
}

// Small-signal jack / preamp-source transfer at f (Hz).
inline std::complex<double> response(double f, const Profile& p)
{
    const double shunt = jackShunt(p);
    std::complex<double> series = p.outputOhms;
    if (p.coupling == Coupling::capacitor)
        series += f > 0.0 ? std::complex<double>(0.0, -1.0 / (2.0 * std::numbers::pi * f * p.couplingFarads))
                          : std::complex<double>(std::numeric_limits<double>::infinity(), 0.0);
    return gain(p) * shunt / (series + shunt);
}

// Static (DC, direct-wired) large-signal transfer.
inline double staticTransfer(double source, const Profile& p) noexcept
{
    return limit(gain(p) * source, p) * p.loadOhms / (p.loadOhms + p.outputOhms);
}
} // namespace cs80::reference::headphone

#pragma once

// A compact authored piano contact/early-board model. Bank & Sujbert separate
// hammer force from the soundboard response and identify attack reproduction
// as a weakness of otherwise useful string models (DAFx-02, sections 2--4):
// https://www.dafx.de/papers/DAFX02_Bank_Sujbert_piano_synthesis.pdf
// This is not their nonlinear, measured or commuted model. In particular the
// contact curve, register law, resonance residues and losses below are voicing
// choices. No claim is made that these four resonances represent a real piano.
//
// The felt force is represented by the nonnegative finite pulse
// 60*u^2*(1-u)^3, 0 <= u <= 1. Its cumulative impulse is evaluated at sample
// boundaries: even a nonintegral contact length delivers exactly one impulse.
// Harder contact is shorter (qualitatively following a hardening felt spring).
// A small fixed-Hz board response is driven by that same impulse, rather than
// by random noise or a directly audible force pulse. All trigonometry and
// exponentials occur at note onset; the sample loop has fixed storage/work.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cs80::body
{

class PianoHammer
{
public:
    struct Frame
    {
        double onset = 0.0;
        double board = 0.0;
    };

    void reset() noexcept
    {
        resonances = {};
        position = cumulative = 0.0;
        positionStep = 1.0;
        duration = referenceContact;
        samplesRemaining = 0;
    }

    void noteOn(double internalRate, double hostRate, double frequency,
                double velocity, double size) noexcept
    {
        reset();
        const double rate = finiteClamp(internalRate, 8000.0, 1536000.0, 96000.0);
        const double host = finiteClamp(hostRate, 8000.0, rate, std::min(rate, 48000.0));
        const double key = finiteClamp(frequency, 8.1757989156, 12543.853951, 261.625565);
        const double touch = finiteClamp(velocity, 0.0, 1.0, 0.7);
        const double boardSize = finiteClamp(size, 0.5, 2.5, 1.0);

        // A power-law contact-time trend is motivated by the hardening felt
        // spring, but normalized velocity is not measured hammer speed. The
        // offset, exponent and register scaling are therefore authored.
        duration = std::clamp(referenceContact * std::pow(261.625565 / key, 0.20)
                              * std::pow((0.20 + touch) / 0.90, -0.38),
                              0.00065, 0.0055);
        positionStep = 1.0 / (rate * duration);
        samplesRemaining = static_cast<std::uint32_t>(std::ceil(rate * (duration + 0.75)));

        // Signed radiation residues allow the spatial modes to oppose each
        // other. Their absolute sum is one. Since each damped rotation has
        // norm <= 1 and its nonnegative drive sums to one, |board| <= level
        // for every sample, rate, size and strike. No output limiter is used.
        constexpr std::array<double, 4> centers {185.0, 420.0, 1050.0, 2450.0};
        constexpr std::array<double, 4> losses {58.0, 80.0, 125.0, 180.0};
        constexpr std::array<double, 4> residues {0.34, -0.28, 0.23, -0.15};
        const double level = 0.008 + 0.020 * touch * touch;
        for (std::size_t i = 0; i < resonances.size(); ++i)
        {
            auto& mode = resonances[i];
            const double hz = centers[i] / boardSize;
            // The host rate owns the audible band even when this model runs
            // oversampled. Fade a resonance before the existing modal bank's
            // 0.465*host cutoff; never fold a culled mode into another pitch.
            const double x = std::clamp((0.465 - hz / host) / 0.085, 0.0, 1.0);
            const double band = x * x * (3.0 - 2.0 * x);
            mode.weight = residues[i] * level * band;
            if (mode.weight == 0.0) continue;
            const double angle = twoPi * hz / rate;
            const double halfLoss = std::exp(-0.5 * losses[i] / (rate * std::sqrt(boardSize)));
            mode.halfReal = halfLoss * std::cos(0.5 * angle);
            mode.halfImaginary = halfLoss * std::sin(0.5 * angle);
            mode.cosine = mode.halfReal * mode.halfReal - mode.halfImaginary * mode.halfImaginary;
            mode.sine = 2.0 * mode.halfReal * mode.halfImaginary;
        }
    }

    // Existing principal/coupled partial growth remains intact, but now runs
    // on the same contact clock as the finite global onset. Call at note-on.
    double onsetScale() const noexcept { return duration / referenceContact; }

    Frame tick() noexcept
    {
        if (samplesRemaining == 0) return {cumulative, 0.0};
        --samplesRemaining;

        double impulse = 0.0;
        if (position < 1.0)
        {
            position = std::min(1.0, position + positionStep);
            // Integral of the finite felt force. Endpoint handling makes
            // contact end exactly, without an exponential force tail.
            const double u = position;
            const double next = u >= 1.0 ? 1.0
                : u * u * u * (20.0 + u * (-45.0 + u * (36.0 - 10.0 * u)));
            // The analytic CDF is monotone; guard last-bit cancellation near
            // its flat endpoint so no negative force can enter a resonator.
            const double bounded = std::clamp(next, cumulative, 1.0);
            impulse = bounded - cumulative;
            cumulative = bounded;
        }

        double output = 0.0;
        for (auto& mode : resonances)
        {
            const double real = mode.real * mode.cosine - mode.imaginary * mode.sine
                              + impulse * mode.halfReal;
            mode.imaginary = mode.imaginary * mode.cosine + mode.real * mode.sine
                           + impulse * mode.halfImaginary;
            mode.real = real;
            output += mode.weight * mode.imaginary;
        }
        return {cumulative, output};
    }

private:
    static constexpr double twoPi = 6.283185307179586476925286766559;
    static constexpr double referenceContact = 0.0021;

    struct Resonance
    {
        double real = 0.0, imaginary = 0.0;
        double cosine = 0.0, sine = 0.0;
        double halfReal = 0.0, halfImaginary = 0.0;
        double weight = 0.0;
    };

    static double finiteClamp(double value, double low, double high, double fallback) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    }

    std::array<Resonance, 4> resonances {};
    double position = 0.0, positionStep = 1.0, cumulative = 0.0;
    double duration = referenceContact;
    std::uint32_t samplesRemaining = 0;
};

} // namespace cs80::body

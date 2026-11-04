#pragma once
// Control smoothing for the circuit engine (IMPROVEMENT_PLAN 0.2, moved to
// Phase 1 because the pinned probes move controls mid-render). Legacy never
// uses it, so its block-rate behavior stays byte-identical.
#include <algorithm>
#include <array>
#include <cmath>

namespace cs80
{
// DECLARED ENGINEERING CHOICE, not a recovered CS-80 constant. The panel CV
// lines are RC-filtered on the boards, but their component values are not in
// the evidence set. 5 ms moves the corner of a stepped control to 32 Hz, so
// the spectrum of a block-rate step falls as 1/f^2 instead of 1/f above it
// (inaudible zipper), while a fast hand move is still followed well inside
// 10 ms. It matches the existing 5 ms output-switch readout dezipper.
// Alternatives considered: 2 ms (more residual zipper on large cutoff jumps),
// 10-20 ms (audibly lagging bends and fader flicks).
inline constexpr double kControlSmoothingSeconds = 0.005;

// One-pole coefficient for y += k (x - y): tau in seconds at any rate, so
// the smoothing time does not depend on host rate or oversampling factor.
inline double smoothingCoefficient(double seconds, double sampleRate) noexcept
{
    return seconds > 0.0 && sampleRate > 0.0 ? -std::expm1(-1.0 / (seconds * sampleRate)) : 1.0;
}

// The time constant a legacy per-sample constant had at its reference rate,
// the 48 kHz host at 2x (96 kHz internal). The circuit engine keeps that
// reference behavior at every other rate; legacy keeps the constant itself.
inline constexpr double kLegacyReferenceInternalRate = 96000.0;
inline double legacyPerSampleSeconds(double perSampleCoefficient) noexcept
{
    return -1.0 / (kLegacyReferenceInternalRate * std::log1p(-perSampleCoefficient));
}

// A bank of one-pole smoothers sharing one coefficient. Values are gathered
// and scattered by index each interval, so the caller keeps its own field list
// in ONE place. Double state: long glides end exactly on their target, and a
// value within 1e-9 (relative) of its target snaps to it, after which the
// bank reports settled() and the caller can skip all work: static renders keep
// their exact unsmoothed arithmetic.
class SmootherBank
{
public:
    static constexpr int kCapacity = 128;

    void setCoefficient(double k) noexcept { coefficient = std::clamp(k, 0.0, 1.0); }
    double getCoefficient() const noexcept { return coefficient; }

    // The next rendered sample starts exactly on the latest targets (no glide):
    // after construction, reset, prepare and policy changes, so whatever frame
    // is applied before audio starts defines the initial state.
    void requestSnap() noexcept { snapPending = true; }
    bool snapIsPending() const noexcept { return snapPending; }

    // Retarget slot i and return the value the caller must keep using now (the
    // running value; a slot's first target is taken as is). Nothing reads the
    // fields between a retarget and the next sample, where a pending snap lands.
    float retarget(int i, float target) noexcept
    {
        if (i < 0 || i >= kCapacity) return target;
        targets[(size_t) i] = target;
        if (!initialised[(size_t) i])
        {
            current[(size_t) i] = target;
            initialised[(size_t) i] = true;
            return target;
        }
        if ((double) target != current[(size_t) i])
            moving = true;
        return (float) current[(size_t) i];
    }
    // Jump slot i to its stored target (pending snap at the first sample).
    float snapToTarget(int i) noexcept
    {
        if (i < 0 || i >= kCapacity) return 0.0f;
        current[(size_t) i] = targets[(size_t) i];
        return targets[(size_t) i];
    }
    // The first rendered sample after a snap pass ends the snap window.
    void endSnap() noexcept { snapPending = false; moving = false; }

    // Advance slot i by one sample toward its target and return the value.
    float advance(int i) noexcept
    {
        if (i < 0 || i >= kCapacity) return 0.0f;
        const double target = targets[(size_t) i];
        double& value = current[(size_t) i];
        value += coefficient * (target - value);
        if (std::abs(target - value) <= 1.0e-9 * std::max(1.0, std::abs(target)))
            value = target;
        else
            stillMoving = true;
        return (float) value;
    }
    // Call before / after an advance pass over every used slot.
    void beginAdvance() noexcept { stillMoving = false; }
    void endAdvance() noexcept { moving = stillMoving; }

    bool settled() const noexcept { return !moving; }
    float value(int i) const noexcept
    { return i >= 0 && i < kCapacity ? (float) current[(size_t) i] : 0.0f; }

private:
    std::array<double, kCapacity> current {};
    std::array<float, kCapacity> targets {};
    std::array<bool, kCapacity> initialised {};
    double coefficient = 1.0;
    bool snapPending = true, moving = false, stillMoving = false;
};

// Single smoothed controller (performance input set by an event handler).
// snap() follows a new note; otherwise each sample moves 1 - exp(-T/tau).
class ControlSmoother
{
public:
    void snap(double target) noexcept { current = target; }
    double advance(double target, double k) noexcept
    {
        // Exact fixed point of the existing finite recurrence. Assign the target
        // as its snap step does, so changing the sign of zero is retained too.
        if (current == target && std::isfinite(target) && std::isfinite(k))
            return current = target;
        current += k * (target - current);
        if (std::abs(target - current) <= 1.0e-9 * std::max(1.0, std::abs(target)))
            current = target;
        return current;
    }
    bool atTarget(double target) const noexcept { return current == target; }
    double value() const noexcept { return current; }

private:
    double current = 0.0;
};
} // namespace cs80

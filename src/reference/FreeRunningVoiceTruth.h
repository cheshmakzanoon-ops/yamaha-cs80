#pragma once

// Offline truth model for free-running voice continuity (IMPROVEMENT_PLAN 2.1).
// NOT used by Engine. One rank chain (oscillator -> IG00156 HP stage -> LP stage)
// advanced at the full internal rate on EVERY sample, in double precision, with
// the circuit-stage implicit equations solved to convergence. It restates the
// declared realtime laws (Dsp.h Oscillator::tick and Svf::processCircuit, the
// in-loop OTA saturation of docs/IG00156_REALTIME_MODEL.md) without the
// realtime model's float state, cached Newton start or iteration cap, so the
// cheap idle path in src/experimental/ContinuousVoiceCircuit.h can be held to it.
// It is a numerical reference for the declared model, not a measured CS-80.

#include <algorithm>
#include <cmath>

namespace cs80::reference::free_running
{
inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kOtaLevel = 2.5;           // Dsp.h kIg00156OtaLevel (inferred)
inline constexpr double kResetPulseSeconds = 30.0e-6; // Dsp.h IG00153 reset pulse

struct RankControls
{
    double frequency = 261.6255653;
    double pulseWidth = 0.5;
    bool saw = true, pulse = false;
    double noise = 0.0; // the truth model is noise-free; must stay 0
    double hpCutoff = 47.575, hpQ = 0.5;   // cutoff INCLUDING the feedforward floor
    double lpCutoff = 2222.575, lpQ = 0.7;
    bool hpOn = true, lpOn = true;
};

inline double polyBlep(double t, double dt)
{
    if (dt <= 0.0) return 0.0;
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

inline double blepPulse(double phase, double width, double dt)
{
    width = std::clamp(width, 0.000001, 0.999999);
    double v = phase < width ? 1.0 : -1.0;
    v += polyBlep(phase, dt);
    double t = phase - width;
    if (t < 0.0) t += 1.0;
    return v - polyBlep(t, dt);
}

struct StageState { double s1 = 0.0, s2 = 0.0; };

// Converged trapezoidal IG00156 circuit stage (same equations as processCircuit).
struct StageOutput { double lp = 0.0, hp = 0.0, bp = 0.0; };
inline StageOutput circuitStage(StageState& st, double input, double cutoffHz, double q, double rate)
{
    cutoffHz = std::clamp(cutoffHz, 18.0, rate * 0.45);
    q = std::clamp(q, 0.35, 9.0);
    const double g = std::tan(kPi * cutoffHz / rate), k = 1.0 / q;
    const double v = kOtaLevel, inv = 1.0 / v;
    double hp = (input - (k + g) * st.s1 - st.s2) / (1.0 + g * (k + g));
    double bp = 0.0, lp = 0.0;
    for (int iteration = 0; iteration < 64; ++iteration)
    {
        const double t1 = std::tanh(hp * inv);
        bp = st.s1 + g * v * t1;
        const double t2 = std::tanh(bp * inv);
        lp = st.s2 + g * v * t2;
        const double f = hp + k * bp + lp - input;
        if (std::abs(f) < 1.0e-14) break;
        const double d1 = 1.0 - t1 * t1, d2 = 1.0 - t2 * t2;
        hp -= f / (1.0 + g * d1 * (k + g * d2));
    }
    st.s1 = 2.0 * bp - st.s1;
    st.s2 = 2.0 * lp - st.s2;
    return { lp, hp, bp };
}

class RankTruth
{
public:
    void reset(double startPhase, StageState hp, StageState lp)
    {
        phase = startPhase - std::floor(startPhase);
        hpState = hp; lpState = lp;
    }
    // One internal sample. Returns the filtered (pre-VCA) rank signal.
    double tick(const RankControls& c, double rate)
    {
        const double dt = std::clamp(c.frequency / rate, 0.0, 0.45);
        phase += dt;
        if (phase >= 1.0) phase -= std::floor(phase);
        const double narrowWidth = std::clamp(kResetPulseSeconds * c.frequency, 0.0001, 0.45);
        const double narrow = blepPulse(phase, narrowWidth, dt) * 0.55;
        double x = 0.0;
        if (c.saw) x += (1.0 - 2.0 * phase) + polyBlep(phase, dt) + narrow * 0.70;
        if (c.pulse) x += blepPulse(phase, c.pulseWidth, dt);
        x *= 0.9;
        double filtered = x;
        if (c.hpOn) filtered = circuitStage(hpState, filtered, c.hpCutoff, c.hpQ, rate).hp;
        if (c.lpOn) filtered = circuitStage(lpState, filtered, c.lpCutoff, c.lpQ, rate).lp;
        return filtered;
    }
    double getPhase() const { return phase; }
    StageState hp() const { return hpState; }
    StageState lp() const { return lpState; }

private:
    double phase = 0.0;
    StageState hpState, lpState;
};
} // namespace cs80::reference::free_running

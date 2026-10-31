#pragma once
// Circuit-engine IG00151 voice VCA (plan step 1.4, docs/IG00151_VCA_MODEL.md).
// The legacy policy keeps the plain levelCv * envelope multiply in Engine.cpp.
//
// Source-derived:
// - IG00151 has distinct exponential (EI1) and linear (LI2) level-control
//   inputs (service p52, HARDWARE_REFERENCES.md).
// - LC31 -> IC5 LI2 with 10 nF to the reference (M41); the LC node is fed by
//   five documented branches: AM 180 k, LV 560 k, TI/TA/KB 270 k each (R39,
//   LEVEL_CONTROL_REFERENCE_DESIGN.md).
// - IC5 is labelled "VCA (for SUB-OSC)" (M41).
// INFERRED (alternatives in the model doc):
// - The SUB VCA modulation drives IC5's remaining control input, EI1, so the
//   tremolo is exponential (constant dB depth). Full depth is +/-1 octave
//   (+/-6 dB), close to the 12.7 dB peak-to-peak span of the former linear law.
// - LI node: the ideal-source Thevenin resistance of the five branches
//   (54.2 kOhm) with the 10 nF pin capacitor, tau = 0.542 ms.
// - Gain = LI x 2^EI x envelope: an OTA control current scaled by an
//   exponential converter; the IG00159 envelope keeps its linear law.
// - Input offset and CV feedthrough with typical OTA magnitudes (not IG00151
//   data). Both scale with the control current, so together they add an
//   envelope-shaped DC pedestal (the "thump"), -50 dB or less.
// - The next stage is AC-coupled like IC5's documented input (1 uF NP into
//   100 kOhm, tau 0.1 s): the pedestal survives only as a transient, so it
//   does not reach the ring modulator as a static offset.
#include <algorithm>
#include <cmath>

namespace cs80
{
inline constexpr double kIg00151LevelNodeOhms = 1.0 / (1.0 / 180.0e3 + 1.0 / 560.0e3 + 3.0 / 270.0e3);
inline constexpr double kIg00151LevelNodeFarads = 10.0e-9;
inline constexpr float kIg00151SubOctaves = 1.0f;          // EI octaves at full SUB VCA depth
inline constexpr float kIg00151OffsetUnits = 1.5e-3f;      // input-referred, engine units
inline constexpr float kIg00151FeedthroughUnits = 1.0e-3f; // per unit control current
inline constexpr double kIg00151CouplingSeconds = 1.0e-6 * 100.0e3;

class Ig00151Vca
{
public:
    void prepare(double sampleRate) noexcept
    {
        coefficient = static_cast<float>(-std::expm1(-1.0 / (kIg00151LevelNodeOhms * kIg00151LevelNodeFarads * sampleRate)));
        couplingCoefficient = -std::expm1(-1.0 / (kIg00151CouplingSeconds * sampleRate));
    }
    // Cold start: the LI node is already charged to the static level network;
    // the silent voice left no charge on the output coupling.
    void reset(float linearCv) noexcept { node = linearCv; coupling = 0.0; }
    float levelNode() const noexcept { return node; }
    // Free-running idle path (plan step 2.1): a silent circuit's VCA output is
    // exactly zero (the envelope is closed), so the next-stage coupling simply
    // discharges. The LI node is left to re-acquire LC through its 0.54 ms RC
    // when the circuit is next clocked (declared simplification).
    void relaxIdle(double seconds) noexcept
    {
        coupling *= std::exp(-seconds / kIg00151CouplingSeconds);
        if (std::abs(coupling) < 1.0e-30) coupling = 0.0;
    }

    // linearCv: the LC drive (engine level units, >= 0); expOctaves: EI drive;
    // envelope: the IG00159 amplitude envelope. Returns the VCA output.
    float process(float input, float linearCv, float expOctaves, float envelope) noexcept
    {
        node += (linearCv - node) * coefficient;
        const float control = std::max(0.0f, node) * envelope * std::exp2(expOctaves);
        const float out = control * (input + kIg00151OffsetUnits) + kIg00151FeedthroughUnits * control;
        // Double state: a float 0.1 s pole stalls about -64 dB short of settling.
        coupling += (out - coupling) * couplingCoefficient;
        return static_cast<float>(out - coupling);
    }

private:
    float coefficient = 1.0f, node = 0.0f;
    double couplingCoefficient = 0.0, coupling = 0.0;
};

// Constant-power rank pan normalized to the former centre gain of 0.5 per side:
// left = 0.5 sqrt(1 - pan), right = 0.5 sqrt(1 + pan), so L^2 + R^2 = 0.5.
inline void constantPowerPan(float pan, float& left, float& right) noexcept
{
    const float p = std::clamp(pan, -1.0f, 1.0f);
    left = 0.5f * std::sqrt(1.0f - p);
    right = 0.5f * std::sqrt(1.0f + p);
}
} // namespace cs80

#pragma once
// ws4-circuits pre-registered acceptance tolerances.
//
// Each block below is committed BEFORE the realtime model it judges is evaluated
// (see docs/workstreams/ws4-circuits.md). A value may only change afterwards with a
// documented reason in that report; loosening a bound to make a model pass is not one.
// These are numerical agreement bounds between a realtime model and its offline truth
// model. They are not analog acceptance tolerances and say nothing about a real CS-80.
namespace cs80::ws4_tolerances
{
// Step 1: SUB (IG00150) speed converter and band-limited waveforms.
namespace sub
{
// Closed-form converter law versus the reference node-current solve (relative).
inline constexpr double kLawRelative = 1.0e-9;
// Engine SUB rate measured from the rendered waveform period, versus the reference.
inline constexpr double kMeasuredRateRelative = 1.0e-3;
// Engine rate telemetry after settling, versus the reference (float state).
inline constexpr double kTelemetryRelative = 2.0e-5;
// 10 ms control-node smoothing: log-rate after one time constant, error as a
// fraction of the logarithmic span of the step.
inline constexpr double kLogSmoothingFraction = 0.01;
// Band-limited waveforms: in-band (20 Hz - 20 kHz) non-harmonic energy relative to
// the fundamental, saw and square, 60-200 Hz, internal rates 88.2 and 96 kHz.
inline constexpr double kAliasCeilingDb = -70.0;
// ... and at least this many dB below the naive waveform at the same settings.
inline constexpr double kAliasImprovementDb = 25.0;
// In-band harmonic magnitudes (h * f0 <= 5 kHz) versus the additive band-limited reference.
inline constexpr double kHarmonicDb = 0.25;
} // namespace sub

// Step 2: ring modulator converter (ADJ p19 anchors) and offset multiplier.
namespace ring
{
// Converter law in double versus the anchor-solved reference (relative).
inline constexpr double kLawRelative = 1.0e-9;
// ADJ p19 anchors (150 ms, 5 ms) from the rendered carrier period (relative). The
// procedure itself allows +/-5 %; this bounds the model, not the instrument.
inline constexpr double kAnchorPeriodRelative = 2.0e-3;
// Envelope -> frequency law: float realtime and engine telemetry versus reference.
inline constexpr double kEnvelopeLawRelative = 2.0e-5;
// Multiplier lines (DC, carrier, signal, both sidebands), coherent DFT versus analytic.
inline constexpr double kLineDb = 0.01;
// Engine output line ratios after the downstream linear stages.
inline constexpr double kEngineLineRatioDb = 0.5;
} // namespace ring
} // namespace cs80::ws4_tolerances

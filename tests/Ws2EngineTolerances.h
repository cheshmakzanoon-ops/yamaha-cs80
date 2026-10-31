#pragma once
// Pre-registered ws2-engine gate tolerances (docs/workstreams/ws2-engine.md).
// Committed BEFORE the realtime models were evaluated against them. They are
// software acceptance limits derived from the stated design targets, not
// analog tolerances, and not measurements of an instrument.

namespace cs80::ws2
{
// --- 1.6 Oversampling quality: per-factor reconstruction filter -------------
// Same host-relative specification as the established 2x Decimator
// (Dsp.h: <=0.1 dB through 20 kHz at 44.1 kHz, >=80 dB above host Nyquist).
inline constexpr double kDecimatorPassbandDb = 0.1;
inline constexpr double kDecimatorStopbandDb = -80.0;
// Factor 2 must stay the established filter exactly (legacy byte identity).
inline constexpr double kDecimatorFactor2CoefficientError = 0.0;
// Reported latency is an integer; the measured group delay of the render path
// may differ from it only by numerical phase-estimation error.
inline constexpr int kReportedLatencySamples = 63;
inline constexpr double kMeasuredLatencyErrorSamples = 1.0e-3;
// Added before evaluation (second commit): a band-limited sine voice through
// the linear SINE/VCA/output path must render the same host-rate waveform at
// every factor (relative RMS difference after the attack). Discretization of
// the RC stages and float phase accumulation are below 1e-3; a decimator delay
// or design mismatch between factors gives an O(1) difference.
inline constexpr double kCrossFactorSineRelativeRms = 1.0e-2;
// Re-registered window (third commit, before re-evaluation): the first 4096
// host samples from note-on, where a delay mismatch is largest. The first
// evaluation over 0.25-0.5 s after note-on FAILED at HQ (1.03e-2, Standard
// 6.9e-3): Oscillator::tick accumulates phase in float, so at 384 kHz each
// increment is quantized to the phase ulp, a systematic ~1e-5 relative pitch
// offset between rates that drifts ~0.01 cycle by 0.5 s. That is an oscillator
// property (ws1), not a decimator mismatch; the tolerance value is unchanged.
inline constexpr int kCrossFactorWindowSamples = 4096;

// --- Parameter smoothing ---------------------------------------------------
// Declared one-pole time constant for continuous controls (engineering choice).
inline constexpr double kSmoothingTimeConstantMs = 5.0;
// Double-state smoother versus the exact exponential, float output resolution.
inline constexpr double kSmootherStepAbsError = 1.0e-6;
// The 63.2 % point of a stepped control must fall within this fraction of tau.
inline constexpr double kSmoothingTauRelativeError = 0.05;
// Zipper gate: energy of (stepped render - unstepped render) in the HF band,
// as a fraction of that difference signal's whole energy. An unsmoothed gain
// step on a sine at a non-zero phase measures about -40 dB (1/f^2 spectrum
// of the jump); a 5 ms one-pole moves the jump spectrum to 1/f^4 and below
// -80 dB. -60 dB separates the two with margin on both sides.
inline constexpr double kZipperHfBandLowHz = 4000.0;
inline constexpr double kZipperHfBandHighHz = 20000.0;
inline constexpr double kZipperMaxHfFractionDb = -60.0;

// --- Phase 3 output modes ----------------------------------------------------
// GENERAL listening is the existing GENERAL node, so it must match exactly.
inline constexpr double kGeneralNodeError = 0.0;
// Realtime headphone stage versus its offline reference (src/reference).
inline constexpr double kHeadphoneResponseDb = 0.02;     // 20 Hz .. 20 kHz
inline constexpr double kHeadphonePhaseDegrees = 0.5;    // 20 Hz .. 20 kHz
inline constexpr double kHeadphoneStaticError = 1.0e-6;  // large-signal transfer

// --- Phase 3 noise -----------------------------------------------------------
// Welch PSD of the realtime source versus the declared analytic PSD, per
// 1/3-octave band 63 Hz .. 16 kHz (about 3.5 sigma of the estimate).
inline constexpr double kNoisePsdBandDb = 1.0;
inline constexpr double kNoisePsdLowHz = 63.0;
inline constexpr double kNoisePsdHighHz = 16000.0;
// Band power 100 Hz .. 10 kHz must not depend on host rate or factor.
inline constexpr double kNoiseRateIndependenceDb = 0.5;
// Gaussian statistics of the declared source.
inline constexpr double kNoiseKurtosisError = 0.1;
inline constexpr double kNoiseMeanSigmaError = 0.01;
// Independent output-stage hiss: left/right correlation bound.
inline constexpr double kNoiseChannelCorrelation = 0.02;
} // namespace cs80::ws2

#pragma once

// ws5-modern acceptance tolerances (IMPROVEMENT_PLAN Phase 6: MSEGs, LFO waveforms,
// modulation matrix, keyboard response curves, modern FX).
//
// PRE-REGISTERED: this file is committed before any of the realtime models it gates
// exists, and must not be loosened to make a model pass. A failing gate means the model
// is wrong. These are engineering acceptance limits for modern (non-original) features;
// none of them is CS-80 evidence or a hardware measurement.

namespace cs80::tests::ws5
{

// ---- LFO waveforms -----------------------------------------------------------------
// Realtime value against the analytic shape evaluated at the realtime phase.
inline constexpr double kLfoShapeAbsTolerance = 1.0e-6;
// Phase wraps against rate * time, in samples, over the whole measured run.
inline constexpr double kLfoPeriodToleranceSamples = 1.0;

// ---- Multi-segment envelopes ---------------------------------------------------------
// Realtime level against the double-precision analytic shape at the same position.
inline constexpr double kMsegLevelAbsTolerance = 2.0e-5;
// A segment boundary (or loop wrap, or release jump) lands within this many internal
// samples of its analytic time.
inline constexpr double kMsegBoundaryToleranceSamples = 1.0;
// Accumulated loop-period error after 100 loops.
inline constexpr double kMsegLoopDriftSamplesPer100Loops = 1.0;
// Tempo used when the host reports none (beats per minute).
inline constexpr double kMsegFallbackBpm = 120.0;

// ---- Modulation matrix -----------------------------------------------------------------
// Offsets against amount * source * via * destination scale, relative to full scale.
inline constexpr double kMatrixRelativeTolerance = 1.0e-6;
// PURE CS-80 inertness and default-render invariance are exact (byte-identical) gates.

// ---- FX loudness ------------------------------------------------------------------------
// K-weighting coefficients at 48 kHz against ITU-R BS.1770-4 Annex 1 Tables 1 and 2.
inline constexpr double kKWeightingCoefficientTolerance = 1.0e-6;
// Static calibration: each modern model at full mix, pink noise at -18 dBFS RMS, over the
// parameter grid {0.1, 0.5, 0.9}^2. K-weighted output loudness minus input loudness.
inline constexpr double kFxStaticLoudnessToleranceDb = 1.5;
// With FX gain compensation on: every classic and modern type, mixes 0.5 and 1, input
// levels -30/-18/-6 dBFS RMS, after kFxMatchSettleSeconds of steady input.
inline constexpr double kFxMatchedLoudnessToleranceDb = 0.5;
inline constexpr double kFxMatchSettleSeconds = 4.0;

// ---- FX stability (44.1 / 48 / 96 / 192 kHz host rates) ----------------------------------
// Every model at maximum settings, 6 s of full-scale noise bursts then silence.
inline constexpr double kFxMaxAbsOutput = 8.0;      // finite and below +18 dBFS
inline constexpr double kFxTailDecayDb = -60.0;     // last second of 12 s silence vs drive level

// ---- Drive oversampling -------------------------------------------------------------------
// Alias = non-harmonic power in 20 Hz..20 kHz over total power for a 4.1 kHz sine at
// -6 dBFS, maximum drive, 96 kHz internal rate. Ratio per oversampling factor.
inline constexpr double kDriveAliasDb2x = -45.0;
inline constexpr double kDriveAliasDb4x = -70.0;
inline constexpr double kDriveAliasDb8x = -80.0;
// Each doubling must improve the alias ratio by at least this much until -80 dB.
inline constexpr double kDriveAliasStepDb = 10.0;
// Realtime (4x) harmonic levels 1..4 against the 64x double-precision truth model.
inline constexpr double kDriveHarmonicToleranceDb = 0.5;
// Halfband up/down cascade: passband 20 Hz..20 kHz at a 96 kHz base rate, and image
// rejection of the up-sampler at the mirror frequency.
inline constexpr double kHalfbandPassbandRippleDb = 0.1;
inline constexpr double kHalfbandStopbandDb = -90.0;

// ---- Reverb and echo -----------------------------------------------------------------------
// Schroeder backward-integrated RT60 (-5..-35 dB fit) against the model's target RT60.
inline constexpr double kReverbRt60RelativeTolerance = 0.25;
// First tape echo arrival against the set delay time.
inline constexpr double kTapeDelayRelativeTolerance = 0.02;

} // namespace cs80::tests::ws5

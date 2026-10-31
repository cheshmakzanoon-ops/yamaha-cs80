#pragma once
// Pre-registered ws3-keyboard stage-gate tolerances. Mirror of
// tests/parity/tolerances-v2/ws3-keyboard.json, committed before the realtime
// models were evaluated. Do not loosen these to make a gate pass: revise the
// implementation instead and record the revision in docs/workstreams/ws3-keyboard.md.
namespace cs80::ws3_tolerances
{
// 2.1 free-running voices (realtime idle path versus per-sample truth).
// Procedure erratum 2026-10-31 (values unchanged; see the JSON): the truth is
// synchronised to the realtime state when the voice goes silent, and the
// residual window compares two production chains started from the realtime
// and the truth wake states, isolating the idle-path error from the production
// oscillator's float phase accumulation.
inline constexpr double kIdlePhaseErrorCyclesMax = 1.0e-4;
inline constexpr double kIdlePhaseErrorCyclesMaxWithVibrato = 2.0e-3;
inline constexpr double kWakeResidualDbMax = -40.0;
inline constexpr double kWakeResidualWindowSeconds = 0.020;
inline constexpr double kShortGapResidualDbMax = -90.0;
inline constexpr double kZeroWaveformDecayRatioMax = 1.0e-3;
inline constexpr double kIdleIncrementalMedianRtfMax = 0.02;
// 2.2 KAS scan and sample/hold.
inline constexpr double kDroopRelativeErrorMax = 1.0e-6;
// 2.3 keyboard contour.
inline constexpr double kContourVoltsAbsErrorMax = 1.0e-5;
inline constexpr double kCalibrationAnchorVolts = 2.76;
inline constexpr double kCalibrationAnchorToleranceVolts = 0.14;
// 2.4 touch.
inline constexpr double kTouchBHoldRelativeErrorMax = 1.0e-4;
inline constexpr double kTouchCTransientAbsErrorMax = 1.0e-3;
inline constexpr double kTouchCPoleE1Tolerance = 1.0e-3;
inline constexpr double kTouchASmoothingRelativeErrorMax = 1.0e-4;
inline constexpr double kTrg5BusVoltsAbsErrorMax = 1.0e-3;
} // namespace cs80::ws3_tolerances

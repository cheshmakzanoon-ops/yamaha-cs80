#pragma once

// Offline truth model for the per-voice key-voltage sample/hold (IMPROVEMENT_PLAN
// 2.2). NOT used by Engine. Restates the declared laws in closed form so the
// realtime model in src/core/KasScanner.h can be held to them:
//  - SH7/SH8: KI reaches the K buffer through a TC4016 bilateral switch whose
//    control is a TC4011 NAND of the voice TR and the common SUS line. In
//    electrical levels the switch conducts for TR,SUS = LL, LH, HL and holds
//    only for HH (docs/voice-continuity/CIRCUIT_REVIEW.md).
//  - Hold-node droop (declared inference): a constant leakage current I into a
//    hold capacitance C discharges the held K toward 0 V at I/C volts per
//    second, never below 0 V.
// These are declared nominal laws, not measured CS-80 behaviour.

#include <algorithm>

namespace cs80::reference::sample_hold
{
inline constexpr bool switchConducts(bool trHigh, bool susHigh) { return !(trHigh && susHigh); }

inline double heldVolts(double acquiredVolts, double leakageAmps, double capacitanceFarads, double seconds)
{
    return std::max(0.0, acquiredVolts - leakageAmps / capacitanceFarads * seconds);
}
} // namespace cs80::reference::sample_hold

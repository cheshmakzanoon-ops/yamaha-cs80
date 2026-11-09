#pragma once
// EXPERIMENTAL topology only, not a calibrated VCA/board model.
// OE55 KEC4453-6Z and foldout 002617: LO3 inverts LO1, LO4 inverts LO2.
// OE57 KEC4454-6Z and the same foldout: IC5 -> IC7/IC10, IC6 -> IC9/IC8.
// Absolute gain, polarity, loading and source revisions remain unresolved.
#include <array>

namespace cs80::experimental::oe2
{
// Dimensionless independent markers. No oscillator-to-control voltage law.
inline constexpr std::array<double, 4> relativeControls(double x, double y) noexcept
{
    return { x, y, -x, -y }; // LO1, LO2, LO3, LO4
}

// Inputs are already reconstructed upper IC5 and lower IC6 node samples.
// Gains are supplied externally in O1, O2, O3, O4 order, with no calibration here.
inline constexpr std::array<double, 2> sumBranches(const std::array<double, 2>& wet,
                                                 const std::array<double, 4>& gains) noexcept
{
    return { gains[0] * wet[0] + gains[1] * wet[1],
             gains[2] * wet[1] + gains[3] * wet[0] };
}
} // namespace cs80::experimental::oe2

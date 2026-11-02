#pragma once

namespace cs80
{
// The neutral historical state preserves the shipped on/mode parameter pair.
// Separate tab states control the existing OE1 clock and VCA laws. Applying
// both laws together is a declared inference; see CHORUS_TABS.md.
enum class ChorusTabMode { historical = 0, off, chorus, tremolo, both };
struct ChorusSwitches { bool chorus = false, tremolo = false; };
constexpr ChorusSwitches chorusSwitches(bool on, bool oldTremolo, ChorusTabMode tabs) noexcept
{
    if (!on) return {};
    switch (tabs)
    {
        case ChorusTabMode::off: return {};
        case ChorusTabMode::chorus: return {true, false};
        case ChorusTabMode::tremolo: return {false, true};
        case ChorusTabMode::both: return {true, true};
        default: return {!oldTremolo, oldTremolo};
    }
}
}

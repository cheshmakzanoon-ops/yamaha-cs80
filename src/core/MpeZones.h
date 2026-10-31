#pragma once

// MPE zone negotiation and high-resolution velocity (IMPROVEMENT_PLAN Phase 6).
// MODERN LAYER: active only while MPE is on (P_mpeOn), which PURE CS-80
// forces off. Default layout when MPE is switched on and no MPE Configuration
// Message (MCM) has arrived: one lower zone with 15 members (master channel 1,
// members 2..16), the layout the engine used before zone negotiation existed.
//
// MPE 1.0 (MMA RP-053): MCM = RPN 6 (MSB 0, LSB 6) on channel 1 (lower zone)
// or 16 (upper zone), data entry MSB = member count 0..15. A zone takes the
// channels next to its master; a new MCM shrinks the other zone if they would
// overlap; 0 members switches the zone off. RPN 0 (pitch-bend sensitivity) on
// a master sets the zone's master range, on a member the members' range.
// RPN 127/127 deselects. CC88 (MMA CA-031) is the low 7 bits of the next
// note-on velocity on that channel.

#include <array>
#include <cstdint>

namespace cs80
{
class MpeZoneLayout
{
public:
    enum class Role { none, lowerMaster, lowerMember, upperMaster, upperMember };

    void reset() noexcept
    {
        lowerMembers = 15; upperMembers = 0;
        for (auto& r : rpn) r = { 127, 127, -1 };
        lowerMasterRange = lowerMemberRange = upperMasterRange = upperMemberRange = -1.0f;
    }

    Role role(int channel) const noexcept
    {
        if (channel < 1 || channel > 16) return Role::none;
        if (lowerMembers > 0 && channel == 1) return Role::lowerMaster;
        if (upperMembers > 0 && channel == 16) return Role::upperMaster;
        if (lowerMembers > 0 && channel >= 2 && channel <= 1 + lowerMembers) return Role::lowerMember;
        if (upperMembers > 0 && channel <= 15 && channel >= 16 - upperMembers) return Role::upperMember;
        return Role::none;
    }
    bool inUpperZone(int channel) const noexcept
    {
        const auto r = role(channel);
        return r == Role::upperMaster || r == Role::upperMember;
    }
    int lowerMemberCount() const noexcept { return lowerMembers; }
    int upperMemberCount() const noexcept { return upperMembers; }
    // Negotiated ranges in semitones; negative = not negotiated (use the parameter).
    float masterRange(bool upper) const noexcept { return upper ? upperMasterRange : lowerMasterRange; }
    float memberRange(bool upper) const noexcept { return upper ? upperMemberRange : lowerMemberRange; }

    // Feed every CC; returns true if it was an RPN controller (101/100/6/38).
    bool controlChange(int channel, int controller, int value) noexcept
    {
        if (channel < 1 || channel > 16) return false;
        auto& s = rpn[static_cast<std::size_t>(channel)];
        switch (controller)
        {
            case 101: s.msb = value; s.dataMsb = -1; return true;
            case 100: s.lsb = value; s.dataMsb = -1; return true;
            case 6: s.dataMsb = value; apply(channel, s, false); return true;
            case 38: if (s.dataMsb >= 0) apply(channel, s, true, value); return true;
            default: return false;
        }
    }

private:
    struct RpnState { int msb = 127, lsb = 127, dataMsb = -1; };
    void apply(int channel, const RpnState& s, bool lsbData, int lsbValue = 0) noexcept
    {
        if (s.msb == 127 && s.lsb == 127) return; // null RPN
        if (s.msb == 0 && s.lsb == 6 && !lsbData)
        {
            const int members = s.dataMsb > 15 ? 15 : s.dataMsb;
            if (channel == 1)
            {
                lowerMembers = members;
                if (lowerMembers + upperMembers > 14) upperMembers = 14 - lowerMembers > 0 ? 14 - lowerMembers : 0;
                if (lowerMembers == 15) upperMembers = 0;
            }
            else if (channel == 16)
            {
                upperMembers = members;
                if (lowerMembers + upperMembers > 14) lowerMembers = 14 - upperMembers > 0 ? 14 - upperMembers : 0;
                if (upperMembers == 15) lowerMembers = 0;
            }
            // MPE: an MCM resets the zone's bend ranges to their defaults.
            if (channel == 1) { lowerMasterRange = -1.0f; lowerMemberRange = -1.0f; }
            if (channel == 16) { upperMasterRange = -1.0f; upperMemberRange = -1.0f; }
            return;
        }
        if (s.msb == 0 && s.lsb == 0)
        {
            const float semis = static_cast<float>(s.dataMsb) + (lsbData ? lsbValue / 100.0f : 0.0f);
            switch (role(channel))
            {
                case Role::lowerMaster: lowerMasterRange = semis; break;
                case Role::upperMaster: upperMasterRange = semis; break;
                case Role::lowerMember: lowerMemberRange = semis; break;
                case Role::upperMember: upperMemberRange = semis; break;
                default: break;
            }
        }
    }
    int lowerMembers = 15, upperMembers = 0;
    std::array<RpnState, 17> rpn {};
    float lowerMasterRange = -1.0f, lowerMemberRange = -1.0f, upperMasterRange = -1.0f, upperMemberRange = -1.0f;
};

// CC88 high-resolution velocity prefix, per MIDI channel. Interface layer.
class HighResolutionVelocity
{
public:
    void reset() noexcept { lsb.fill(-1); }
    void controlChange(int channel, int value) noexcept
    {
        if (channel >= 1 && channel <= 16) lsb[static_cast<std::size_t>(channel)] = value & 0x7f;
    }
    // Velocity for a note-on with 7-bit velocity `v7`; consumes the prefix.
    float noteOnVelocity(int channel, int v7) noexcept
    {
        // Same arithmetic as juce::MidiMessage::getFloatVelocity, bit for bit.
        if (channel < 1 || channel > 16) return static_cast<float>(v7) * (1.0f / 127.0f);
        auto& l = lsb[static_cast<std::size_t>(channel)];
        if (l < 0) return static_cast<float>(v7) * (1.0f / 127.0f);
        const float v = static_cast<float>((v7 << 7) | l) / 16383.0f;
        l = -1;
        return v;
    }

private:
    std::array<int, 17> lsb { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
};
} // namespace cs80

#pragma once
#include <array>
#include <cmath>
#include <stdexcept>

// Offline normalized SIGNAL routing only. Not charge conservation, pin voltages,
// sampling-aperture timing, or OE2 analog output. See MN3001_CLOCK_EVENT_DESIGN.md.
namespace cs80::reference::mn3001
{
enum class Phase { cp1, cp2 };
struct Packet
{
    double value = 0;
    bool present = false;
    bool operator==(const Packet&) const = default;
};
struct Output
{
    Packet out1, out2;
    double normalizedSum() const noexcept { return out1.value + out2.value; }
    bool operator==(const Output&) const = default;
};

class Channel
{
public:
    // Explicit empty/zero-signal initialization, not a physical reset pedestal.
    explicit Channel(Phase firstPhase) { reset(firstPhase); }
    void reset(Phase firstPhase)
    {
        validPhase(firstPhase);
        upper_ = {}; lower_ = {}; next_ = firstPhase;
        hasTime_ = false; lastTime_ = 0;
    }
    Output output() const noexcept { return {upper_[512], lower_[3]}; }
    void validate(double time, Phase phase, double input) const
    {
        validPhase(phase);
        if (phase != next_ || !std::isfinite(time) || !std::isfinite(input)
            || (hasTime_ && time <= lastTime_))
            throw std::invalid_argument("MN3001 packet: invalid completed-phase event");
    }
    Output step(double time, Phase phase, double input)
    {
        // Reject everything BEFORE any mutation, including unused CP1 input.
        validate(time, phase, input);
        const auto oldUpper = upper_;
        const auto oldLower = lower_;
        const int first = phase == Phase::cp1 ? 1 : 2;
        for (int stage = first; stage <= 512; stage += 2)
        {
            upper_[stage] = oldUpper[stage - 1];
            upper_[stage - 1] = {};
        }
        if (phase == Phase::cp2)
        {
            upper_[0] = {input, true}; // Front storage acquired on CP2.
            lower_[0] = oldUpper[509]; // Explicit normalized unity fork to510.
            lower_[2] = oldLower[1];   // Lower512.
            lower_[1] = {};
            lower_[3] = {};           // Terminating514 drains sensed lower513.
        }
        else
        {
            upper_[512] = {};         // Terminating513 drains sensed upper512.
            lower_[1] = oldLower[0];
            lower_[3] = oldLower[2];
            lower_[0] = {}; lower_[2] = {};
        }
        next_ = phase == Phase::cp1 ? Phase::cp2 : Phase::cp1;
        lastTime_ = time; hasTime_ = true;
        return output();
    }
    bool operator==(const Channel&) const = default;
private:
    static void validPhase(Phase phase)
    {
        if (phase != Phase::cp1 && phase != Phase::cp2)
            throw std::invalid_argument("MN3001 packet: invalid phase");
    }
    std::array<Packet, 513> upper_ {}; // Front0, common1..509, upper510..512.
    std::array<Packet, 4> lower_ {};   // Explicit lower510..513 tail.
    Phase next_ = Phase::cp2;
    bool hasTime_ = false;
    double lastTime_ = 0;
};

struct DualOutput
{
    Output a, b;
    bool operator==(const DualOutput&) const = default;
};
// Common-clock adapter; separate Channel instances allow independent clocks.
// Supplying inputA == inputB reproduces OE57's tied-input topology only.
class CommonClockDual
{
public:
    explicit CommonClockDual(Phase firstPhase) : a_(firstPhase), b_(firstPhase) {}
    void reset(Phase firstPhase)
    {
        // Construct first: invalid reset phase leaves both channels unchanged.
        const Channel empty(firstPhase);
        a_ = empty; b_ = empty;
    }
    DualOutput output() const noexcept { return {a_.output(), b_.output()}; }
    DualOutput step(double time, Phase phase, double inputA, double inputB)
    {
        a_.validate(time, phase, inputA);
        b_.validate(time, phase, inputB);
        return {a_.step(time, phase, inputA), b_.step(time, phase, inputB)};
    }
    bool operator==(const CommonClockDual&) const = default;
private:
    Channel a_, b_;
};
} // namespace cs80::reference::mn3001

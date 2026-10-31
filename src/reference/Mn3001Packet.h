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

// ---- Nonideal extension (ws4-circuits Step 3) --------------------------------------
// Same graph and event semantics as Channel, but with signal values only: every stage
// keeps whatever a transfer leaves behind, so there is no "empty" flag. Each transfer
// moves (1 - inefficiency) of the source stage into the destination and leaves the rest,
// which joins the next packet through that stage one clock period later. Input sampling
// overwrites front storage (a switched voltage sample, not added charge). The 509 fork
// keeps the normalized unity duplication of Channel. Each output pin carries a clock
// pedestal: +pinFeedthrough after a completed CP2 and -pinFeedthrough after CP1 on OUT1,
// and -(1 - mismatch) times that on OUT2 (complementary clocks). All three option values
// are DECLARED INFERENCES, not MN3001 data; zero options reproduce Channel exactly.
struct NonidealOptions
{
    double inefficiency = 0;
    double pinFeedthrough = 0;
    double feedthroughMismatch = 0;
};
struct PinOutput
{
    double out1 = 0, out2 = 0;
    double normalizedSum() const noexcept { return out1 + out2; }
};
class NonidealChannel
{
public:
    explicit NonidealChannel(Phase firstPhase, NonidealOptions options = {})
    {
        if (!(options.inefficiency >= 0 && options.inefficiency < 1)
            || !std::isfinite(options.pinFeedthrough)
            || !(options.feedthroughMismatch >= 0 && options.feedthroughMismatch <= 1))
            throw std::invalid_argument("MN3001 nonideal: invalid options");
        options_ = options;
        reset(firstPhase);
    }
    void reset(Phase firstPhase)
    {
        if (firstPhase != Phase::cp1 && firstPhase != Phase::cp2)
            throw std::invalid_argument("MN3001 nonideal: invalid phase");
        upper_ = {}; lower_ = {}; next_ = firstPhase;
        hasTime_ = false; lastTime_ = 0; last_ = Phase::cp1;
    }
    PinOutput output() const noexcept
    {
        const double clock = !hasTime_ ? 0.0 : last_ == Phase::cp2 ? 1.0 : -1.0;
        const double pedestal = options_.pinFeedthrough * clock;
        return {upper_[512] + pedestal, lower_[3] - (1.0 - options_.feedthroughMismatch) * pedestal};
    }
    PinOutput step(double time, Phase phase, double input)
    {
        if ((phase != Phase::cp1 && phase != Phase::cp2) || phase != next_ || !std::isfinite(time)
            || !std::isfinite(input) || (hasTime_ && time <= lastTime_))
            throw std::invalid_argument("MN3001 nonideal: invalid completed-phase event");
        const auto u = upper_;
        const auto l = lower_;
        const double left = options_.inefficiency, moved = 1.0 - left;
        const int first = phase == Phase::cp1 ? 1 : 2;
        for (int stage = first; stage <= 512; stage += 2)
        {
            upper_[stage] = u[stage] + moved * u[stage - 1];
            upper_[stage - 1] = left * u[stage - 1];
        }
        if (phase == Phase::cp2)
        {
            upper_[0] = input;                    // front storage samples the input
            lower_[0] = l[0] + moved * u[509];    // fork to lower510 (upper510 in the loop)
            lower_[2] = l[2] + moved * l[1];      // lower512
            lower_[1] = left * l[1];
            lower_[3] = left * l[3];              // 514 drains sensed lower513
        }
        else
        {
            upper_[512] = left * u[512];          // 513 drains sensed upper512
            lower_[1] = l[1] + moved * l[0];
            lower_[3] = l[3] + moved * l[2];
            lower_[0] = left * l[0];
            lower_[2] = left * l[2];
        }
        next_ = phase == Phase::cp1 ? Phase::cp2 : Phase::cp1;
        last_ = phase;
        lastTime_ = time; hasTime_ = true;
        return output();
    }
private:
    NonidealOptions options_;
    std::array<double, 513> upper_ {};
    std::array<double, 4> lower_ {};
    Phase next_ = Phase::cp2, last_ = Phase::cp1;
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

#pragma once

// KAS keyboard scan and SH per-voice key-voltage hold (IMPROVEMENT_PLAN 2.2).
// Circuit engine only; the Legacy policy keeps its historical allocator.
// Evidence and declared models: docs/workstreams/ws3-keyboard.md (2.2).
//
// 1. SH hold (source-derived logic). SH7/SH8: KI reaches the K buffer through
//    a TC4016 switch controlled by NAND(TR, SUS); it holds only when TR and SUS
//    are both electrically high. TR is active low (KAS4/KAS6: key-on -6.5 V),
//    SUS is pulled high in SUSTAIN I and forced low in SUSTAIN II (Panel66).
//    So a released key holds K in Sustain I and keeps tracking KI in Sustain II.
// 2. Hold droop (declared inference). The SH hold capacitance is not printed on
//    SH7; the nominal 0.033 uF is the per-voice held node drawn on the companion
//    KBC board (KEYBOARD_CONTOUR_EVIDENCE.md). The leakage is the input current
//    of the CA3140T K buffer (SH8 parts list); its data sheet gives a 10 pA
//    typical MAGNITUDE without sign, and near mid-supply the two protection-
//    diode leakages oppose. Nominal 1 pA toward 0 V is a declared choice at the
//    low end of that range (about 0.8 cent/s at C1, 0.1 cent/s at C4). Switch
//    leakage is taken as zero because KI stays at the channel's key after
//    release, so the switch sees no voltage across it. Alternatives (tested for
//    sensitivity in tests/KasScannerTests.cpp): 0 (ideal hold, the earlier
//    engine), 10-50 pA, either polarity; 0.02 uF (SH7 application note) or
//    0.22 uF (CS-50 servicing guide).
// 3. Scan timing and priority (declared inference). The YM26600 scans the
//    keyboard continuously; an event is acted on at the next scan instant, and
//    events seen in one scan are taken in scan order (lowest key first, a
//    release before a strike of the same key). The scan period is not in the
//    inspected sources; nominal 1 ms, alternatives 0.25-4 ms. Opt-in through
//    the kas_scan_timing parameter (it adds up to one period of note latency).
// 4. Ninth held key (declared inference). The YM26600 keeps an 8-entry code
//    memory, with no evident 61-key previous-scan store (IC guidebook pp15-16
//    block diagram), so a held key without a channel looks like a new key on
//    every scan and is assigned as soon as a channel frees. Alternative: an
//    edge-detecting scanner that never reconsiders it (the Legacy behaviour and
//    the owner manual p43 statement, which covers only the occupied case).
//    A reconsidered key has no strike transient, so its initial touch is zero.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace cs80::kas
{
// ---- SH hold ---------------------------------------------------------------
inline constexpr bool shHolds(bool trHigh, bool susHigh) noexcept { return trHigh && susHigh; }

inline constexpr double kHoldCapacitanceFarads = 0.033e-6;
inline constexpr double kHoldLeakageAmps = 1.0e-12;

// One voice's held K node. Holding starts from the volts acquired at release;
// the ratio K(t)/K(acquired) scales both the VCO frequency and the filter key
// voltage, which are linear in K.
struct HeldKeyNode
{
    bool holding = false;
    double acquiredVolts = 0.0, seconds = 0.0;

    void track() noexcept { holding = false; seconds = 0.0; }
    void hold(double volts) noexcept { holding = true; acquiredVolts = volts; seconds = 0.0; }
    void advance(double dt) noexcept { if (holding) seconds += dt; }
    double ratio(double leakageAmps = kHoldLeakageAmps,
                 double capacitanceFarads = kHoldCapacitanceFarads) const noexcept
    {
        if (!holding || !(acquiredVolts > 0.0)) return 1.0;
        const double volts = acquiredVolts - leakageAmps / capacitanceFarads * seconds;
        return std::max(0.0, volts) / acquiredVolts;
    }
};

// ---- Scan timing and priority -----------------------------------------------
inline constexpr double kNominalScanSeconds = 0.001;

class Scanner
{
public:
    static constexpr int kCapacity = 64;
    struct Event { int note = 60, channel = 1; float velocity = 0.0f; bool on = false; std::uint32_t order = 0; };

    void prepare(double internalRate, double scanSeconds = kNominalScanSeconds) noexcept
    {
        period = std::max(1, static_cast<int>(std::lround(internalRate * scanSeconds)));
        reset();
    }
    void reset() noexcept { count = 0; clock = 0; arrivals = 0; }
    int scanPeriodSamples() const noexcept { return period; }
    int pending() const noexcept { return count; }

    // Returns false when full; the caller then acts on the event at once.
    bool push(int note, int channel, float velocity, bool on) noexcept
    {
        if (count >= kCapacity) return false;
        events[static_cast<std::size_t>(count++)] = { note, channel, velocity, on, ++arrivals };
        return true;
    }
    // Drop queued events of one MIDI channel (0 = all), e.g. on a panic.
    void discard(int channel) noexcept
    {
        int kept = 0;
        for (int i = 0; i < count; ++i)
            if (channel != 0 && events[static_cast<std::size_t>(i)].channel != channel)
                events[static_cast<std::size_t>(kept++)] = events[static_cast<std::size_t>(i)];
        count = kept;
    }

    // One internal sample. At a scan instant, dispatch the queued events in scan
    // order. Returns true at a scan instant (also when nothing was queued).
    template <class Dispatch>
    bool tick(Dispatch&& dispatch) noexcept
    {
        if (++clock < period) return false;
        clock = 0;
        if (count > 0)
        {
            std::sort(events.begin(), events.begin() + count, [](const Event& a, const Event& b) {
                if (a.note != b.note) return a.note < b.note;
                if (a.on != b.on) return !a.on; // release before strike of the same key
                return a.order < b.order;
            });
            const int n = count;
            count = 0;
            for (int i = 0; i < n; ++i) dispatch(events[static_cast<std::size_t>(i)]);
        }
        return true;
    }

private:
    std::array<Event, kCapacity> events {};
    int count = 0, clock = 0, period = 96;
    std::uint32_t arrivals = 0;
};
} // namespace cs80::kas

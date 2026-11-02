// Numerical cache invariants, not hardware measurements. The uncached oracle
// restores the complete physical/capacitor/residual state before each sample,
// forcing a fresh analytic event search. Independent waveform, event-timing
// and spectral truth gates remain in ReconstructedOscillatorTests.cpp.
#include "core/ReconstructedOscillator.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace {
using Osc = cs80::ReconstructedOscillator;
int failures = 0;
void check(bool pass, const char* label)
{
    if (!pass) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
std::array<double, 3> pins(const cs80::WscOutputVolts& v)
{
    return { v.sawPin5, v.pulsePin7, v.sinePin2 };
}
void continuation(const ws1::Tolerances& tolerance)
{
    double worst = 0.0, worstTime = 0.0;
    bool finite = true, bounded = true, eventsAgree = true;
    for (double rate : { 88200.0, 96000.0, 192000.0, 384000.0 })
        for (double rc : { 0.001, 0.1034 })
        {
            cs80::OscillatorNominalProfile profile;
            profile.couplingSecondsNominal = rc;
            Osc cached;
            check(cached.prepare(rate, profile), "prepare cache fixture");
            cached.reset(0.317, 261.6);
            for (int n = 0; n < 4096; ++n) cached.tick(261.6);
            Osc fresh = cached;
            for (int n = 0; n < 8192; ++n)
            {
                const double hz = n < 1024 ? 261.6 : n < 1280 ? 0.0
                                : n < 4096 ? 1046.0 : 4186.0;
                cs80::WscControls controls;
                controls.pulseThresholdVolts = 0.7 * std::sin((n / 127) * 0.39);
                controls.sawEnabled = (n / 191) % 3 != 0;
                controls.pulseEnabled = (n / 193) % 4 != 0;
                if (n == 2048)
                {
                    cached.advanceIdle(0.002, hz, controls);
                    fresh.advanceIdle(0.002, hz, controls);
                }
                if (n == 3072) cached.restoreState(cached.getState());
                if (n == 6000)
                {
                    cached.restoreIdleState(cached.getIdleState());
                    fresh.restoreIdleState(fresh.getIdleState());
                }
                fresh.restoreState(fresh.getState());
                const auto a = pins(cached.tick(hz, controls));
                const auto b = pins(fresh.tick(hz, controls));
                for (std::size_t pin = 0; pin < a.size(); ++pin)
                {
                    finite = finite && std::isfinite(a[pin]) && std::isfinite(b[pin]);
                    worst = std::max(worst, std::abs(a[pin] - b[pin]));
                }
                const auto& da = cached.getDiagnostics();
                const auto& db = fresh.getDiagnostics();
                bounded = bounded && !da.eventCapacityExceeded && !db.eventCapacityExceeded;
                eventsAgree = eventsAgree && da.eventCount == db.eventCount;
                if (da.eventCount == db.eventCount)
                    for (std::size_t i = 0; i < da.eventCount; ++i)
                    {
                        eventsAgree = eventsAgree && da.events[i].kind == db.events[i].kind;
                        worstTime = std::max(worstTime,
                            std::abs(da.events[i].absoluteSeconds - db.events[i].absoluteSeconds));
                    }
            }
        }
    std::printf("cache continuation: worst pin difference %.12g V; event difference %.12g s\n",
                worst, worstTime);
    check(finite, "cached and restored continuation are finite");
    check(bounded, "no event-capacity failure after invalidation and restore");
    check(eventsAgree, "cached and restored schedules report the same events");
    check(worst <= tolerance["maximum_pin_difference_volts"], "cached continuation pin tolerance");
    check(worstTime <= tolerance["maximum_event_time_difference_seconds"], "cached continuation event-time tolerance");
}
void restoredSnapshots(const ws1::Tolerances& tolerance)
{
    double worst = 0.0;
    for (bool idle : { false, true })
    {
        Osc oscillator;
        oscillator.prepare(96000.0, {});
        oscillator.reset(0.317, 261.6);
        for (int n = 0; n < 4111; ++n) oscillator.tick(261.6);
        const auto snapshot = oscillator.getState();
        const auto idleSnapshot = oscillator.getIdleState();
        for (int n = 0; n < 97; ++n) oscillator.tick(261.6);
        Osc fresh;
        fresh.prepare(96000.0, {});
        if (idle)
        {
            oscillator.restoreIdleState(idleSnapshot);
            fresh.restoreIdleState(idleSnapshot);
        }
        else
        {
            oscillator.restoreState(snapshot);
            fresh.restoreState(snapshot);
        }
        for (int n = 0; n < 2048; ++n)
        {
            const auto a = pins(oscillator.tick(261.6));
            const auto b = pins(fresh.tick(261.6));
            for (std::size_t pin = 0; pin < a.size(); ++pin)
                worst = std::max(worst, std::abs(a[pin] - b[pin]));
        }
    }
    std::printf("cache snapshot restoration: worst pin difference %.12g V\n", worst);
    check(worst <= tolerance["maximum_pin_difference_volts"], "restoring a prior snapshot discards future-event cache");
}
void workBound(const ws1::Tolerances& tolerance)
{
    Osc oscillator;
    oscillator.prepare(96000.0, {});
    for (int n = 0; n < 4096; ++n) oscillator.tick(261.6);
    const auto before = oscillator.getDiagnostics().branchSearchCount;
    for (int n = 0; n < 8192; ++n) oscillator.tick(261.6);
    const auto searches = oscillator.getDiagnostics().branchSearchCount - before;
    std::printf("cache work: %llu analytic searches for 8192 samples (%.6f/sample)\n",
                static_cast<unsigned long long>(searches), searches / 8192.0);
    check(searches > 0, "search instrumentation remains active");
    check(searches / 8192.0 <= tolerance["maximum_searches_per_sample_at_261_6_hz"],
          "scheduler searches once per analytic segment, not once per sample");
}
}
int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tolerance(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/kernel-cache.json", "oscillator");
        continuation(tolerance);
        restoredSnapshots(tolerance);
        workBound(tolerance);
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}

// A49 source-level mechanism probe, not a native GUI reproduction or release gate.
// Uses the unmodified production mailbox and display headers. Synthetic samples
// and clocks deliberately separate their invariants from native timer delivery.
#include "plugin/OutputMeterDisplay.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr float observedPeak = 0.0834435672f;
constexpr double capturePhaseSeconds = 6.355234;
int checks = 0;
int failures = 0;

void check(bool condition, const char* description)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL: %s\n", description);
    }
}

void publishSyntheticFixture(cs80::OutputMeter& mailbox)
{
    for (int blockIndex = 0; blockIndex < 32; ++blockIndex)
    {
        std::array<float, 256> samples {};
        samples[3] = -observedPeak * static_cast<float>(blockIndex + 1) / 32.0f;
        samples[129] = samples[3] * -0.5f;
        cs80::OutputMeterBlock block;
        block.addSamples(samples.data(), static_cast<int>(samples.size()));
        mailbox.publish(block);
    }
}
}

int main()
{
    using cs80::ui::OutputMeterDisplay;
    const double initialDb = 20.0 * std::log10(static_cast<double>(observedPeak));
    const double secondsToFloor = (initialDb - OutputMeterDisplay::floorDbfs)
        / OutputMeterDisplay::releaseDbPerSecond;
    std::printf("MODELED MECHANISMS ONLY: no native window, JUCE timer, or host audio reproduction\n");
    std::printf("observed_peak=%.9g initial_dbfs=%.9f seconds_to_floor=%.9f capture_phase_seconds=%.6f\n",
                observedPeak, initialDb, secondsToFloor, capturePhaseSeconds);

    // A delayed first callback cannot age an unread mailbox peak away. The
    // display decays its previous state before accepting this new reading.
    for (int delayMs = 0; delayMs <= 2000; ++delayMs)
    {
        cs80::OutputMeter mailbox;
        OutputMeterDisplay display;
        (void) mailbox.consume(); // The editor's constructor discard.
        display.reset(0.0);
        publishSyntheticFixture(mailbox);
        display.update(mailbox.consume(), static_cast<double>(delayMs) * 0.001);
        check(display.samplePeak() == observedPeak && !display.atFloor()
                  && !display.nonFinite() && !display.overRange(),
              "any first consumption within the two-second window presents the positive peak");
    }

    // JUCE's 24 Hz timer has an integer 41 ms interval; the editor's 8 Hz
    // callback has a 125 ms interval. Both feed the same display. Enumerate
    // their initial phases, including coincident callbacks in either order.
    int schedules = 0;
    for (int meterOffset = 0; meterOffset < 41; ++meterOffset)
    {
        for (int editorOffset = 0; editorOffset < 125; ++editorOffset)
        {
            cs80::OutputMeter mailbox;
            OutputMeterDisplay display;
            display.reset(0.0);
            publishSyntheticFixture(mailbox);
            std::vector<int> callbackTimes;
            for (int ms = meterOffset; ms <= 2000; ms += 41) callbackTimes.push_back(ms);
            for (int ms = editorOffset; ms <= 2000; ms += 125) callbackTimes.push_back(ms);
            std::sort(callbackTimes.begin(), callbackTimes.end());
            for (int ms : callbackTimes)
                display.update(mailbox.consume(), static_cast<double>(ms) * 0.001);
            check(!display.atFloor() && display.peakDbfs() > -46.0f,
                  "two callbacks consuming one shared display cannot erase the peak within two seconds");
            ++schedules;
        }
    }
    std::printf("first_poll_delays_checked=2001 shared_timer_phase_schedules_checked=%d\n", schedules);

    cs80::OutputMeter mailbox;
    OutputMeterDisplay display;
    display.reset(0.0);
    publishSyntheticFixture(mailbox);
    // Withhold all callback dispatch for the modeled window. This reproduces
    // the observed symptom at the state-machine boundary, not its native cause.
    check(display.atFloor(), "no dispatched update leaves the original floor label state");
    const auto pending = mailbox.consume();
    check(pending.samplePeak == observedPeak,
          "withheld callback dispatch leaves the complete peak pending");
    std::printf("dispatch_withheld_for_modeled_2s: display_at_floor=%d pending_peak=%.9g\n",
                display.atFloor() ? 1 : 0, pending.samplePeak);
    display.update(pending, 2.0);
    check(display.samplePeak() == observedPeak && !display.atFloor(),
          "first actual update immediately recovers the pending measured peak");
    std::printf("after_first_update: dbfs=%.9g at_floor=%d\n",
                display.peakDbfs(), display.atFloor() ? 1 : 0);

    // Conservatively allow decay for the entire measured capture phase,
    // although publication actually happens partway into that phase.
    display.reset(0.0);
    display.update({observedPeak, false, false}, 0.0);
    display.update({}, capturePhaseSeconds);
    check(!display.atFloor(), "normal decay cannot reach the floor during the complete capture phase");
    std::printf("after_entire_capture_phase_decay: dbfs=%.9g at_floor=%d\n",
                display.peakDbfs(), display.atFloor() ? 1 : 0);
    std::printf("A49 mechanism checks: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

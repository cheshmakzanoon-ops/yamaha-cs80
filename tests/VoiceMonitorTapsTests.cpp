#include "plugin/VoiceMonitorTaps.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>

namespace
{
int checks = 0, failures = 0;
void check(bool condition, const char* law)
{
    ++checks;
    if (! condition) { ++failures; std::printf("FAIL: %s\n", law); }
}
bool near(float value, float reference, float tolerance)
{ return std::isfinite(value) && std::abs(value - reference) <= tolerance; }
}

int main()
{
    cs80::VoiceMonitorTaps taps;
    auto state = taps.read();
    check(!state.ranks[0].active && !state.ranks[1].active,
          "unprepared monitor publishes no stale active model tap");

    cs80::VoiceMonitorSnapshot datum;
    datum.ranks = { cs80::VoiceMonitorRank { true, 0.25f, -0.4f, 784.0f, 6500.0f },
                    cs80::VoiceMonitorRank { true, 0.75f, 0.6f, 234.0f, 12400.0f } };
    taps.publish(datum);
    state = taps.read();
    check(state.ranks[0].active && state.ranks[1].active,
          "both real rank model taps are delivered from the same callback");
    check(near(state.ranks[0].ampEnvelope, 0.25f, 2e-5f)
              && near(state.ranks[1].ampEnvelope, 0.75f, 2e-5f),
          "mailbox preserves both envelopes to better than 2e-5 normalized level");
    check(near(state.ranks[0].filterEnvelope, -0.4f, 4e-5f)
              && near(state.ranks[1].filterEnvelope, 0.6f, 4e-5f),
          "mailbox preserves signed VCF envelopes to better than 4e-5");
    check(near(state.ranks[0].hpfHz, 784.0f, 0.51f)
              && near(state.ranks[0].lpfHz, 6500.0f, 0.51f)
              && near(state.ranks[1].hpfHz, 234.0f, 0.51f)
              && near(state.ranks[1].lpfHz, 12400.0f, 0.51f),
          "mailbox preserves effective model HPF/LPF cutoffs to at most 0.51 Hz");

    // This is a source-only live monitor, not part of the DSP or host state;
    // concurrent writer/reader may skip contended UI frames, never mix ranks
    // published at different sample boundaries.
    const cs80::VoiceMonitorRank initial { true, 0.125f, 0.0f, 250.0f, 5000.0f };
    taps.publish({ { initial, initial } });
    std::atomic<bool> running { true };
    std::atomic<int> mismatch { 0 };
    std::thread writer([&]
    {
        for (int n = 0; n < 20000; ++n)
        {
            const float level = (n & 1) ? 0.125f : 0.875f;
            const cs80::VoiceMonitorRank r { true, level, 0.0f, 250.0f, 5000.0f };
            taps.publish({ { r, r } });
        }
        running.store(false, std::memory_order_release);
    });
    int polled = 0;
    do
    {
        const auto polledState = taps.read();
        if (polledState.ranks[0].active != polledState.ranks[1].active
            || (polledState.ranks[0].active
                && polledState.ranks[0].ampEnvelope != polledState.ranks[1].ampEnvelope))
            mismatch.fetch_add(1, std::memory_order_relaxed);
        ++polled;
    } while (running.load(std::memory_order_acquire));
    writer.join();
    check(polled > 0 && mismatch.load(std::memory_order_relaxed) == 0,
          "concurrent UI reads never mix two rank taps from different audio callbacks");
    taps.reset();
    state = taps.read();
    check(!state.ranks[0].active && !state.ranks[1].active,
          "transport reset clears displayed engine taps without touching DSP");

    std::printf("Voice monitor atomic taps: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

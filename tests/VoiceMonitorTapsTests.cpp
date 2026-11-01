#include "plugin/VoiceMonitorTaps.h"

#include <atomic>
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
}

int main()
{
    cs80::VoiceMonitorTaps taps;
    auto state = taps.read();
    check(!state.ranks[0].active && !state.ranks[1].active,
          "unprepared monitor publishes no stale active model tap");

    cs80::VoiceMonitorSnapshot datum;
    // Model filter envelopes can exceed +/-1 (the actual Engine host oracle
    // exposed -1.41807). A live tap must not clamp or invent that evidence.
    datum.ranks = { cs80::VoiceMonitorRank { true, 0.25f, -1.41807f, 40700.0f, 6500.0f },
                    cs80::VoiceMonitorRank { true, 0.75f, 1.42182f, 234.0f, 51000.0f } };
    taps.publish(datum);
    state = taps.read();
    check(state.ranks[0].active && state.ranks[1].active,
          "both real rank model taps are delivered from the same callback");
    check(state.ranks[0].ampEnvelope == datum.ranks[0].ampEnvelope
              && state.ranks[1].ampEnvelope == datum.ranks[1].ampEnvelope,
          "mailbox preserves both VCA model envelope floats bit-exactly");
    check(state.ranks[0].filterEnvelope == datum.ranks[0].filterEnvelope
              && state.ranks[1].filterEnvelope == datum.ranks[1].filterEnvelope,
          "mailbox preserves signed VCF model envelopes beyond +/-1 bit-exactly");
    check(state.ranks[0].hpfHz == datum.ranks[0].hpfHz
              && state.ranks[0].lpfHz == datum.ranks[0].lpfHz
              && state.ranks[1].hpfHz == datum.ranks[1].hpfHz
              && state.ranks[1].lpfHz == datum.ranks[1].lpfHz,
          "mailbox preserves full-range effective HPF/LPF model cutoffs bit-exactly");

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

#pragma once
#include "PumpUntil.h"
#include <juce_events/juce_events.h>
namespace cs80::tests {
template<class Ready> bool waitForTimer(Ready ready, int timeoutMs=2000) {
    return pumpUntil(ready, [](int ms) {
        juce::Thread::sleep(ms);
        juce::Timer::callPendingTimersSynchronously();
    }, [] { return std::chrono::steady_clock::now(); }, timeoutMs);
}
// Observe actual timer-thread progress, not just elapsed wall time. Existing
// processor/mount timers have periods <=125ms and therefore precede this barrier.
inline bool waitForTimerCycle() {
    struct Barrier final : juce::Timer {
        bool fired=false;
        Barrier() { startTimer(130); }
        void timerCallback() override { stopTimer(); fired=true; }
    } barrier;
    return waitForTimer([&] { return barrier.fired; });
}
}

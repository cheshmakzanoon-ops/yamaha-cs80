#include "plugin/CapturedKeyboardEvents.h"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition, const char* description)
{
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

// The audio callback constructs one immutable, self-contained snapshot. Check its
// live prefix, not unused slots (which intentionally contain no Event objects).
void testCapturedBatch()
{
    using Queue = cs80::KeyboardEventQueue<4>;
    Queue queue;
    const cs80::CapturedKeyboardEvents empty(queue);
    require(empty.size() == 0 && empty.samplePosition(0, 64) == 0,
            "empty capture has no live entries");

    cs80::KeyPressureToken token;
    require(queue.push(1, 60, 0.8f, true, 0xfffffffeu, &token) && token, "timestamp-wrap onset");
    require(queue.pushPressure(token, 0.25f, 1), "timestamp-wrap pressure");
    require(queue.push(1, 60, 0, false, 0), "clock-regressed release");
    require(queue.push(2, 72, 0, false, 2), "fourth queued event");
    const cs80::CapturedKeyboardEvents full(queue);
    require(full.size() == 4, "all slots may be live");

    // Refill and retire the same slots before inspecting the first snapshot.
    // This guards against accidentally lending queue storage to the callback.
    for (int note = 0; note < 4; ++note)
        require(queue.push(16, note, 0.5f, true, 10), "ring slots reusable after capture");
    require(!queue.push(16, 8, 0, false, 11) && queue.hasOverflow(), "refill overflow latches");
    queue.requestReset();
    queue.acknowledge(queue.requestedEpoch());

    require(full[0].on && !full[0].pressure && full[0].channel == 1 && full[0].note == 60
            && full[0].velocity == 0.8f && full[0].token == token, "onset snapshot survives refill");
    require(full[1].pressure && full[1].token == token && full[1].velocity == 0.25f,
            "pressure identity and value survive refill");
    require(!full[2].on && !full[2].pressure && !full[2].token && full[2].milliseconds == 1
            && full[3].channel == 2 && full[3].note == 72, "release ordering and defaults retained");
    require(full.samplePosition(0, 64) == 0 && full.samplePosition(1, 64) == 38
            && full.samplePosition(2, 64) == 38 && full.samplePosition(3, 64) == 51
            && full.samplePosition(1, 0) == 0, "wrapped and zero-frame sample maps");
    require(!queue.pushPressure(token, 0.5f, 12)
            && cs80::CapturedKeyboardEvents(queue).size() == 0, "old epoch cannot populate a new capture");
}
}

int main()
{
    testCapturedBatch();
    std::cout << "Captured keyboard events passed\n";
}

#include "tools/EventTimeline.h"

#include <cstdio>
#include <vector>

namespace
{
struct Event { int frame; int value; };
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", message); }
}

void exercise(const std::vector<Event>& events, int frames, int block)
{
    int position = 0, value = 0;
    std::size_t dispatched = 0;
    std::vector<int> actual;
    cs80::tools::renderEventTimeline(frames, block, events,
        [&](const Event& e) {
            check(position == std::max(0, e.frame), "event delivered at exact frame");
            check(e.value == events[dispatched++].value, "simultaneous event order preserved");
            value = e.value;
        },
        [&](int start, int count) {
            check(start == position && count > 0 && count <= block, "contiguous bounded render blocks");
            actual.insert(actual.end(), count, value);
            position += count;
        });
    std::vector<int> expected;
    std::size_t next = 0;
    value = 0;
    for (int i = 0; i < frames; ++i)
    {
        while (next < events.size() && events[next].frame <= i)
            value = events[next++].value;
        expected.push_back(value);
    }
    check(actual == expected, "output matches independent single-sample scheduler");
    check(dispatched == next && position == frames, "no extra events or frames");
}
}

int main()
{
    for (int block : { 1, 7, 64, 128, 257 })
    {
        exercise({}, 0, block);
        exercise({}, 513, block);
        exercise({{0, 1}, {1, 2}, {127, 3}, {128, 4}, {129, 5},
                  {129, 6}, {255, 7}, {512, 8}, {513, 9}, {1000, 10}}, 513, block);
        exercise({{-1, 1}, {0, 2}, {1, 3}}, 0, block);
        exercise({{-1, 1}, {0, 2}, {1, 3}}, 2, block);
        // First real demo chord at 0.35 s / 48 kHz: previously delivered at
        // frame 16896 instead of 16800 with the default 128-frame block.
        exercise({{16800, 1}, {16800, 2}, {16801, 3}}, 17000, block);
        std::vector<Event> dense;
        for (int i = 0; i < 2048; ++i) dense.push_back({i, i + 1});
        exercise(dense, 2048, block);
    }
    if (failures) return 1;
    std::puts("offline event timeline: exact timestamps and sample-reference output passed");
}

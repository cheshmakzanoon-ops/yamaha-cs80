#pragma once

#include <algorithm>
#include <cstddef>

namespace cs80::tools
{
// Offline renderer only. Events must be sorted by frame; maxBlock must be
// positive. Dispatch before rendering the event's sample, not at the next
// block boundary. Events at/after frames do not affect the requested output.
template <typename Events, typename Dispatch, typename Render>
void renderEventTimeline(int frames, int maxBlock, const Events& events,
                         Dispatch dispatch, Render render)
{
    std::size_t nextEvent = 0;
    int frame = 0;
    while (frame < frames)
    {
        while (nextEvent < events.size() && events[nextEvent].frame <= frame)
            dispatch(events[nextEvent++]);
        int n = std::min(maxBlock, frames - frame);
        if (nextEvent < events.size() && events[nextEvent].frame < frames)
            n = std::min(n, events[nextEvent].frame - frame);
        render(frame, n);
        frame += n;
    }
}
} // namespace cs80::tools

#pragma once
#include "KeyboardEventQueue.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace cs80
{
// A callback-local snapshot. Only the accepted prefix has a live Event; no
// initialization of up to Capacity unused Events is needed on the audio thread.
template <std::size_t Capacity>
class CapturedKeyboardEvents
{
    using Queue = KeyboardEventQueue<Capacity>;
    using Event = typename Queue::Event;
    static_assert(std::is_trivially_destructible_v<Event>
                  && std::is_nothrow_copy_constructible_v<Event>);

    union Slot
    {
        Event value;
        Slot() noexcept {} // No active member until the queue accepts this index.
        void operator=(const Event& source) noexcept { std::construct_at(&value, source); }
    };

    std::array<Slot, Capacity> slots;
    const std::size_t count;

public:
    explicit CapturedKeyboardEvents(Queue& queue) noexcept : count(queue.takeInto(slots)) {}
    CapturedKeyboardEvents(const CapturedKeyboardEvents&) = delete;
    CapturedKeyboardEvents& operator=(const CapturedKeyboardEvents&) = delete;

    std::size_t size() const noexcept { return count; }
    const Event& operator[](std::size_t index) const noexcept
    {
        assert(index < count);
        return slots[index].value;
    }
    int samplePosition(std::size_t index, int frames) const noexcept
    {
        return Queue::samplePositionOf(*this, count, index, frames);
    }
};
} // namespace cs80

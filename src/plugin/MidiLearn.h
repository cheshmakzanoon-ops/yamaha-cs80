#pragma once

// Per-session MIDI learn map: MIDI control change number -> host parameter index.
//
// The audio thread reads and (while learning) writes the map with atomics only: no locks, no
// allocation, bounded loops. The message thread arms learning, edits and clears mappings, and
// serialises the map into the plugin state (PluginProcessor). One controller drives at most one
// parameter and one parameter is driven by at most one controller: learning a parameter again
// moves it to the new controller.
//
// MIDI learn is a modern feature: PluginProcessor neither learns nor applies mappings in
// PURE CS-80, where the original's MIDI-free control surface is the reference.
//
// Plain C++ (no JUCE), so the core test suite covers it.

#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace cs80
{

class MidiLearnMap
{
public:
    static constexpr int kNumControllers = 128;
    static constexpr int kNone = -1;
    // Format version of the serialised map (the MIDI_LEARN element of the plugin state).
    static constexpr int kStateVersion = 1;

    // Callback must be allocation-free and non-blocking: learning can invoke it on audio.
    // Its context must outlive this map; no callback is made for transient learning state.
    using ChangeCallback = void (*)(void*) noexcept;
    explicit MidiLearnMap(void* context = nullptr, ChangeCallback changed = nullptr) noexcept
        : changeContext(context), onChange(changed)
    {
        for (auto& slot : map)
            slot.store(kNone, std::memory_order_relaxed);
    }

    // Controllers that carry protocol meaning are never learnable: bank select (0, 32), data entry
    // (6, 38), data increment/decrement and NRPN/RPN selection (96-101, used for MPE and pitch-bend
    // range negotiation) and the channel-mode messages (120-127).
    static constexpr bool isLearnable(int cc) noexcept
    {
        return cc > 0 && cc < 120 && cc != 6 && cc != 32 && cc != 38 && ! (cc >= 96 && cc <= 101);
    }

    // ---- message thread ------------------------------------------------------------------
    // The next learnable controller that arrives is assigned to `parameter`.
    void armLearn(int parameter) noexcept
    {
        learn.store(parameter >= 0 ? parameter : kNone, std::memory_order_release);
        bump();
    }
    void cancelLearn() noexcept
    {
        learn.store(kNone, std::memory_order_release);
        bump();
    }
    int learningParameter() const noexcept { return learn.load(std::memory_order_acquire); }

    // Assigns `cc` to `parameter`, removing the parameter's previous controller. False for an
    // unlearnable controller or a negative parameter. notify=false is reserved for host restore.
    bool assign(int cc, int parameter, bool notify = true) noexcept
    {
        if (! isLearnable(cc) || parameter < 0)
            return false;
        const int removed = release(parameter, cc);
        const int previous = map[(std::size_t) cc].exchange(parameter, std::memory_order_acq_rel);
        bump();
        if (notify && (removed != 0 || previous != parameter))
            changed();
        return true;
    }

    void clearController(int cc) noexcept
    {
        if (cc < 0 || cc >= kNumControllers)
            return;
        const int previous = map[(std::size_t) cc].exchange(kNone, std::memory_order_acq_rel);
        bump();
        if (previous != kNone)
            changed();
    }

    // Removes every controller assigned to `parameter`; returns how many there were.
    int clearParameter(int parameter) noexcept
    {
        const int removed = parameter >= 0 ? release(parameter, kNone) : 0;
        bump();
        if (removed != 0)
            changed();
        return removed;
    }

    void clearAll(bool notify = true) noexcept
    {
        bool removed = false;
        for (auto& slot : map)
            removed |= slot.exchange(kNone, std::memory_order_acq_rel) != kNone;
        learn.store(kNone, std::memory_order_release);
        bump();
        if (notify && removed)
            changed();
    }

    // ---- any thread ----------------------------------------------------------------------
    int parameterFor(int cc) const noexcept
    {
        return cc >= 0 && cc < kNumControllers ? map[(std::size_t) cc].load(std::memory_order_acquire) : kNone;
    }

    int controllerFor(int parameter) const noexcept
    {
        if (parameter < 0)
            return kNone;
        for (int cc = 0; cc < kNumControllers; ++cc)
            if (map[(std::size_t) cc].load(std::memory_order_acquire) == parameter)
                return cc;
        return kNone;
    }

    int count() const noexcept
    {
        int n = 0;
        for (const auto& slot : map)
            n += slot.load(std::memory_order_relaxed) >= 0 ? 1 : 0;
        return n;
    }

    // Increments on every change, so a display can poll for edits cheaply.
    std::uint32_t generation() const noexcept { return gen.load(std::memory_order_acquire); }

    // ---- audio thread --------------------------------------------------------------------
    // The parameter a control change drives, learning it first if learning is armed. kNone when
    // the controller is unassigned (it then keeps its ordinary MIDI meaning).
    int handleController(int cc) noexcept
    {
        if (cc < 0 || cc >= kNumControllers)
            return kNone;
        if (isLearnable(cc))
        {
            int armed = learn.load(std::memory_order_acquire);
            if (armed >= 0 && learn.compare_exchange_strong(armed, kNone, std::memory_order_acq_rel))
            {
                const int removed = release(armed, cc);
                const int previous = map[(std::size_t) cc].exchange(armed, std::memory_order_acq_rel);
                bump();
                if (removed != 0 || previous != armed)
                    changed();
                return armed;
            }
        }
        return map[(std::size_t) cc].load(std::memory_order_acquire);
    }

private:
    // Unassigns every controller except `keep` from `parameter`.
    int release(int parameter, int keep) noexcept
    {
        int removed = 0;
        for (int cc = 0; cc < kNumControllers; ++cc)
        {
            if (cc == keep)
                continue;
            int expected = parameter;
            if (map[(std::size_t) cc].compare_exchange_strong(expected, kNone, std::memory_order_acq_rel))
                ++removed;
        }
        return removed;
    }

    void changed() noexcept { if (onChange != nullptr) onChange(changeContext); }

    void* const changeContext;
    const ChangeCallback onChange;

    void bump() noexcept { gen.fetch_add(1, std::memory_order_acq_rel); }

    std::array<std::atomic<int>, kNumControllers> map;
    std::atomic<int> learn { kNone };
    std::atomic<std::uint32_t> gen { 0 };

    static_assert(std::atomic<int>::is_always_lock_free, "the MIDI learn map must be lock-free on the audio thread");
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free, "the MIDI learn generation must be lock-free");
};

} // namespace cs80

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

namespace cs80
{

class MidiLearnMap
{
public:
    static constexpr int kNumControllers = 128;
    static constexpr int kNone = -1;
    // Format version of the serialised map (the MIDI_LEARN element of the plugin state).
    static constexpr int kStateVersion = 1;

    MidiLearnMap() noexcept
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
    // unlearnable controller or a negative parameter.
    bool assign(int cc, int parameter) noexcept
    {
        if (! isLearnable(cc) || parameter < 0)
            return false;
        release(parameter, cc);
        map[(size_t) cc].store(parameter, std::memory_order_release);
        bump();
        return true;
    }

    void clearController(int cc) noexcept
    {
        if (cc < 0 || cc >= kNumControllers)
            return;
        map[(size_t) cc].store(kNone, std::memory_order_release);
        bump();
    }

    // Removes every controller assigned to `parameter`; returns how many there were.
    int clearParameter(int parameter) noexcept
    {
        const int removed = release(parameter, kNone);
        bump();
        return removed;
    }

    void clearAll() noexcept
    {
        for (auto& slot : map)
            slot.store(kNone, std::memory_order_release);
        learn.store(kNone, std::memory_order_release);
        bump();
    }

    // ---- any thread ----------------------------------------------------------------------
    int parameterFor(int cc) const noexcept
    {
        return cc >= 0 && cc < kNumControllers ? map[(size_t) cc].load(std::memory_order_acquire) : kNone;
    }

    int controllerFor(int parameter) const noexcept
    {
        if (parameter < 0)
            return kNone;
        for (int cc = 0; cc < kNumControllers; ++cc)
            if (map[(size_t) cc].load(std::memory_order_acquire) == parameter)
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
                release(armed, cc);
                map[(size_t) cc].store(armed, std::memory_order_release);
                bump();
                return armed;
            }
        }
        return map[(size_t) cc].load(std::memory_order_acquire);
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
            if (map[(size_t) cc].compare_exchange_strong(expected, kNone, std::memory_order_acq_rel))
                ++removed;
        }
        return removed;
    }

    void bump() noexcept { gen.fetch_add(1, std::memory_order_acq_rel); }

    std::array<std::atomic<int>, kNumControllers> map;
    std::atomic<int> learn { kNone };
    std::atomic<std::uint32_t> gen { 0 };

    static_assert(std::atomic<int>::is_always_lock_free, "the MIDI learn map must be lock-free on the audio thread");
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free, "the MIDI learn generation must be lock-free");
};

} // namespace cs80

#pragma once

#include "core/Engine.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace cs80
{
struct VoiceMonitorFrame
{
    struct Voice
    {
        Engine::VoiceInfo info;
        std::array<Engine::RankInfo, 2> ranks {};
    };
    std::array<Voice, 16> voices {};
    std::array<int, 2> sources { 13, 13 };
    int capacity = 8;
    std::uint64_t serial = 0;
};

// Single audio writer, message-thread readers. Every payload word is atomic, so even a
// rejected concurrent read has no C++ data race. The version gives a coherent frame. The
// writer never retries or waits; a busy reader keeps its previous display after three tries.
// Only read-only diagnostics pass here. This storage is never read by the audio engine.
class VoiceMonitor
{
public:
    void publish(const VoiceMonitorFrame& frame) noexcept
    {
        std::array<std::uint32_t, wordCount> words {};
        std::memcpy(words.data(), &frame, sizeof(frame));
        const auto next = version.load(std::memory_order_relaxed) + 1;
        version.store(next, std::memory_order_seq_cst);
        for (std::size_t i = 0; i < wordCount; ++i) data[i].store(words[i], std::memory_order_seq_cst);
        version.store(next + 1, std::memory_order_seq_cst);
    }

    bool read(VoiceMonitorFrame& frame) const noexcept
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const auto before = version.load(std::memory_order_seq_cst);
            if ((before & 1u) != 0) continue;
            std::array<std::uint32_t, wordCount> words {};
            for (std::size_t i = 0; i < wordCount; ++i) words[i] = data[i].load(std::memory_order_seq_cst);
            if (before != version.load(std::memory_order_seq_cst)) continue;
            if (before == 0) return false;
            std::memcpy(&frame, words.data(), sizeof(frame));
            return true;
        }
        return false;
    }

private:
    static_assert(std::is_trivially_copyable_v<VoiceMonitorFrame>);
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static constexpr std::size_t wordCount = (sizeof(VoiceMonitorFrame) + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t);
    std::array<std::atomic<std::uint32_t>, wordCount> data {};
    std::atomic<std::uint32_t> version { 0 };
};
} // namespace cs80

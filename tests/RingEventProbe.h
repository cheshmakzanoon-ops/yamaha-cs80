#pragma once
// Test-only global/rank continuation inspection. Engine and Adsr grant this
// isolated friend no runtime implementation, storage or audio behavior.
#include "core/Engine.h"
#include <array>
#include <cstdint>

namespace cs80 {
struct EngineEventContinuityProbe
{
    static constexpr int voiceCount = Engine::kNumVoices;
    struct Envelope
    {
        Adsr::State visible;
        double charge = 0.0, elapsed = 0.0, attackOrigin = 0.0;
    };
    struct Ring
    {
        float phase = 0.0f, output = 0.0f, frequency = 0.0f;
        bool keyDown = false;
        Envelope charge;
        Adsr saved;
    };
    static Envelope envelope(const Adsr& e) noexcept
    {
        return { e.getState(), e.preciseValue, e.preciseTime, e.attackFrom };
    }
    static Ring ring(const Engine& e) noexcept
    {
        return { e.ringPhase, e.ringEnv, e.ringFrequency, e.ringKeyDown,
                 envelope(e.ringAdsr), e.ringAdsr };
    }
    static Envelope rank(const Engine& e, int voice, int rank) noexcept
    {
        return envelope(e.voices[static_cast<std::size_t>(voice)].ranks[rank].aenv);
    }
    static void retainedRingReference(Engine& e, const Ring& before,
                                      bool retainPhase = true, bool retainCharge = true) noexcept
    {
        if (retainPhase) e.ringPhase = before.phase;
        if (retainCharge)
        {
            e.ringAdsr = before.saved;
            // The reference deliberately calls the unchanged default path,
            // then implements the registered state law independently.
            e.ringAdsr.noteOn(e.global.ringAttack, e.global.ringDecay, 0.0f, 0.35f);
            e.ringAdsr.preciseValue = before.charge.charge;
            e.ringAdsr.attackFrom = before.charge.charge;
            e.ringAdsr.value = before.charge.visible.value;
            e.ringEnv = before.output;
        }
    }
    static void historicalRingReset(Engine& e) noexcept
    {
        e.ringPhase = 0.0f;
        e.ringEnv = 0.0f;
        e.ringAdsr.reset();
        e.ringAdsr.noteOn(e.global.ringAttack, e.global.ringDecay, 0.0f, 0.35f);
    }
    static std::array<std::uint64_t, Engine::kNumVoices> assignments(const Engine& e) noexcept
    {
        std::array<std::uint64_t, Engine::kNumVoices> result {};
        for (int voice = 0; voice < Engine::kNumVoices; ++voice)
            result[static_cast<std::size_t>(voice)] = e.voices[static_cast<std::size_t>(voice)].age;
        return result;
    }
    static void historicalRankPrecision(Engine& e,
        const std::array<std::uint64_t, Engine::kNumVoices>& priorAssignments) noexcept
    {
        for (int voice = 0; voice < Engine::kNumVoices; ++voice)
        {
            auto& v = e.voices[static_cast<std::size_t>(voice)];
            if (v.age == priorAssignments[static_cast<std::size_t>(voice)]) continue;
            for (auto& r : v.ranks)
                r.aenv.attackFrom = r.aenv.preciseValue = static_cast<double>(r.aenv.value);
        }
    }
};
} // namespace cs80

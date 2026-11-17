#pragma once
// Qualification driver only. Never include/link this in training or cost binaries.
#include "core/Engine.h"
#include <array>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <xmmintrin.h>

namespace strict_pgo_research {
inline std::ofstream pcm, fp;
inline std::uint64_t calls = 0, frames = 0;
inline void render(cs80::Engine& engine, float* audio, int count) {
    if ((_mm_getcsr() & 0x1f80u) != 0x1f80u)
        throw std::runtime_error("qualification requires masked SSE exceptions");
    std::fenv_t saved;
    if (std::fegetenv(&saved) != 0) throw std::runtime_error("cannot save FP environment");
    const int savedErrno = errno;
    std::feclearexcept(FE_ALL_EXCEPT);
    const unsigned seed = static_cast<unsigned>(calls % 3);
    if (seed == 1) std::feraiseexcept(FE_INVALID);
    if (seed == 2) std::feraiseexcept(FE_INEXACT | FE_UNDERFLOW);
    errno = 17;
    try {
        engine.render(audio, count);
    } catch (...) {
        std::fesetenv(&saved); errno = savedErrno; throw;
    }
    unsigned short status, control;
    asm volatile("fnstsw %0" : "=am"(status));
    asm volatile("fnstcw %0" : "=m"(control));
    const std::array<std::uint32_t, 6> record {
        static_cast<std::uint32_t>(count), _mm_getcsr(), status, control,
        static_cast<std::uint32_t>(errno), seed
    };
    if (std::fesetenv(&saved) != 0) throw std::runtime_error("cannot restore FP environment");
    errno = savedErrno;
    pcm.write(reinterpret_cast<const char*>(audio), std::streamsize(count) * 2 * sizeof(float));
    fp.write(reinterpret_cast<const char*>(record.data()), sizeof(record));
    if (!pcm || !fp) throw std::runtime_error("qualification transcript write failed");
    ++calls; frames += static_cast<unsigned>(count);
}
} // namespace strict_pgo_research

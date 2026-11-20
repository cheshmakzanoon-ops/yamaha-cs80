// Directed integration diagnostic. Only this TU is compiled with
// -fno-access-control; all shipping core objects retain their normal flags.
// Compare the emitted observations with the identical executable linked to
// the previous Engine. Never compare indeterminate object padding.
#include "core/Engine.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <xmmintrin.h>
using namespace cs80;
namespace {
void put(FILE* out, const void* p, std::size_t bytes) { if (std::fwrite(p, 1, bytes, out) != bytes) std::abort(); }
template<class T> void put(FILE* out, const T& v) { put(out, &v, sizeof v); }
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto* out = std::fopen(argv[1], "wbx"); if (!out) return 2;
    Engine e;
    e.prepare(48000);
    ControlFrame f;
    f.hardwareSources = {0, 3}; f.effectiveValues[P_parityMode] = 0; f.effectiveValues[P_sourcePerformance] = 1;
    if (!e.setControlFrame(f) || !e.sourceActive[0] || !e.sourceActive[1]) return 3;
    const auto tones = e.sourceTones; const auto loads = e.sourceBoundaries;
    constexpr float nan = std::bit_cast<float>(0x7fc00017u);
    constexpr double bad = std::bit_cast<double>(0x7ff000000000000bull);
    const std::array<int,4> rounding{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    using C = source_factory::CutoffReadout; using S = source_factory::SharedReadout;
    unsigned count = 0;
    for (int r = 0; r < 4; ++r) for (int mode = 0; mode < 4; ++mode)
    for (int previous = 0; previous < 2; ++previous) for (int active = 0; active < 4; ++active)
    for (int lane = 0; lane < 8; ++lane) for (int rank = 0; rank < 2; ++rank)
    for (int kind = 0; kind < 10; ++kind) {
        std::fesetenv(FE_DFL_ENV);
        e.sourceBoundaries = loads; e.sourceTones = tones;
        e.packetTonePrechecked = {true,true};
        e.sourceActive[0] = active & 1; e.sourceActive[1] = active & 2;
        e.global.resonance = .37f;
        for (int v = 0; v < 8; ++v) {
            e.voices[v].touchB = float(8-v)*.11f; e.voices[v].touchA = float(v)*.13f;
            for(int k=0;k<2;++k) {
                auto& env=e.voices[v].ranks[k].fenv; env.reset(); env.preciseValue=env.value=float(v)*.21f;
            }
        }
        // Material previous publication whose zero mask must not certify a
        // failed/inactive rank; also test a stale nonzero mask before acceptance.
        e.sourceCutoff[0].saturatedVoices=e.sourceCutoff[1].saturatedVoices=previous ? 0x24u : 0u;
        e.sourceShared.masterResVolts=-19.25; e.sourceShared.saturatedVoices=0x5a5au;
        auto& env=e.voices[lane].ranks[rank].fenv;
        switch (kind) {
        case 1: env.preciseValue=env.value=3.5f; break;
        case 2: env.preciseValue=env.value=nan; break;
        case 3: e.voices[lane].touchB=1.4f; break;
        case 4: e.voices[lane].touchA=nan; break;
        case 5: e.sourceTones[rank].compiled=false; e.packetTonePrechecked[rank]=false; break;
        case 6: // stale zero + cutoff failure + a drive clipped exclusively in the failing rank
            e.sourceTones[rank].cutoff[0].pinPerTouch=bad; e.packetTonePrechecked[rank]=false;
            env.preciseValue=env.value=-3.25f; break;
        case 7: e.sourceBoundaries[rank].level.volts=bad; break;
        case 8: e.global.resonance=nan; break;
        case 9: e.voices[lane].touchB=-0.f; e.voices[lane].touchA=std::numeric_limits<float>::denorm_min(); break;
        }
        std::fesetround(rounding[r]);
        _mm_setcsr((_mm_getcsr() & ~0x3fu) | (mode & 1 ? 0x8000u : 0u)
                   | (mode & 2 ? 0x40u : 0u) | ((lane + 11*kind + rank) & 0x3fu));
        errno=EDOM; e.updateSourceReadouts(96000.f);
        const auto csr=_mm_getcsr(); unsigned short cw, sw;
        asm volatile("fnstcw %0; fnstsw %1" : "=m"(cw), "=m"(sw) :: "memory");
        const int error=errno;
        put(out, csr); put(out, cw); put(out, sw); put(out, error);
        put(out, e.sourceFilterValues);
        for (const auto& cutoff:e.sourceCutoff)
            put(out, &cutoff, offsetof(C,saturatedVoices)+sizeof(cutoff.saturatedVoices));
        put(out, &e.sourceShared, offsetof(S,masterResSaturated)+sizeof(e.sourceShared.masterResSaturated));
        ++count;
    }
    std::printf("%u readout handoffs\n", count);
    return std::fclose(out) != 0;
}

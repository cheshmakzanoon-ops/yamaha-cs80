// Isolated old/direct publication differential. Both instantiations use the
// same numerical evaluator; the engine alone opts into direct publication.
#include "core/UniformSourceCutoff.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <xmmintrin.h>

namespace {
namespace sf = cs80::source_factory;
template<bool Direct>
[[gnu::noipa]] bool evaluate(const sf::ToneControls& t, const sf::RankDrives& d,
                             sf::CutoffReadout& o, bool& uniform, bool prechecked) {
    return sf::uniform_cutoff::detail::evaluateForEngine<Direct>(t, d, o, &uniform, prechecked);
}
struct Words {
    std::uint32_t state{0x828d1bu};
    std::uint32_t next() { state ^= state << 13; state ^= state >> 17; return state ^= state << 5; }
    double unit() { return static_cast<double>(next()) * 0x1p-32; }
    double edge() {
        constexpr double tiny = std::numeric_limits<double>::denorm_min();
        constexpr std::array<double,18> values{0., -0., tiny, -tiny, 1., -1., 3., -3., .67,
            2.7, -2.5, -1e308, 1e308, 1e8, __builtin_inf(), -__builtin_inf(),
            std::bit_cast<double>(0x7ff0000000000009ULL), std::bit_cast<double>(0x7ff8000000000011ULL)};
        return values[next() % values.size()];
    }
};
struct Fp { unsigned csr; unsigned short cw, sw; int err; };
Fp observe() {
    Fp result{_mm_getcsr(), 0, 0, errno};
    asm volatile("fnstcw %0; fnstsw %1" : "=m"(result.cw), "=m"(result.sw) :: "memory");
    return result;
}
void reset(unsigned csr) { std::fesetenv(FE_DFL_ENV); _mm_setcsr(csr); errno = EDOM; }
}

int main() {
    std::array<sf::ToneControls,22> factory;
    for (unsigned i=0; i<factory.size(); ++i)
        if (!sf::compileTone(i % 2, i / 2, factory[i])) return 2;
    Words rng;
    unsigned accepted = 0, uniformEntries = 0;
    for (unsigned i=0; i<65536; ++i) {
        reset(0x1f80);
        auto t = factory[i % factory.size()];
        sf::VoiceDrives v{2*rng.unit()-1., rng.unit(), rng.unit()};
        if (i % 3 == 0) {
            std::array<double*,3> fields{&v.filterEnvelopeZ, &v.velocity, &v.pressure};
            *fields[rng.next() % fields.size()] = rng.edge();
        }
        sf::RankDrives d; d.fill(v);
        if (i % 7 == 0) d.back().velocity = rng.unit(); // heterogeneous control
        if (i % 5 == 0) {
            auto& m = t.cutoff[i % 2]; // include failures on the *second* side
            std::array<double*,9> fields{&m.pinPerLoadedBus, &m.pinPerTouch, &m.pinPerEnvelopeVolt,
                &m.busBaseVolts, &m.busPerTouchSum, &m.busPerEnvelopeVoltSum, &m.effectiveCvPerPinVolt,
                &m.referencePinVolts, &m.effectiveCvOffset};
            *fields[rng.next() % fields.size()] = rng.edge();
        }
        if (i % 23 == 0) t.compiled = false;
        const bool prechecked = i % 2 == 0 && sf::heterogeneous_cutoff::classifyImmutableTone(t);
        sf::CutoffReadout old, next;
        std::memset(&old, 0x3a, sizeof old); std::memset(&next, 0x3a, sizeof next);
        bool oldUniform = false, nextUniform = false;
        const unsigned csr = 0x1f80 | (i&1 ? 0x8000 : 0) | (i&2 ? 0x40 : 0)
            | ((i>>2) & 63u) | ((i/127 % 4) << 13);
        reset(csr); const bool a = evaluate<false>(t, d, old, oldUniform, prechecked); const auto fa = observe();
        reset(csr); const bool b = evaluate<true>(t, d, next, nextUniform, prechecked); const auto fb = observe();
        if (a != b || oldUniform != nextUniform || fa.csr != fb.csr || fa.cw != fb.cw || fa.sw != fb.sw
            || fa.err != fb.err || std::memcmp(&old, &next, sizeof old)) {
            std::fprintf(stderr,"mismatch in transaction %u, ok=%d/%d, csr=%x/%x\n",i,a,b,fa.csr,fb.csr);
            return 1;
        }
        accepted += a; uniformEntries += oldUniform;
    }
    std::printf("65536 transactions, %u accepted; %u uniform-path entries; outputs and FP state matched\n",
                accepted, uniformEntries);
    return accepted == 0 || uniformEntries == 0;
}

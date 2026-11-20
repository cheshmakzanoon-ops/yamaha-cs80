// Standalone differential for the Engine-private receiver entry. The sole
// numerical oracle is the unchanged public scalar shared receiver solver.
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

namespace cs80 {
#include "core/SharedUniformReceivers.cpp.inc"
}

#if !defined(CS80_DETAIL_CERTIFIED_SHARED_RECEIVERS)
int main() { std::fputs("Unqualified build; no certified receiver entry.\n", stderr); return 77; }
#else
namespace {
namespace sf = cs80::source_factory;
using Boundaries = std::array<sf::RankBoundary, 2>;
struct Fp { unsigned csr; unsigned short cw, sw; int err; };
Fp observe() {
    Fp result{_mm_getcsr(), 0, 0, errno};
    asm volatile("fnstcw %0; fnstsw %1" : "=m"(result.cw), "=m"(result.sw) :: "memory");
    return result;
}
void reset(unsigned csr) { std::fesetenv(FE_DFL_ENV); _mm_setcsr(csr); errno = EDOM; }

struct Words {
    std::uint32_t state = 0x5c80a772u;
    std::uint32_t next() { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
    double unit() { return static_cast<double>(next()) * 0x1p-32; }
    // The engine creates its actual drives by ordinary float-to-double promotion.
    double control(float upper = 1.f) { return static_cast<float>(unit() * upper); }
    double special() {
        constexpr auto min = std::numeric_limits<double>::denorm_min();
        constexpr std::array<double, 18> values {0., -0., min, -min, 0x1p-1022, 0x1p-160, 1., 3.,
            10., 10000., -15., 10001., 1e300, -1e300, __builtin_inf(), -__builtin_inf(),
            std::bit_cast<double>(0x7ff0000000000011ull),std::bit_cast<double>(0x7ff8000000000011ull)};
        return values[next() % values.size()];
    }
};
}

int main() {
    Words rng;
    unsigned accepted = 0, rejected = 0;
    for (unsigned i = 0; i < 65536; ++i) {
        reset(0x1f80);
        Boundaries b;
        sf::BothRankDrives d;
        const double m = i % 5 == 0 ? rng.special() : rng.unit();
        for (unsigned rank = 0; rank != 2; ++rank) {
            auto& s = b[rank];
            s = {{rng.unit()*10, rng.unit()*9999}, {rng.unit()*10, rng.unit()*9999},
                 {rng.unit()*10, rng.unit()*9999}, rng.unit()*10, rng.unit()*10};
            // Each boundary/side can reject independently, including rank II
            // after rank I has inputs for an otherwise ordinary valid transaction.
            if (i % (3+rank) == 0) {
                std::array<double*,8> fields{&s.qHp.volts, &s.qHp.ohms, &s.qLp.volts, &s.qLp.ohms,
                    &s.level.volts, &s.level.ohms, &s.initialLevelVolts, &s.afterLevelVolts};
                *fields[rng.next() % fields.size()] = rng.special();
            }
            for (auto& v : d[rank]) {
                v = {(rng.next() & 1u ? 1. : -1.) * rng.control(3.f), rng.control(), rng.control()};
                // Both signed zeros, float subnormals promoted to *normal*
                // doubles, and exact endpoints remain in the unclamped domain.
                constexpr std::array<double,6> special{0., -0., 0x1p-149, 0x1p-126, .5, 1.};
                if ((rng.next() & 3u) == 0) v.velocity = special[rng.next() % special.size()];
                if ((rng.next() & 3u) == 0) v.pressure = special[rng.next() % special.size()];
            }
        }
        sf::SharedReadout old{}, next{};
        std::memset(&old, 0x3a, sizeof old); std::memset(&next, 0x3a, sizeof next);
        old.masterResSaturated = next.masterResSaturated = (i & 1u) != 0;
        const unsigned csr = 0x1f80U | (i & 1u ? 0x8000u : 0) | (i & 2u ? 0x40u : 0) | (i >> 2u & 63u);
        reset(csr);
        const bool oldOk = sf::evaluateShared(b, m, d, old);
        const auto a = observe();
        reset(csr);
        const bool nextOk = cs80::evaluateCertifiedReceivers(b, m, d, next);
        const auto n = observe();
        if (oldOk) ++accepted; else ++rejected;
        if (oldOk != nextOk || a.csr != n.csr || a.cw != n.cw || a.sw != n.sw || a.err != n.err
            || std::memcmp(&old, &next, sizeof old)) {
            std::fprintf(stderr, "mismatch at %u, result=%d/%d csr=%x/%x memory=%d\n",
                         i, oldOk, nextOk, a.csr, n.csr, std::memcmp(&old, &next, sizeof old)); return 1;
        }
    }
    std::printf("65536 transactions: %u accepted, %u rejected; complete receiver words and FP state matched\n", accepted, rejected);
    return accepted == 0 || rejected == 0;
}
#endif

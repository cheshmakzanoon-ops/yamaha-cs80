// Diagnostic ONLY. Production arms contain neither a counter nor this link specialization.
// Link externally to regular archive with --wrap=<printed in wrapper-symbol.txt>.
#include "core/Ig00156Batch.h"
#include <cstdint>
#include <limits>
namespace { unsigned long long hits{}, calls{}; }
extern "C" void cs80_lane_count_reset() {hits=0;calls=0;}
extern "C" unsigned long long cs80_lane_count_hits() {return hits;}
extern "C" unsigned long long cs80_lane_count_calls() {return calls;}
#define NAME(x) x ## _ZN4cs8029tryReuseIg00156LpCoefficientsERNS_12Ig00156StageEffffRNS_24Ig00156LpCoefficientMemoERNS_21Ig00156LpStockRefreshE
#define ARGS cs80::Ig00156Stage& s, float cutoff, float q, float fs, float clock, cs80::Ig00156LpCoefficientMemo& m, cs80::Ig00156LpStockRefresh& t
extern "C" bool NAME(__real_)(ARGS) noexcept;
extern "C" bool NAME(__wrap_)(ARGS) noexcept {
    const auto hit=NAME(__real_)(s,cutoff,q,fs,clock,m,t);
    ++calls;hits+=hit;return hit;
}

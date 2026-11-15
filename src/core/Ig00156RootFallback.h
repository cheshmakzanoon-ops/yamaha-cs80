#pragma once
#include "Dsp.h"
#include <cmath>

namespace cs80::ig00156_detail {
struct RootPoint { double bp, hp, t1, t2, residual; };
// Caller-local, cold bounded recovery: do not merge differently compiled FP
// implementations across translation units. No width-only convergence criterion.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static inline bool bracketedRoot(double z, double c0, double c1, double g,
                                 double ff, double level, RootPoint& output) noexcept
{
    if (!std::isfinite(z) || !std::isfinite(c0) || !std::isfinite(c1)
        || !std::isfinite(g) || !std::isfinite(ff) || !std::isfinite(level)
        || g < 0 || ff < 0 || level <= 0) return false;
    const double a = 1.0 + ff * (c1 + ff), b = ff * c0 - z;
    const double bound = g * level * (1.0 + ff);
    if (!std::isfinite(a) || a <= 0 || !std::isfinite(b) || !std::isfinite(bound)) return false;
    const double inv = 1.0 / level;
    if (!std::isfinite(inv)) return false;
    auto evaluate = [&](double bp, RootPoint& p) {
        p.bp = bp; p.t2 = fastTanh(bp * inv);
        p.hp = c0 + c1 * bp + g * level * p.t2 + ff * bp;
        p.t1 = fastTanh(p.hp * inv);
        p.residual = bp - z + g * level * p.t1 + ff * p.hp;
        return std::isfinite(p.bp) && std::isfinite(p.hp)
            && std::isfinite(p.t1) && std::isfinite(p.t2) && std::isfinite(p.residual);
    };
    double lo = (-b - bound) / a, hi = (-b + bound) / a;
    RootPoint l{}, h{}, m{};
    for (int expansion = 0; expansion <= 8; ++expansion) {
        if (!evaluate(lo,l) || !evaluate(hi,h)) return false;
        if (std::abs(l.residual) < 1.0e-7) { output=l; return true; }
        if (std::abs(h.residual) < 1.0e-7) { output=h; return true; }
        if (l.residual <= 0 && h.residual >= 0) break;
        if (expansion == 8) return false;
        const double width = hi-lo;
        if (!std::isfinite(width) || width <= 0) return false;
        lo -= width; hi += width;
    }
    double fl=l.residual, fh=h.residual;
    int lastSide=0;
    for (int iteration=0; iteration<80; ++iteration) {
        const double width=hi-lo;
        if (!std::isfinite(width) || width<=0) return false;
        // Try at most16 interpolation steps, then retain a full64 bisection tail.
        // Illinois weights only select candidates; acceptance uses real residual.
        double fraction=0.5;
        if (iteration<16) {
            const double denominator=fh-fl;
            const double proposed=-fl/denominator;
            if (std::isfinite(proposed) && proposed>0.0 && proposed<1.0) fraction=proposed;
        }
        double mid=lo+fraction*width;
        if (mid==lo || mid==hi) mid=lo+0.5*width;
        if (mid==lo || mid==hi || !evaluate(mid,m)) return false;
        if (std::abs(m.residual)<1.0e-7) { output=m; return true; }
        if (m.residual<0) {
            lo=mid; fl=m.residual;
            if (lastSide==-1) fh*=0.5;
            lastSide=-1;
        } else {
            hi=mid; fh=m.residual;
            if (lastSide==1) fl*=0.5;
            lastSide=1;
        }
    }
    return false;
}
}

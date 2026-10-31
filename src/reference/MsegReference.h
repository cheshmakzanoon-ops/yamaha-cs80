#pragma once

#include "core/Mseg.h"

#include <cmath>

// Offline closed-form reference for the multi-segment envelope (src/core/Mseg.h). It does
// not step: the level after k ticks is computed directly from k, the shape and the gate
// history, in double precision, so stepping error in the realtime runner shows up as a
// residual. This is a specification check of a modern control, not CS-80 evidence.
namespace cs80::reference
{

// Curve law restated from its definition: a single-RC charge with exponent ln(1000) * c.
inline double msegReferenceCurve(double t, double c)
{
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;
    if (c == 0.0 || std::abs(c) < 1.0e-4) return t;
    const double a = 6.907755278982137 * c;
    return (1.0 - std::exp(-a * t)) / (1.0 - std::exp(-a));
}

// Level on the static shape at position p, from segment `first` onwards, where the first
// segment may start from `firstStartLevel` instead of its own point.
inline double msegReferenceStatic(const modern::MsegShape& s, double p, int first, double firstStartLevel)
{
    const int last = s.count - 1;
    for (int i = first; i < last; ++i)
    {
        const double x0 = s.points[(std::size_t) i].time, x1 = s.points[(std::size_t) i + 1].time;
        if (p < x1)
        {
            const double y0 = i == first ? firstStartLevel : s.points[(std::size_t) i].level;
            const double y1 = s.points[(std::size_t) i + 1].level;
            const double t = x1 > x0 ? (p - x0) / (x1 - x0) : 1.0;
            return y0 + (y1 - y0) * msegReferenceCurve(t, s.points[(std::size_t) i].curve);
        }
    }
    return first == last ? firstStartLevel : (double) s.points[(std::size_t) last].level;
}

// Position after k ticks of `advance` with the gate held (loop folding applied).
inline double msegReferenceHeldPosition(const modern::MsegShape& s, double advance, long k)
{
    const double p = (double) k * advance;
    const double end = s.points[(std::size_t) (s.count - 1)].time;
    if (s.loop == modern::MsegLoop::off)
        return std::min(p, end);
    const double xs = s.points[(std::size_t) s.loopStart].time;
    const double xe = s.points[(std::size_t) s.loopEnd].time;
    if (p < xe)
        return p;
    return xe > xs ? xs + std::fmod(p - xe, xe - xs) : xe;
}

// Level output by the k-th tick (k >= 1) after a trigger at tick 0. releaseTick is the
// first tick whose gate is low (or -1 for a gate held throughout).
inline double msegReferenceLevel(const modern::MsegShape& s, double advance, long k, long releaseTick = -1)
{
    const bool released = releaseTick > 0 && k >= releaseTick;
    if (! released || s.loop != modern::MsegLoop::sustain)
    {
        const double p = msegReferenceHeldPosition(s, advance, k);
        return msegReferenceStatic(s, p, 0, s.points[0].level);
    }
    // Gate fell at releaseTick: the level reached by the previous tick is held as the start
    // of the segment after the loop end, and the position restarts from the loop end.
    const double reached = releaseTick > 1 ? msegReferenceLevel(s, advance, releaseTick - 1) : s.points[0].level;
    const int e = s.loopEnd;
    if (e >= s.count - 1)
        return reached;
    const double end = s.points[(std::size_t) (s.count - 1)].time;
    const double q = std::min((double) s.points[(std::size_t) e].time + (double) (k - releaseTick + 1) * advance, end);
    return msegReferenceStatic(s, q, e, reached);
}

} // namespace cs80::reference

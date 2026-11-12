#pragma once
// Offline conditional model, not measured hardware. Independent of core/Vca.h.
// Values/laws and their evidence classes: docs/IG00151_VCA_MODEL.md.
#include <algorithm>
#include <cmath>

namespace cs80::reference::ig00151 {
inline constexpr double liSeconds = (1.0 / (1.0 / 180000.0 + 1.0 / 560000.0 + 3.0 / 270000.0)) * 1e-8;
inline constexpr double acSeconds = 1e-6 * 100000.0;
// Preserve the declared production float constants, promoted to double.
inline constexpr double offset = static_cast<double>(1.5e-3f);
inline constexpr double feedthrough = static_cast<double>(1e-3f);
struct Drive { double input, lc, ei, envelope; };
struct State { double li, coupling; };
inline Drive interpolate(Drive a, Drive b, double f) {
    return {a.input+(b.input-a.input)*f, a.lc+(b.lc-a.lc)*f,
            a.ei+(b.ei-a.ei)*f, a.envelope+(b.envelope-a.envelope)*f};
}
inline double gain(State s, Drive d) { return std::max(0.0, s.li)*std::exp2(d.ei)*d.envelope; }
inline double raw(State s, Drive d) { return gain(s,d)*(d.input+offset) + feedthrough*gain(s,d); }
inline double output(State s, Drive d) { return raw(s,d)-s.coupling; }
inline State derivative(State s, Drive d) { return {(d.lc-s.li)/liSeconds, (raw(s,d)-s.coupling)/acSeconds}; }
inline State add(State a, State b, double h) { return {a.li+h*b.li, a.coupling+h*b.coupling}; }
// a,b are the forcing endpoints of this single step. Callers split at knots.
inline State rk4(State s, Drive a, Drive b, double h) {
    const auto mid=interpolate(a,b,0.5);
    const auto k1=derivative(s,a), k2=derivative(add(s,k1,h/2),mid);
    const auto k3=derivative(add(s,k2,h/2),mid), k4=derivative(add(s,k3,h),b);
    return {s.li+h/6*(k1.li+2*k2.li+2*k3.li+k4.li),
            s.coupling+h/6*(k1.coupling+2*k2.coupling+2*k3.coupling+k4.coupling)};
}
} // namespace cs80::reference::ig00151

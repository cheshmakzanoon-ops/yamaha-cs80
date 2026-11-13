// Bounded ordinary-signal regression; no Engine, plugin, FX models or benchmarks.
#include "core/FxLoudness.h"
#include <array>
#include <cmath>
#include <cstdio>

namespace {
using cs80::fx::LoudnessMatcher;
constexpr double pi = 3.14159265358979323846;
constexpr double terminalTolerance = 1.0e-6;
constexpr double monotonicTolerance = 1.0e-12;
constexpr std::array<double, 3> rates {44100.0, 48000.0, 96000.0};
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
std::array<float, 2> signal(int n, double rate) {
    return {float(0.1 * std::sin(2.0 * pi * 1000.0 * n / rate)),
            float(0.07 * std::cos(2.0 * pi * 733.0 * n / rate))};
}
void silenceAndUnity(double rate) {
    LoudnessMatcher silence, unity;
    silence.prepare(rate); unity.prepare(rate);
    bool silenceExact = true, unityExact = true;
    for (int n = 0; n < int(rate); ++n) {
        silenceExact = silence.process(0.f, 0.f, 0.f, 0.f) == 1.f
            && silence.currentGain() == 1.0 && silenceExact;
        const auto x = signal(n, rate);
        unityExact = unity.process(x[0], x[1], x[0], x[1]) == 1.f
            && unity.currentGain() == 1.0 && unityExact;
    }
    check(silenceExact, "fresh silence preserves exact unity");
    check(unityExact, "identical input/output preserves exact unity");
}
void ratiosAndChannelSwap(double rate, float scale) {
    LoudnessMatcher normal, swapped;
    normal.prepare(rate); swapped.prepare(rate);
    const double expected = 1.0 / scale;
    double previous = 1.0;
    bool finite = true, monotonic = true, bounded = true, swapExact = true;
    for (int n = 0; n < int(4.0 * rate); ++n) {
        const auto x = signal(n, rate);
        const float a = normal.process(x[0], x[1], scale*x[0], scale*x[1]);
        const float b = swapped.process(x[1], x[0], scale*x[1], scale*x[0]);
        const double g = normal.currentGain();
        finite = std::isfinite(a) && std::isfinite(b) && std::isfinite(g) && finite;
        swapExact = a == b && g == swapped.currentGain() && swapExact;
        monotonic = (expected > 1.0 ? g + monotonicTolerance >= previous
                                   : g <= previous + monotonicTolerance) && monotonic;
        bounded = g >= std::min(1.0, expected)-monotonicTolerance
            && g <= std::max(1.0, expected)+monotonicTolerance && bounded;
        previous = g;
    }
    const double error = std::abs(normal.currentGain()-expected);
    check(finite, "ordinary scaled-signal gains finite");
    check(monotonic && bounded, "gain approaches proportional target monotonically within bounds");
    check(swapExact, "stereo channel exchange leaves exact gain sequence unchanged");
    check(error <= terminalTolerance, "proportional target reached within fixed tolerance");
    std::printf("rate=%.0f scale=%.1f terminal_gain=%.17g terminal_error=%.17g\n",
                rate, double(scale), normal.currentGain(), error);
}
void resetAndReprepare(double rate, double nextRate) {
    LoudnessMatcher used;
    used.prepare(rate);
    for (int n=0; n<int(rate/4); ++n) {
        const auto x=signal(n,rate); used.process(x[0],x[1],0.5f*x[0],0.5f*x[1]);
    }
    check(std::abs(used.currentGain()-1.0)>0.1, "reset witness first establishes non-neutral gain");
    used.reset();
    LoudnessMatcher fresh; fresh.prepare(rate);
    bool resetExact=used.currentGain()==1.0;
    for (int n=0; n<int(rate/4); ++n) {
        const auto x=signal(n+31,rate);
        const float a=used.process(x[0],x[1],2.f*x[0],2.f*x[1]);
        const float b=fresh.process(x[0],x[1],2.f*x[0],2.f*x[1]);
        resetExact=a==b && used.currentGain()==fresh.currentGain() && resetExact;
    }
    check(resetExact,"reset reproduces fresh matcher exactly");
    check(std::abs(used.currentGain()-1.0)>0.1, "reprepare witness first establishes non-neutral gain");
    used.prepare(nextRate);
    LoudnessMatcher nextFresh; nextFresh.prepare(nextRate);
    bool prepareExact=used.currentGain()==1.0;
    for (int n=0; n<int(nextRate/4); ++n) {
        const auto x=signal(n+59,nextRate);
        const float a=used.process(x[0],x[1],0.5f*x[0],0.5f*x[1]);
        const float b=nextFresh.process(x[0],x[1],0.5f*x[0],0.5f*x[1]);
        prepareExact=a==b && used.currentGain()==nextFresh.currentGain() && prepareExact;
    }
    check(prepareExact,"reprepare at changed rate reproduces fresh matcher exactly");
}
}
int main() {
    for (std::size_t i=0;i<rates.size();++i) {
        const double rate=rates[i];
        silenceAndUnity(rate);
        ratiosAndChannelSwap(rate,0.5f);
        ratiosAndChannelSwap(rate,2.0f);
        resetAndReprepare(rate,rates[(i+1)%rates.size()]);
    }
    std::printf("checks=%d failures=%d rates=3 ordinary_signal_only=1\n",checks,failures);
    return failures?1:0;
}

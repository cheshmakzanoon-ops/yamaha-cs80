// A38 ordinary public-API lifecycle checks. No production instrumentation,
// invalid inputs, fault injection, allocation hooks, or stack constraints.
#include "core/InstrumentBody.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
using namespace cs80::body;
struct Route {
    const char* label;
    int model, variant, material;
    float note, size;
};
struct Rate { double internal, host; };
constexpr std::array<Rate, 3> rates {{{8000, 8000}, {96000, 48000}, {384000, 96000}}};
constexpr std::uint32_t seed = 0x7812a3u;
int checks = 0, failures = 0, comparisons = 0;
std::uint64_t samplePairs = 0;
double largestPeak = 0;

void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL %s\n", label); }
}
float hz(double note) { return static_cast<float>(440.0 * std::exp2((note - 69.0) / 12.0)); }
void onset(InstrumentBody& body, const Route& route) {
    body.noteOn(route.model, route.note, .83f, .63f, 1.7f, seed,
                route.size, route.variant, route.material);
}
float step(InstrumentBody& body, const Route& route, int frame) {
    // Ordinary brightness, pressure and one-octave pitch gestures, including
    // exact return to the captured onset pitch. Every argument is finite.
    const double bend = frame < 192 ? 0.0 : frame < 768 ? 12.0
                      : frame < 1536 ? -12.0 : frame < 2304 ? .35 : 0.0;
    const float colour = (frame / 113) % 2 ? .82f : .21f;
    const float pressure = (frame / 257) % 2 ? .72f : .10f;
    return body.tick(hz(static_cast<double>(route.note) + bend), colour, pressure);
}
void warm(InstrumentBody& body, const Route& route, int frames) {
    for (int frame = 0; frame < frames; ++frame) (void)step(body, route, frame);
}
void disabled(InstrumentBody& body) {
    check(!body.enabled(), "reset/prepare/off disables the body");
    bool silent = true;
    for (int frame = 0; frame < 37; ++frame)
        silent = silent && body.tick(261.625565f, .63f, .4f) == 0.0f;
    check(silent, "disabled body remains exactly silent");
}
void compare(InstrumentBody& a, InstrumentBody& b, const Route& route, int first = 0) {
    bool equal = true, finite = true;
    double peak = 0;
    for (int frame = first; frame < first + 4097; ++frame) {
        const float x = step(a, route, frame), y = step(b, route, frame);
        equal = equal && std::bit_cast<std::uint32_t>(x) == std::bit_cast<std::uint32_t>(y);
        finite = finite && std::isfinite(x) && std::isfinite(y);
        peak = std::max({peak, std::abs(static_cast<double>(x)), std::abs(static_cast<double>(y))});
        ++samplePairs;
    }
    largestPeak = std::max(largestPeak, peak);
    ++comparisons;
    check(equal, route.label);
    check(finite && peak < .85, "lifecycle comparison stays finite and bounded");
}
std::vector<Route> routes() {
    std::vector<Route> result {
        {"48-mode Ah reuse", organ, choirAh, defaultMaterial, 36, 1},
        {"48-mode Oo reuse", organ, choirOo, defaultMaterial, 36, 1},
        {"24-mode Ah reuse", organ, choirAh, defaultMaterial, 60, 1},
        {"24-mode Oo reuse", organ, choirOo, defaultMaterial, 60, 1},
        {"clav reuse", clav, standard, defaultMaterial, 57, 1},
        {"harpsichord reuse", harpsichord, standard, defaultMaterial, 57, 1},
        {"nylon reuse", nylon, standard, defaultMaterial, 57, 1},
        {"steel reuse", steel, standard, defaultMaterial, 57, 1},
        {"bass reuse", bass, standard, defaultMaterial, 45, 1},
        {"banjo reuse", steel, standard, banjo, 40, 1},
        {"koto reuse", steel, standard, koto, 48, 1},
        {"harp reuse", nylon, standard, harp, 48, 1},
        {"piano reuse", piano, standard, defaultMaterial, 24, 1},
        {"reed pickup reuse", reed, standard, defaultMaterial, 60, 1},
        {"flute noise reuse", flute, standard, defaultMaterial, 60, 1},
    };
    // Neighbouring legal body-size settings around the 24- and 48-mode
    // onset boundaries; the existing LowChoirSpectrumTests owns continuity.
    const double key = 440.0 * std::exp2((48.0 - 69.0) / 12.0);
    for (int variant : {choirAh, choirOo})
        for (int boundary : {24, 48})
            for (double side : {-1.0, 1.0})
                result.push_back({"fractional choir boundary reuse", organ, variant,
                                  defaultMaterial, 48,
                                  static_cast<float>(4400.0 / (boundary * key) * (1.0 + side * 1.e-6))});
    return result;
}
void noteReuse(const std::vector<Route>& targets) {
    const std::array<Route, 3> previous {{targets[0], targets[9], targets[13]}};
    for (const Rate rate : rates)
        for (const Route& donor : previous)
            for (const Route& target : targets)
                for (int resetKind : {0, 1, 2}) {
                    InstrumentBody used, fresh;
                    used.prepare(rate.internal, rate.host);
                    fresh.prepare(rate.internal, rate.host);
                    onset(used, donor);
                    warm(used, donor, 2051);
                    if (resetKind == 1) { used.reset(); disabled(used); }
                    if (resetKind == 2) {
                        used.noteOn(off, 60, .7f, .5f, 1, seed);
                        disabled(used);
                    }
                    onset(used, target); onset(fresh, target);
                    compare(used, fresh, target);
                }
}
void reprepare(const std::vector<Route>& targets) {
    for (const Rate from : rates)
        for (const Rate to : rates)
            for (const Route& target : targets) {
                InstrumentBody used, fresh;
                used.prepare(from.internal, from.host);
                onset(used, targets[0]); warm(used, targets[0], 2051);
                used.prepare(to.internal, to.host); disabled(used);
                fresh.prepare(to.internal, to.host);
                onset(used, target); onset(fresh, target);
                compare(used, fresh, target);
            }
}
void copiedHandoff(const std::vector<Route>& targets) {
    for (const Route& target : targets)
        for (int split : {0, 1, 15, 16, 17, 2047, 2048, 2051}) {
            InstrumentBody original, reference;
            original.prepare(96000, 48000); reference.prepare(96000, 48000);
            onset(original, target); onset(reference, target);
            warm(original, target, split); warm(reference, target, split);
            InstrumentBody outgoing = original;
            // Mirrors ownership at a normal Engine reassignment: the outgoing
            // body is independent after the incoming body is reset/retriggered.
            original.reset(); onset(original, targets[13]);
            compare(outgoing, reference, target, split);
        }
}
} // namespace

int main() {
    const auto targets = routes();
    noteReuse(targets); reprepare(targets); copiedHandoff(targets);
    std::printf("A38 body lifecycle: %d checks, %d failures, %d comparisons, %llu sample pairs, peak %.9g\n",
                checks, failures, comparisons, static_cast<unsigned long long>(samplePairs), largestPeak);
    std::printf("Routes %zu; valid rate pairs 8000/8000, 96000/48000, 384000/96000; body size %zu bytes\n",
                targets.size(), sizeof(InstrumentBody));
    return failures == 0 ? 0 : 1;
}

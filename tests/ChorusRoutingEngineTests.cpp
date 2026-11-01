// Circuit-only opt-in OE1/OE2 routing contracts, preregistered before audio
// wiring. Yamaha BA617 adjustment p21 and OE57/OE58 establish distinct named
// control nodes, NOT simultaneous hardware operation (declared inference).
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
using namespace cs80;
int checks = 0, failures = 0;
void check(bool condition, const char* law)
{
    ++checks;
    if (! condition)
    {
        std::printf("FAIL: %s\n", law);
        ++failures;
    }
}

// All other parameters are exactly equal; no hardware measurements are used.
ControlFrame frame(RenderPolicy policy, int independentRouting)
{
    ControlFrame f;
    f.effectiveValues[P_sourcePerformance] = policy != RenderPolicy::legacy ? 1.0f : 0.0f;
    f.effectiveValues[P_parityMode] = policy == RenderPolicy::isolation ? 1.0f : 0.0f;
    f.effectiveValues[P_oversampling] = 0.0f; // 2x, deterministic Eco policy
    f.effectiveValues[P_chorusOn] = 0.0f;
    f.effectiveValues[P_chorusMode] = 1.0f;
    f.effectiveValues[P_chorusSpeed] = 2.37f;
    f.effectiveValues[P_chorusDepth] = 0.95f;
    f.effectiveValues[P_chorusTremoloRouting] = (float) independentRouting;
    f.effectiveValues[P_analogNoise] = 0.0f;
    f.effectiveValues[P_drift] = 0.0f;
    f.effectiveValues[P_voiceDispersion] = 0.0f;
    f.effectiveValues[P_detune] = 0.0f;
    f.effectiveValues[P_ringAmount] = 0.0f;
    f.effectiveValues[P_subVca] = 0.0f;
    f.effectiveValues[P_subVcf] = 0.0f;
    f.effectiveValues[P_subVco] = 0.0f;
    return f;
}

std::vector<float> render(RenderPolicy policy, int routing)
{
    Engine e;
    e.prepare(48000.0);
    check(e.setControlFrame(frame(policy, routing)), "routing fixture accepted by Engine control frame");
    e.noteOn(60, 0.9f);
    std::vector<float> audio(8192 * 2);
    e.render(audio.data(), 8192);
    check(std::all_of(audio.begin(), audio.end(), [](float s) { return std::isfinite(s); }),
          "independent routing keeps all engine audio samples finite");
    return audio;
}

float largestDifference(const std::vector<float>& a, const std::vector<float>& b)
{
    float greatest = 0.0f;
    for (size_t i = 0; i < a.size(); ++i)
        greatest = std::max(greatest, std::abs(a[i] - b[i]));
    return greatest;
}
} // namespace

int main()
{
    static_assert(P_chorusTremoloRouting == P_numParams - 1,
                  "independent OE1/OE2 routing must be the last appended parameter");
    constexpr auto legacy = RenderPolicy::legacy;
    constexpr auto pure = RenderPolicy::isolation;
    constexpr auto circuit = RenderPolicy::sourcePerformance;

    // Non-negotiable byte identity: neither old engine nor PURE may respond to
    // the modern feature even when a host automates its strongest override.
    check(render(legacy, 0) == render(legacy, 4),
          "legacy audio is byte-identical with both OE1/OE2 overrides on");
    check(render(pure, 0) == render(pure, 4),
          "PURE CS-80 circuit audio is byte-identical with both OE1/OE2 overrides on");

    // Old selector OFF is the historical default in this fixture. An explicit
    // BOTH-OFF override must be sample-identical; all opt-ins must be audible.
    const auto neutral = render(circuit, 0);
    const auto bothOff = render(circuit, 1);
    const auto chorusOnly = render(circuit, 2);
    const auto tremoloOnly = render(circuit, 3);
    const auto bothOn = render(circuit, 4);
    check(neutral == bothOff, "default shipped selector is exactly preserved by explicit BOTH OFF");
    constexpr float kDetectableChange = 1.0e-5f; // prereg, not a circuit calibration tolerance
    check(largestDifference(neutral, chorusOnly) > kDetectableChange,
          "opt-in CHORUS alone changes source-engine audio");
    check(largestDifference(neutral, tremoloOnly) > kDetectableChange,
          "opt-in TREMOLO alone changes source-engine audio");
    check(largestDifference(chorusOnly, bothOn) > kDetectableChange
              && largestDifference(tremoloOnly, bothOn) > kDetectableChange,
          "opt-in BOTH ON combines independent CHORUS clock and TREMOLO VCA laws");

    std::printf("OE1/OE2 Engine routing: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

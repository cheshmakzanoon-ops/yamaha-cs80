// Integration gate registered before evaluation in ws1-voice.json.
// The model's inferred downstream capacitor stays connected after amplitude
// Idle. No claim of hardware audibility or a change to the phrase click gate.
#include "core/Engine.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace cs80;
namespace {
constexpr int kNumRanks = 2;
int checks = 0, failures = 0;
void check(bool okay, const char* message)
{
    ++checks;
    if (!okay) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

void fixture(const ws1::Tolerances& limits, int rate, int quality, bool pure, int audibleRank)
{
    ParamValues p;
    p[P_sourcePerformance] = 1.0f;
    p[P_parityMode] = pure ? 1.0f : 0.0f;
    p[P_oversampling] = static_cast<float>(quality);
    p[P_mix] = static_cast<float>(audibleRank);
    p[P_keyRelease] = 0.0f;
    p[P_masterSustain] = 0.0f;
    p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = 0.0f;
    p[P_chorusOn] = p[P_arpOn] = 0.0f;
    p[P_volume] = 0.6f;
    for (int rank = 0; rank < kNumRanks; ++rank)
    {
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_sine)] = p[chParam(rank, CP_noise)] = 0.0f;
        p[chParam(rank, CP_vcfLevel)] = 0.0f;
        p[chParam(rank, CP_vLevel)] = 2.6f;
        p[chParam(rank, CP_vA)] = 0.002f;
        p[chParam(rank, CP_vD)] = 0.015f;
        p[chParam(rank, CP_vS)] = 0.7f;
        p[chParam(rank, CP_vR)] = 0.005f;
    }

    Engine normal, panic;
    normal.setParams(p); panic.setParams(p);
    normal.prepare(rate); panic.prepare(rate);
    normal.noteOn(60, 0.9f); panic.noteOn(60, 0.9f);
    std::array<float, 2> n {}, h {};
    bool prefixIdentical = true;
    auto clockTwins = [&] {
        normal.render(n.data(), 1); panic.render(h.data(), 1);
        prefixIdentical = prefixIdentical && n == h;
    };
    for (int frame = 0; frame < rate / 4; ++frame) clockTwins();
    normal.noteOff(60); panic.noteOff(60);
    int released = 0;
    while (normal.activeVoices() != 0 && released < rate)
    {
        clockTwins();
        ++released;
    }
    check(prefixIdentical, "twins retain identical PCM before diagnostic panic");
    check(released > 0 && released < rate && normal.activeVoices() == 0
          && panic.activeVoices() == 0, "both amplitude envelopes retire");

    std::array<double, kNumRanks> charge {};
    for (int rank = 0; rank < kNumRanks; ++rank)
    {
        charge[rank] = normal.getRankVcaState(0, rank).coupling;
        check(std::abs(charge[rank]) > 100.0 * limits["closed_handoff_step_abs"],
              "both ranks retain a charged capacitor at retirement");
    }
    panic.allNotesOff(false);
    bool panicCleared = true;
    for (int rank = 0; rank < kNumRanks; ++rank)
        panicCleared = panicCleared && panic.getRankVcaState(0, rank).coupling == 0.0;
    check(panicCleared, "explicit hard panic clears both coupling states exactly");

    double maximumOutputDifference = 0.0;
    std::array<double, kNumRanks> maximumStateError {};
    for (int frame = 1; frame <= rate / 20; ++frame)
    {
        normal.render(n.data(), 1); panic.render(h.data(), 1);
        maximumOutputDifference = std::max({maximumOutputDifference,
            std::abs(static_cast<double>(n[0]) - h[0]),
            std::abs(static_cast<double>(n[1]) - h[1])});
        for (int rank = 0; rank < kNumRanks; ++rank)
        {
            // Independent physical time, not the engine's idleClock or its
            // recurrence. A second decimated discharge cannot pass this oracle.
            const double expected = charge[rank]
                * std::exp(-static_cast<double>(frame) / (rate * kIg00151CouplingSeconds));
            maximumStateError[rank] = std::max(maximumStateError[rank],
                std::abs(normal.getRankVcaState(0, rank).coupling - expected));
            panicCleared = panicCleared && panic.getRankVcaState(0, rank).coupling == 0.0;
        }
    }
    check(maximumOutputDifference > limits["closed_engine_tail_min_peak_abs"],
          "the selected rank's idle capacitor remains connected to output");
    for (int rank = 0; rank < kNumRanks; ++rank)
        check(maximumStateError[rank] <= limits["closed_engine_coupling_truth_abs"],
              "coupling discharge conserves actual host elapsed time");
    check(panicCleared, "panicked idle coupling stays exactly cleared");
    normal.reset();
    bool coldCleared = true;
    for (int frame = 0; frame <= rate / 1000; ++frame)
    {
        for (int rank = 0; rank < kNumRanks; ++rank)
            coldCleared = coldCleared && std::abs(normal.getRankVcaState(0, rank).coupling)
                <= limits["closed_engine_reset_coupling_abs_max"];
        normal.render(n.data(), 1);
    }
    check(coldCleared, "explicit engine reset clears charged coupling before cold idle output");
    std::printf("%s rate=%d quality=%d rank=%d tail=%.12g coupling_error_I=%.12g coupling_error_II=%.12g\n",
                pure ? "pure" : "source", rate, quality, audibleRank,
                maximumOutputDifference, maximumStateError[0], maximumStateError[1]);
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances limits(argc > 1 ? argv[1]
            : "tests/parity/tolerances-v2/ws1-voice.json", "ig00151_vca");
        // A changed contract must be reviewed, never silently weaken this gate.
        check(limits["closed_discharge_truth_abs"] == 1e-7
              && limits["closed_handoff_step_abs"] == 1e-6
              && limits["closed_engine_tail_min_peak_abs"] == 1e-9
              && limits["closed_engine_coupling_truth_abs"] == 1e-12
              && limits["closed_engine_reset_coupling_abs_max"] == 0.0,
              "registered integration limits remain unchanged");
        for (bool pure : {false, true})
            for (int rate : {44100, 48000, 96000})
                for (int quality : {0, 1, 2})
                    for (int rank : {0, 1}) fixture(limits, rate, quality, pure, rank);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n'; return 2;
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

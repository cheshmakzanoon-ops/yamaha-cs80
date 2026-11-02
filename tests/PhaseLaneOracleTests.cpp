// Preregistered mixed-lane continuation oracle for a prospective full nonlinear
// IG00156 phase-kernel backend. No new SIMD backend is implemented here.
// Reuse the existing, independently hash-sealed original-scalar comparator
// without modifying either it or its frozen math/source headers.
#define main cs80_original_scalar_oracle_main_not_selected
#include "ExactScalarFilterTests.cpp"
#undef main

#include <bit>
#include <limits>
#include <string>
#include <vector>

namespace {
struct LaneSnapshot {
    Phase::Vector nodes {}, inputs {};
    double firstSlope = 1., secondSlope = 1., inputRate = 0.;
};

struct PhaseLane {
    Phase actual;
    ReferencePhase reference;
    float rate;
    std::uint32_t rng;
    LaneSnapshot savedActual {}, savedReference {};

    PhaseLane(const Network& network, float selectedRate, std::uint32_t seed)
        : actual(network), reference(referenceNetwork(network)), rate(selectedRate), rng(seed) {}

    void set(float cutoff, float q) {
        const auto fp = fpControl();
        actual.set(cutoff, q, rate);
        check.equal(fpControl(), fp, "lane.set.actualFpControl");
        reference.set(cutoff, q, rate);
        check.equal(fpControl(), fp, "lane.set.oracleFpControl");
    }

    void save() {
        savedActual = {actual.nodes(), actual.inputHistory(), actual.firstSlope(),
                       actual.secondSlope(), actual.inputRate()};
        savedReference = {reference.nodes(), reference.inputHistory(), reference.firstSlope(),
                          reference.secondSlope(), reference.inputRate()};
    }

    void restore() {
        actual.restore(savedActual.nodes, savedActual.inputs, savedActual.firstSlope,
                       savedActual.secondSlope, savedActual.inputRate);
        reference.restore(savedReference.nodes, savedReference.inputs, savedReference.firstSlope,
                          savedReference.secondSlope, savedReference.inputRate);
        comparePhase(actual, reference);
    }

    void reset() {
        actual.reset();
        reference.reset();
        comparePhase(actual, reference);
    }
};

struct Coverage {
    std::uint64_t cases = 0, compared = 0, active = 0, masked = 0;
    std::uint64_t copies = 0, restores = 0, resets = 0, fpTransitions = 0;
    std::uint64_t realScalarProcessCalls = 0;
    std::uint64_t realVectorBackendCalls = 0; // must remain zero in prereg baseline
    std::uint32_t widthsSeen = 0, ratesSeen = 0;
};

constexpr std::array<int, 8> laneWidths {1, 2, 3, 4, 7, 8, 15, 16};
constexpr std::array<int, 4> roundModes {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
constexpr std::uint64_t sampleCount = 4096;

std::array<float, 2> laneControls(int lane, std::uint64_t n) {
    if (lane % 4 == 0) return controls[(lane + (n / 257)) % controls.size()];
    if (lane % 4 == 1) return movingControls(n + std::uint64_t(lane) * 31);
    return controls[lane % controls.size()];
}

float laneInput(PhaseLane& lane, int coordinate, std::uint64_t n) {
    float value = inputAt(3, n, lane.rate, lane.rng);
    if (n == sampleCount - 1) {
        constexpr std::array<std::uint32_t, 8> exceptional {
            0x80000000u, 0x00000001u, 0x007fffffu, 0x00800000u,
            0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc12345u
        };
        value = std::bit_cast<float>(exceptional[coordinate % exceptional.size()]);
    }
    return value;
}

int runLaneGrid() {
    FpEnvironment restoreCaller;
    const auto denormal = denormalModes();
    Coverage coverage;
    for (int width : laneWidths) {
        coverage.widthsSeen |= 1u << width;
        for (std::size_t ri = 0; ri < roundModes.size(); ++ri) {
            if (std::fesetround(roundModes[ri]) != 0) {
                std::cout << "Unsupported rounding mode=" << roundModes[ri] << '\n';
                continue;
            }
            for (std::size_t mi = 0; mi < denormal.size(); ++mi) {
                selectDenormalMode(denormal[mi]);
                std::vector<PhaseLane> lanes;
                lanes.reserve(static_cast<std::size_t>(width));
                for (int l = 0; l < width; ++l) {
                    const int rateIndex = (l + int(ri) + int(mi)) % int(audioRates.size());
                    const Network& network = (l % 2 == 0) ? cs80::kIg00156HighPassNetwork
                                                            : cs80::kIg00156LowPassNetwork;
                    lanes.emplace_back(network, audioRates[rateIndex],
                                       seed ^ (0x9e3779b9u * std::uint32_t(l + 1)));
                    coverage.ratesSeen |= 1u << rateIndex;
                }
                ++coverage.cases;
                for (std::uint64_t n = 0; n < sampleCount; ++n) {
                    check.sample = n;
                    // Change effective FP controls with stored state live;
                    // no reset, new coefficient key, or hidden rounding policy.
                    if (n == 512) {
                        check.require(std::fesetround(roundModes[(ri + 1) % roundModes.size()]) == 0,
                                      "lane.fpTransitionAccepted");
                        ++coverage.fpTransitions;
                    }
                    if (n == 1024) {
                        check.require(std::fesetround(roundModes[ri]) == 0,
                                      "lane.fpReturnAccepted");
                        ++coverage.fpTransitions;
                    }
                    if (n == 2048) {
                        selectDenormalMode(denormal[(mi + 1) % denormal.size()]);
                        ++coverage.fpTransitions;
                    }
                    if (n == 3072) {
                        selectDenormalMode(denormal[mi]);
                        ++coverage.fpTransitions;
                    }
                    // Copy, restore and reset independent lanes without
                    // copying or changing another lane's network or RNG.
                    if (n == 1024 && width > 1) {
                        lanes.back().actual = lanes.front().actual;
                        lanes.back().reference = lanes.front().reference;
                        ++coverage.copies;
                    }
                    if (n == 2048) {
                        for (int l = 0; l < width; ++l) if (l % 3 == 0) {
                            lanes[static_cast<std::size_t>(l)].restore();
                            ++coverage.restores;
                        }
                    }
                    if (n == 3072) {
                        for (int l = 0; l < width; ++l) if (l % 5 == 0) {
                            lanes[static_cast<std::size_t>(l)].reset();
                            ++coverage.resets;
                        }
                    }
                    for (int l = 0; l < width; ++l) {
                        check.context = "phase-lanes/width=" + std::to_string(width)
                                      + "/round=" + std::to_string(roundModes[ri])
                                      + "/denormal=" + std::to_string(denormal[mi])
                                      + "/lane=" + std::to_string(l);
                        auto& lane = lanes[static_cast<std::size_t>(l)];
                        if (n == 512) lane.save();
                        const auto fcq = laneControls(l, n);
                        lane.set(fcq[0], fcq[1]);
                        const float input = laneInput(lane, l, n);
                        const bool active = (n + std::uint64_t(l)) % 13 != 0;
                        if (active) {
                            const auto fp = fpControl();
                            float candidateInput = input;
#if defined(CS80_PHASE_LANE_MUTATE_CROSS_LANE)
                            if (n == 1 && l == 1) candidateInput += .125f;
#endif
                            auto actual = lane.actual.process(candidateInput);
                            check.equal(fpControl(), fp, "lane.actualFpControl");
                            const auto expected = lane.reference.process(input);
                            check.equal(fpControl(), fp, "lane.oracleFpControl");
#if defined(CS80_PHASE_LANE_MUTATE_OUTPUT)
                            if (n == 1 && l == 0)
                                actual.lp = std::bit_cast<float>(std::bit_cast<std::uint32_t>(actual.lp) ^ 1u);
#endif
                            compareOutput(actual, expected, "lane.output");
                            ++coverage.active;
                            ++coverage.realScalarProcessCalls;
                        } else {
#if defined(CS80_PHASE_LANE_MUTATE_MASK)
                            if (n == 0 && l == 0) (void)lane.actual.process(.25f);
#endif
                            ++coverage.masked;
                        }
                        comparePhase(lane.actual, lane.reference);
                        ++coverage.compared;
                        if (check.failures != 0) {
                            std::cout << "phase-lane oracle bit mismatch budget exceeded: "
                                      << check.failures << " failures after " << check.count
                                      << " comparisons\n";
                            return 1;
                        }
                    }
                }
            }
        }
    }
    check.context = "phase-lanes/coverage";
    check.require(coverage.widthsSeen == ((1u << 1) | (1u << 2) | (1u << 3) | (1u << 4)
                          | (1u << 7) | (1u << 8) | (1u << 15) | (1u << 16)), "lane.widthsComplete");
    check.require(coverage.ratesSeen == 0xffu, "lane.allEightRates");
    check.require(coverage.cases >= laneWidths.size() * 2, "lane.minimumFpCases");
    check.require(coverage.active >= 1000000 && coverage.masked >= 100000,
                  "lane.activeAndMaskedCoverage");
    check.require(coverage.copies >= 16 && coverage.restores >= 16 && coverage.resets >= 16,
                  "lane.copyRestoreResetCoverage");
    check.require(coverage.fpTransitions >= 64, "lane.fpTransitions");
    check.require(coverage.realScalarProcessCalls == coverage.active,
                  "lane.preregScalarProcessCalls");
    check.require(coverage.realVectorBackendCalls == 0,
                  "lane.preregNoImplementedVectorBackend");
    std::cout << "Preregistered original-scalar lane oracle: widths=" << laneWidths.size()
              << " rates=8 cases=" << coverage.cases << " lane-samples=" << coverage.compared
              << " active=" << coverage.active << " masked=" << coverage.masked
              << " copies=" << coverage.copies << " restores=" << coverage.restores
              << " resets=" << coverage.resets << " FP-transitions=" << coverage.fpTransitions
              << " direct-scalar-calls=" << coverage.realScalarProcessCalls
              << " vector-backend-calls=" << coverage.realVectorBackendCalls
              << " bit-checks=" << check.count << " mismatches=" << check.failures << '\n';
    return check.failures == 0 ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 1) { std::cerr << "usage: cs80_phase_lane_oracle_tests\n"; return 2; }
    (void)argv;
    return runLaneGrid();
}

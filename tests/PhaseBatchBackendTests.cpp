// Separate implementation-side backend witness. The original frozen scalar
// comparator and preregistered mixed-lane grid are intentionally untouched.
#define CS80_PHASE_LANE_ORACLE_INCLUDED_ONLY 1
#include "PhaseLaneOracleTests.cpp"

namespace {
struct BatchCoverage {
    std::uint64_t active = 0, masked = 0, packedGroups = 0;
    std::uint64_t packedSamples = 0, scalarSamples = 0, fallbackGroups = 0;
    std::uint64_t fpTransitions = 0, checks = 0;
    std::uint32_t widths = 0, rates = 0;
};

int runBatchBackendGrid() {
    FpEnvironment restoreCaller;
    const auto denormal = denormalModes();
    BatchCoverage tally;
    for (int width : laneWidths) {
        tally.widths |= 1u << width;
        for (std::size_t ri = 0; ri < roundModes.size(); ++ri) {
            if (std::fesetround(roundModes[ri]) != 0) continue;
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
                    tally.rates |= 1u << rateIndex;
                }
                std::vector<float> inputs(static_cast<std::size_t>(width));
                std::vector<bool> enabled(static_cast<std::size_t>(width));
                std::vector<Phase::Output> outputs(static_cast<std::size_t>(width));
                std::vector<bool> processed(static_cast<std::size_t>(width));
                for (std::uint64_t n = 0; n < sampleCount; ++n) {
                    check.sample = n;
                    if (n == 512) {
                        check.require(std::fesetround(roundModes[(ri + 1) % roundModes.size()]) == 0,
                                      "batch.fpTransitionAccepted");
                        ++tally.fpTransitions;
                    }
                    if (n == 1024) {
                        check.require(std::fesetround(roundModes[ri]) == 0,
                                      "batch.fpReturnAccepted");
                        ++tally.fpTransitions;
                    }
                    if (n == 2048) {
                        selectDenormalMode(denormal[(mi + 1) % denormal.size()]);
                        ++tally.fpTransitions;
                    }
                    if (n == 3072) {
                        selectDenormalMode(denormal[mi]);
                        ++tally.fpTransitions;
                    }
                    if (n == 1024 && width > 1) {
                        lanes.back().actual = lanes.front().actual;
                        lanes.back().reference = lanes.front().reference;
                    }
                    if (n == 2048) for (int l = 0; l < width; ++l)
                        if (l % 3 == 0) lanes[static_cast<std::size_t>(l)].restore();
                    if (n == 3072) for (int l = 0; l < width; ++l)
                        if (l % 5 == 0) lanes[static_cast<std::size_t>(l)].reset();
                    for (int l = 0; l < width; ++l) {
                        auto& lane=lanes[static_cast<std::size_t>(l)];
                        if (n == 512) lane.save();
                        const auto control=laneControls(l,n);
                        lane.set(control[0],control[1]);
                        inputs[static_cast<std::size_t>(l)]=laneInput(lane,l,n);
                        enabled[static_cast<std::size_t>(l)]=((n+std::uint64_t(l))%13)!=0;
                        processed[static_cast<std::size_t>(l)]=false;
                        if (enabled[static_cast<std::size_t>(l)]) ++tally.active;
                        else ++tally.masked;
                    }
#if defined(CS80_PHASE_BATCH_MUTATE_CROSS_LANE)
                    if (n==1 && width>=4) inputs[1]+=0.125f;
#endif
                    std::array<int,4> group {};
                    int filled=0;
                    for (int l=0;l<width;++l) {
                        if (!enabled[static_cast<std::size_t>(l)]) continue;
                        group[static_cast<std::size_t>(filled++)]=l;
                        if (filled!=4) continue;
                        Phase* selected[4];
                        float data[4];
                        Phase::Output result[4];
                        for (int k=0;k<4;++k) {
                            selected[k]=&lanes[static_cast<std::size_t>(group[k])].actual;
                            data[k]=inputs[static_cast<std::size_t>(group[k])];
                        }
                        const auto control=fpControl();
                        const bool used=Phase::tryProcessFour(selected,data,result);
                        check.equal(fpControl(),control,"batch.packedFpControlPreserved");
                        if (used) {
                            ++tally.packedGroups;
                            tally.packedSamples+=4;
                        } else {
                            ++tally.fallbackGroups;
                            for (int k=0;k<4;++k) {
                                result[k]=selected[k]->process(data[k]);
                                ++tally.scalarSamples;
                            }
                        }
                        for (int k=0;k<4;++k) {
                            const auto index=static_cast<std::size_t>(group[k]);
                            outputs[index]=result[k];
                            processed[index]=true;
                        }
                        filled=0;
                    }
                    for (int k=0;k<filled;++k) {
                        const auto index=static_cast<std::size_t>(group[k]);
                        outputs[index]=lanes[index].actual.process(inputs[index]);
                        ++tally.scalarSamples;
                        processed[index]=true;
                    }
#if defined(CS80_PHASE_BATCH_MUTATE_MASK)
                    if (n==1 && width>=4) (void)lanes[0].actual.process(0.25f);
#endif
#if defined(CS80_PHASE_BATCH_MUTATE_STATE)
                    if (n==1 && width>=4) lanes[1].actual.reset();
#endif
                    for (int l=0;l<width;++l) {
                        const auto index=static_cast<std::size_t>(l);
                        check.context="phase-packed/width="+std::to_string(width)
                                      +"/round="+std::to_string(roundModes[ri])
                                      +"/denormal="+std::to_string(denormal[mi])
                                      +"/lane="+std::to_string(l);
                        check.require(processed[index]==enabled[index],"batch.maskPreserved");
                        if (enabled[index]) {
                            const auto expected=lanes[index].reference.process(inputs[index]);
#if defined(CS80_PHASE_BATCH_MUTATE_OUTPUT)
                            if (n==1 && l==0 && width>=4)
                                outputs[index].lp=std::bit_cast<float>(
                                    std::bit_cast<std::uint32_t>(outputs[index].lp)^1u);
#endif
                            compareOutput(outputs[index],expected,"batch.output");
                        }
                        comparePhase(lanes[index].actual,lanes[index].reference);
                        if (check.failures!=0) {
                            std::cerr << "Phase packed bit divergence: failures=" << check.failures
                                      << " compared=" << check.count << '\n';
                            return 1;
                        }
                    }
                }
            }
        }
    }
    check.context="phase-packed/coverage";
    check.require(tally.widths==((1u<<1)|(1u<<2)|(1u<<3)|(1u<<4)|
                                (1u<<7)|(1u<<8)|(1u<<15)|(1u<<16)),"batch.allWidths");
    check.require(tally.rates==0xffu,"batch.allRates");
    check.require(tally.active>=1000000 && tally.masked>=100000,"batch.activeAndMask");
    check.require(tally.fpTransitions>=64,"batch.fpTransitions");
    check.require(tally.packedSamples==4*tally.packedGroups,"batch.realBackendCalls");
    check.require(tally.packedGroups>10000,"batch.actualPackedInstructionWork");
    check.require(tally.scalarSamples>100000,"batch.fallbackAndRemainderWork");
#if defined(CS80_PHASE_BATCH_MUTATE_COUNTER)
    ++tally.packedSamples;
    check.require(tally.packedSamples==4*tally.packedGroups,"batch.mutationCounterReject");
#endif
    std::cout << "Packed phase backend witness: widths=8 rates=8 active=" << tally.active
              << " masked=" << tally.masked << " four-lane-avx2-calls=" << tally.packedGroups
              << " vector-lane-processes=" << tally.packedSamples
              << " scalar-lane-processes=" << tally.scalarSamples
              << " scalar-fallback-groups=" << tally.fallbackGroups
              << " FP-transitions=" << tally.fpTransitions
              << " exact-bit-checks=" << check.count
              << " mismatches=" << check.failures << '\n';
    return check.failures==0 ? 0:1;
}
}

int main(int argc,char** argv) {
    if (argc!=1) { std::cerr << "usage: cs80_phase_batch_backend_tests\n"; return 2; }
    (void)argv;
    return runBatchBackendGrid();
}

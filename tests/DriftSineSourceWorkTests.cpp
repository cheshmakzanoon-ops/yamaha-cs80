// Test-only variant of the real Engine source measures actual tickRanks
// helper branch entries and actual remaining sinf sites. Never benchmark
// this instrumented build or ship these counters in production cs80_core.
#include "core/Engine.h"
#include "core/DriftSineExact.h"

#include <array>
#include <cstdio>
#include <cstdint>

#if !defined(CS80_DRIFT_SINE_SOURCE_WITNESS)
#error "This source-site test must compile and link instrumented Engine.cpp"
#endif

namespace {
struct Work {
    std::uint64_t fast=0,scalar=0;
};

Work sampleOriginalEngine(bool performance, bool isolation, float drift) {
    cs80::ParamValues p;
    p[cs80::P_parityMode]=isolation?1.0f:0.0f;
    p[cs80::P_sourcePerformance]=performance?1.0f:0.0f;
    p[cs80::P_drift]=drift;
    p[cs80::P_driftModel]=0.0f;
    p[cs80::P_analogNoise]=0.0f;
    cs80::Engine e;
    e.prepare(48000.0);
    e.setParams(p);
    for (int lane=0;lane<8;++lane) e.noteOn(48+2*lane,.8f);
    cs80::drift_opt::sourceWork={};
    std::array<float,512> out{};
    constexpr int frames=1536;
    for (int i=0;i<frames;i+=256) e.render(out.data(),256);
    return {cs80::drift_opt::sourceWork.fastZeroBranches,
            cs80::drift_opt::sourceWork.originalSineCalls};
}

bool require(bool condition, const char* name) {
    if (!condition) std::fprintf(stderr,"FAIL drift-work %s\n",name);
    return condition;
}
} // namespace

int main() {
    const auto isolation=sampleOriginalEngine(false,true,0.0f);
    const auto thermalZero=sampleOriginalEngine(true,false,0.0f);
    const auto thermalMoving=sampleOriginalEngine(true,false,.25f);
    const auto legacyMoving=sampleOriginalEngine(false,false,.25f);
    std::printf("Actual Engine.cpp site: PURE +0 fast=%llu realSinf=%llu; performance thermal +0 fast=%llu realSinf=%llu; thermal nonzero fast=%llu realSinf=%llu; legacy nonzero fast=%llu realSinf=%llu\n",
                (unsigned long long)isolation.fast,(unsigned long long)isolation.scalar,
                (unsigned long long)thermalZero.fast,(unsigned long long)thermalZero.scalar,
                (unsigned long long)thermalMoving.fast,(unsigned long long)thermalMoving.scalar,
                (unsigned long long)legacyMoving.fast,(unsigned long long)legacyMoving.scalar);
    if (!require(isolation.fast>10000,"work.realEngineIsolationFastEntered")) return 1;
    if (!require(thermalZero.fast>10000,"work.realEngineThermalZeroFastEntered")) return 1;
    if (!require(isolation.scalar>0 && isolation.scalar<isolation.fast/10,
                 "work.realEngineIsolationSavedSinfCalls")) return 1;
    if (!require(thermalZero.scalar>0 && thermalZero.scalar<thermalZero.fast/10,
                 "work.realEngineThermalSavedSinfCalls")) return 1;
    if (!require(thermalMoving.fast==0 && thermalMoving.scalar>10000,
                 "work.thermalMovingScalarFallback")) return 1;
    if (!require(legacyMoving.fast==0 && legacyMoving.scalar>10000,
                 "work.legacyMovingScalarFallback")) return 1;
    std::printf("PASS actual original Engine source sine-site work and drift fallbacks\n");
    return 0;
}

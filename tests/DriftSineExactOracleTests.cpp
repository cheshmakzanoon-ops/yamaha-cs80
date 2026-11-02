// Test-only, independently frozen Engine sine/drift software continuation.
// Preregister before any production Engine.cpp branch or FP change.
#include "core/Engine.h"
#include "fixtures/drift_sine/OriginalDrift.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

#if defined(__linux__) && defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <immintrin.h>
#define CS80_DRIFT_X86_FP 1
#else
#define CS80_DRIFT_X86_FP 0
#endif

#pragma STDC FENV_ACCESS ON

namespace {
namespace frozen = cs80::drift_original_20261102;

struct Checks {
    std::uint64_t compares=0, failures=0, sinePairs=0, zeroPairs=0;
    std::uint64_t thermalPairs=0, modes=0;
    int lane=0, caseIndex=0;
    const char* caseName="";
    template<class T> void equal(T got, T expect, const char* field) {
        static_assert(std::is_integral_v<T> || std::is_same_v<T,float>);
        ++compares;
        bool ok=false;
        if constexpr (std::is_same_v<T,float>)
            ok=std::bit_cast<std::uint32_t>(got)==std::bit_cast<std::uint32_t>(expect);
        else ok=got==expect;
        if (!ok) {
            ++failures;
            if (failures<=16) {
                std::fprintf(stderr,"FAIL %s case=%d lane=%d %s",caseName,caseIndex,lane,field);
                if constexpr (std::is_same_v<T,float>)
                    std::fprintf(stderr," bits=0x%08x/0x%08x\n",
                                 std::bit_cast<std::uint32_t>(got),std::bit_cast<std::uint32_t>(expect));
                else std::fprintf(stderr," actual=%llu expected=%llu\n",
                                  static_cast<unsigned long long>(got),
                                  static_cast<unsigned long long>(expect));
            }
        }
    }
    void require(bool ok, const char* field) { equal(ok,true,field); }
} check;

struct FpRestore {
    std::fenv_t old{};
#if CS80_DRIFT_X86_FP
    unsigned mxcsr=0;
#endif
    FpRestore() {
        std::fegetenv(&old);
#if CS80_DRIFT_X86_FP
        mxcsr=_mm_getcsr();
#endif
    }
    ~FpRestore() {
        std::fesetenv(&old);
#if CS80_DRIFT_X86_FP
        _mm_setcsr(mxcsr);
#endif
    }
};

std::uint64_t fpControls() {
    const auto round=static_cast<std::uint32_t>(std::fegetround());
#if CS80_DRIFT_X86_FP
    return (std::uint64_t(_mm_getcsr() & ~0x3fu)<<32) | round;
#else
    return round;
#endif
}

std::vector<unsigned> denormalModes() {
#if CS80_DRIFT_X86_FP
    alignas(16) std::array<unsigned char,512> area{};
    _fxsave(area.data());
    unsigned mask=0;
    std::memcpy(&mask,area.data()+28,sizeof(mask));
    if (mask==0) mask=0xffbfu;
    std::vector<unsigned> out{0};
    if (mask & (1u<<15)) out.push_back(1u<<15);
    if (mask & (1u<<6)) out.push_back(1u<<6);
    if ((mask & 0x8040u)==0x8040u) out.push_back(0x8040u);
    return out;
#else
    return {0};
#endif
}

void selectDenormal(unsigned mode) {
#if CS80_DRIFT_X86_FP
    _mm_setcsr((_mm_getcsr() & ~0x8040u) | mode);
#else
    (void)mode;
#endif
}

// This is the pre-optimization current-source positive control, deliberately
// separate from the fixture. After implementation, a real source helper and
// source-site work witness must replace this control; the fixture stays frozen.
float scalarSourceControl(float drift,float phase,float offset) {
    return drift * std::sin(cs80::kTwoPi * (phase+offset)) * 0.9f;
}

float scalarThermalControl(bool performance,int model,float drift,float rankCents,float history) {
    return performance && model==0
               ? drift / cs80::tolerance::kDefaultDrift * rankCents * 0.01f
               : history;
}

int frozenSineGrid() {
    FpRestore restore;
    constexpr std::array<int,4> rounds {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    constexpr std::array<float,5> drifts {
        0.0f,-0.0f,0.25f,-0.25f,std::numeric_limits<float>::denorm_min()
    };
    const auto denorm=denormalModes();
    std::array<float,256> phases{};
    for (int i=0;i<256;++i) phases[static_cast<std::size_t>(i)]=(float)i/257.0f;
    phases[0]=0.0f;
    phases[1]=0.5f;
    phases[2]=std::nextafter(0.5f,0.0f);
    phases[3]=std::nextafter(0.5f,1.0f);
    phases[4]=std::nextafter(1.0f,0.0f);
    phases[5]=1.0f;
    phases[6]=-0.0f;
    for (int round:rounds) {
        if (std::fesetround(round)!=0) continue;
        for (unsigned dm:denorm) {
            selectDenormal(dm);
            ++check.modes;
            for (int di=0;di<(int)drifts.size();++di) {
                for (int n=0;n<(int)phases.size();++n) {
                    const float phase=phases[static_cast<std::size_t>(n)];
                    for (int lane=0;lane<8;++lane) {
                        check.caseName="drift-independent-frozen-bits";
                        check.caseIndex=n*5+di;
                        check.lane=lane;
                        const float offset=(float)lane * 0.73f;
                        const auto beforeControls=fpControls();
                        std::feclearexcept(FE_ALL_EXCEPT);
                        if ((n&7)==0) std::feraiseexcept(FE_INEXACT);
                        std::fenv_t start{};
                        std::fegetenv(&start);
                        float candidateOffset=offset;
#if defined(CS80_DRIFT_MUTATE_CROSS_LANE)
                        if (n==7 && lane==1) candidateOffset+=(float)0.17f;
#endif
                        auto candidate=scalarSourceControl(drifts[di],phase,candidateOffset);
#if defined(CS80_DRIFT_MUTATE_UNSAFE_ZERO_SKIP)
                        // Naively replacing the sine with the incoming signed
                        // zero loses sign across half-cycles and exception bits.
                        if (drifts[di]==0.0f) candidate=drifts[di];
#endif
#if defined(CS80_DRIFT_MUTATE_ZERO_SIGN)
                        if (di==0 && n==0 && lane==0)
                            candidate=std::bit_cast<float>(std::bit_cast<std::uint32_t>(candidate) ^ 0x80000000u);
#endif
#if defined(CS80_DRIFT_MUTATE_FLAGS)
                        if ((n&7)==0) std::feclearexcept(FE_ALL_EXCEPT);
#endif
                        const auto candFlags=std::fetestexcept(FE_ALL_EXCEPT);
                        check.equal(fpControls(),beforeControls,"drift.sourceFpControl");
                        std::fesetenv(&start);
                        const auto expected=frozen::sine(drifts[di],phase,offset);
                        const auto frozenFlags=std::fetestexcept(FE_ALL_EXCEPT);
                        check.equal(fpControls(),beforeControls,"drift.frozenFpControl");
                        check.equal(candidate,expected,"drift.signedZeroAndSineBits");
                        check.equal(candFlags,frozenFlags,"drift.fpStickyAndExceptions");
                        ++check.sinePairs;
                        if (candidate==0.0f) ++check.zeroPairs;
                        const float rankCents=(float)((lane-4)*3.25);
                        for (int policy=0;policy<3;++policy) {
                            std::fesetenv(&start);
                            auto got=scalarThermalControl(policy!=0,policy==2?1:0,
                                                          drifts[di],rankCents,candidate);
#if defined(CS80_DRIFT_MUTATE_THERMAL)
                            if (policy==1 && n==7 && lane==1) got+=.0001f;
#endif
                            const auto afterCandidateFlags=std::fetestexcept(FE_ALL_EXCEPT);
                            std::fesetenv(&start);
                            const auto truth=frozen::term(policy!=0,policy==2?1:0,
                                                          drifts[di],rankCents,expected);
                            const auto afterFrozenFlags=std::fetestexcept(FE_ALL_EXCEPT);
                            check.equal(got,truth,"drift.thermalOrWornTermBits");
                            check.equal(afterCandidateFlags,afterFrozenFlags,"drift.selectionFpExceptions");
                            ++check.thermalPairs;
                        }
                        if (check.failures) return 1;
                    }
                }
            }
        }
    }
    check.caseName="drift-grid-coverage";
    check.require(check.modes>=4,"drift.fpModesPresent");
    check.require(check.sinePairs>=10000 && check.zeroPairs>=1000,"drift.signedZeroCoverage");
    check.require(check.thermalPairs>=30000,"drift.thermalCoverage");
    std::printf("Frozen drift: modes=%llu sine-pairs=%llu signed-zero=%llu selection-pairs=%llu checks=%llu failures=%llu; production fast calls=0\n",
                (unsigned long long)check.modes,(unsigned long long)check.sinePairs,
                (unsigned long long)check.zeroPairs,(unsigned long long)check.thermalPairs,
                (unsigned long long)check.compares,(unsigned long long)check.failures);
    return check.failures==0?0:1;
}

struct Hash64 {
    std::uint64_t x=0xcbf29ce484222325ull;
    void bytes(std::uint64_t bits,int n) {
        for (int b=0;b<n;++b) { x^=(bits>>(8*b))&255u; x*=0x100000001b3ull; }
    }
    template<class T> void put(T value) {
        if constexpr (std::is_same_v<T,float>) bytes(std::bit_cast<std::uint32_t>(value),4);
        else if constexpr (std::is_same_v<T,double>) bytes(std::bit_cast<std::uint64_t>(value),8);
        else if constexpr (std::is_integral_v<T>) bytes((std::uint64_t)value,sizeof(T));
    }
};
struct HashPair { std::uint64_t audio, state; };

void hashState(Hash64& h,const cs80::Engine& e) {
    for (int vi=0;vi<8;++vi) {
        auto v=e.getVoiceInfo(vi);
        h.put(v.active); h.put(v.held); h.put(v.pedalHeld);
        h.put(v.note); h.put(v.assignmentSerial);
        h.put(v.velocity); h.put(v.pressure);
        h.put(v.touchA); h.put(v.touchB); h.put(v.touchC);
        for (int rank=0;rank<2;++rank) {
            auto r=e.getRankInfo(vi,rank);
            h.put(r.hpfHz); h.put(r.lpfHz); h.put(r.oscillatorHz);
            h.put(r.filterEnvelope); h.put(r.ampEnvelope);
            h.put(r.keyVolts); h.put(r.hpfQ); h.put(r.lpfQ);
        }
    }
    auto q=e.getSourceSharedReadout();
    for (int rank=0;rank<2;++rank) {
        h.put(q.levelBusVolts[rank]);
        for (int vi=0;vi<8;++vi) {
            h.put(q.levelGain[rank][vi]); h.put(q.levelPinVolts[rank][vi]);
            for (int side=0;side<2;++side) {
                h.put(q.q[rank][vi][side]); h.put(q.qVolts[rank][vi][side]);
            }
        }
    }
    h.put(q.masterResVolts); h.put(q.masterResSaturated); h.put(q.saturatedVoices);
    for (int tap=0;tap<3;++tap) {
        auto o=e.getOutputTapState(tap);
        h.put(o.state.capacitorVolts); h.put(o.state.previousSourceVolts);
        h.put(o.state.readoutGain); h.put(o.nodes.capacitorVolts);
        h.put(o.nodes.commonVolts); h.put(o.nodes.jackVolts); h.put(o.nodes.audibleJackVolts);
    }
    h.put(e.getSubOscillatorRate());
}

cs80::ParamValues paramsFor(int scenario) {
    cs80::ParamValues p;
    p[cs80::P_parityMode]=scenario==0?1.0f:0.0f;
    p[cs80::P_sourcePerformance]=(scenario>=1 && scenario<=3)?1.0f:0.0f;
    p[cs80::P_drift]=(scenario>=2)?.25f:0.0f;
    p[cs80::P_driftModel]=scenario==3?1.0f:0.0f;
    p[cs80::P_voiceDispersion]=0.0f;
    p[cs80::P_analogNoise]=0.0f;
    for (int rank=0;rank<2;++rank) {
        p[cs80::chParam(rank,cs80::CP_initBrill)]=.4f;
        p[cs80::chParam(rank,cs80::CP_initLevel)]=.3f;
        p[cs80::chParam(rank,cs80::CP_afterBrill)]=.35f;
        p[cs80::chParam(rank,cs80::CP_afterLevel)]=.25f;
    }
    return p;
}

HashPair trace(int scenario,int callback) {
    cs80::Engine e;
    e.prepare(48000.0);
    e.setParams(paramsFor(scenario));
    Hash64 audio,state;
    std::array<float,512> out{};
    constexpr int end=24576;
    constexpr std::array<int,4> events {17,8193,22529,23553};
    for (int frame=0;frame<end;) {
        switch (frame) {
            case 17:
            case 22529:
                for (int vi=0;vi<8;++vi) e.noteOn(48+vi*2,.80f);
                break;
            case 8193:
            case 23553:
                for (int vi=0;vi<8;++vi) e.noteOff(48+vi*2);
                break;
        }
        int next=end;
        for (int event:events) if (event>frame) { next=event; break; }
        const int checkpoint=(frame/256+1)*256;
        const int n=std::min({callback,end-frame,next-frame,checkpoint-frame});
        e.render(out.data(),n);
        for (int i=0;i<2*n;++i) audio.put(out[static_cast<std::size_t>(i)]);
        frame+=n;
        if (frame==checkpoint) hashState(state,e);
    }
#if defined(CS80_DRIFT_MUTATE_ENGINE_GOLDEN)
    if (scenario==0) audio.x^=1u; // identical corruption for all partitions
#endif
    return {audio.x,state.x};
}

// Filled from --record-original on the unchanged source tree, qualified only
// for this original-host compiler and libm. Not a substitute for strict V2.
constexpr std::array<HashPair,5> originalHostGolden {{
    {0xece24f4e0b81d431ull,0xcbe3430e20a0cf6bull}, // PURE isolation, drift forced +0
    {0xece24f4e0b81d431ull,0xcbe3430e20a0cf6bull}, // performance thermal, drift +0
    {0x945bd7421116ecbdull,0x710e1db62e155520ull}, // performance thermal, +0.25
    {0x44cfefe44b55c0c1ull,0x6adbecc0c679115aull}, // performance worn sine, +0.25
    {0xf5574236042a1539ull,0xace3f1e915a0094dull}  // legacy sine, +0.25
}};

bool exactGoldenHost() {
#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBC__) && defined(__GNUC__)
    return __GNUC__==13 && __GNUC_MINOR__==3 &&
           std::string_view(gnu_get_libc_version())=="2.39";
#else
    return false;
#endif
}

int engineOracle(bool record) {
    FpRestore restore;
    std::fesetround(FE_TONEAREST);
    selectDenormal(0);
    check.caseName="engine-original-branch-partitions";
    for (int scenario=0;scenario<5;++scenario) {
        check.caseIndex=scenario; check.lane=-1;
        const auto one=trace(scenario,1);
        const auto seventeen=trace(scenario,17);
        const auto twoFiftySix=trace(scenario,256);
        if (record)
            std::printf("Original Engine branch scenario=%d: 1={0x%016llx,0x%016llx} 17={0x%016llx,0x%016llx} 256={0x%016llx,0x%016llx}\n",
                        scenario,(unsigned long long)one.audio,(unsigned long long)one.state,
                        (unsigned long long)seventeen.audio,(unsigned long long)seventeen.state,
                        (unsigned long long)twoFiftySix.audio,(unsigned long long)twoFiftySix.state);
        check.equal(one.audio,seventeen.audio,"engine.audio.1vs17");
        check.equal(one.audio,twoFiftySix.audio,"engine.audio.1vs256");
        check.equal(one.state,seventeen.state,"engine.state.1vs17");
        check.equal(one.state,twoFiftySix.state,"engine.state.1vs256");
        if (!record && exactGoldenHost()) {
            check.equal(one.audio,originalHostGolden[scenario].audio,"engine.frozenAudioBits");
            check.equal(one.state,originalHostGolden[scenario].state,"engine.frozenStateBits");
        }
        if (check.failures) return 1;
    }
    std::printf("Engine frozen branch golden: original-host=%s policies=5 partitions=1/17/256 samples=24576 checks=%llu mismatches=%llu\n",
                exactGoldenHost()?"yes":"foreign-host-skip",(unsigned long long)check.compares,
                (unsigned long long)check.failures);
    return check.failures==0?0:1;
}

} // namespace

int main(int argc,char** argv) {
    const bool record=argc==2 && std::string_view(argv[1])=="--record-original";
    if (!(argc==1 || record)) {
        std::fprintf(stderr,"usage: cs80_drift_sine_exact_oracle_tests [--record-original]\n");
        return 2;
    }
    if (frozenSineGrid()!=0) return 1;
    if (engineOracle(record)!=0) return 1;
    std::printf("Independent frozen drift sine: total-checks=%llu mismatches=%llu\n",
                (unsigned long long)check.compares,(unsigned long long)check.failures);
    return check.failures==0?0:1;
}

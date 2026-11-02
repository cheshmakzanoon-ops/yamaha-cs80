// Preregistered test-only continuation: original TWS touch arithmetic frozen
// before any proposed idle fixed-point shortcut.  Do not regenerate the frozen
// header from an optimized production source.  The engine checkpoints below
// also pin global/readout and wake behavior at host block sizes 1/17/256.
#include "core/Engine.h"
#include "core/TouchCircuit.h"
#include "fixtures/touch_idle/TouchCircuitBefore.h"

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
#define CS80_TOUCH_IDLE_X86_FP 1
#else
#define CS80_TOUCH_IDLE_X86_FP 0
#endif

#pragma STDC FENV_ACCESS ON

namespace {
namespace original = cs80::touch_original_20261102;
namespace current = cs80::touch;

struct Checks {
    std::uint64_t compared = 0, failures = 0, touchSamples = 0, busSamples = 0;
    std::uint64_t zeroSamples = 0, copies = 0, resets = 0, triggers = 0;
    const char* caseName = "";
    std::uint64_t sample = 0;
    int lane = 0;

    template<class T> void equal(T actual, T expected, const char* field) {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double> || std::is_integral_v<T>);
        ++compared;
        bool same = false;
        if constexpr (std::is_same_v<T, double>)
            same = std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected);
        else if constexpr (std::is_same_v<T, float>)
            same = std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected);
        else same = actual == expected;
        if (!same) {
            ++failures;
            if (failures <= 16) {
                std::fprintf(stderr, "FAIL %s sample=%llu lane=%d %s",
                             caseName, static_cast<unsigned long long>(sample), lane, field);
                if constexpr (std::is_same_v<T, double>)
                    std::fprintf(stderr, " bits=0x%016llx/0x%016llx\n",
                                 static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(actual)),
                                 static_cast<unsigned long long>(std::bit_cast<std::uint64_t>(expected)));
                else if constexpr (std::is_same_v<T, float>)
                    std::fprintf(stderr, " bits=0x%08x/0x%08x\n",
                                 std::bit_cast<std::uint32_t>(actual), std::bit_cast<std::uint32_t>(expected));
                else std::fprintf(stderr, " actual=%lld expected=%lld\n",
                                  static_cast<long long>(actual), static_cast<long long>(expected));
            }
        }
    }
    void require(bool condition, const char* field) { equal(condition, true, field); }
} check;

struct FpRestore {
    std::fenv_t env {};
#if CS80_TOUCH_IDLE_X86_FP
    unsigned csr = _mm_getcsr();
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t fpcr = 0;
#endif
    FpRestore() {
        std::fegetenv(&env);
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
#endif
    }
    ~FpRestore() {
        std::fesetenv(&env);
#if CS80_TOUCH_IDLE_X86_FP
        _mm_setcsr(csr);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
    }
};

std::uint64_t fpControls() {
    const std::uint64_t round = static_cast<std::uint32_t>(std::fegetround());
#if CS80_TOUCH_IDLE_X86_FP
    return (std::uint64_t(_mm_getcsr() & ~0x3fu) << 32) | round;
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t x=0; __asm__ volatile("mrs %0, fpcr" : "=r"(x));
    return (x << 32) | round;
#else
    return round;
#endif
}

std::vector<unsigned> fpDenormalModes() {
#if CS80_TOUCH_IDLE_X86_FP
    alignas(16) std::array<unsigned char, 512> area {};
    _fxsave(area.data());
    unsigned mask=0; std::memcpy(&mask, area.data()+28, sizeof(mask));
    if (mask==0) mask=0xffbfu;
    std::vector<unsigned> values {0};
    if (mask & (1u<<15)) values.push_back(1u<<15);
    if (mask & (1u<<6)) values.push_back(1u<<6);
    if ((mask & 0x8040u)==0x8040u) values.push_back(0x8040u);
    return values;
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    return {0, 1u<<24};
#else
    return {0};
#endif
}

void selectFpDenormal(unsigned bits) {
#if CS80_TOUCH_IDLE_X86_FP
    _mm_setcsr((_mm_getcsr() & ~0x8040u) | bits);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t x=0; __asm__ volatile("mrs %0, fpcr" : "=r"(x));
    x=(x & ~(std::uint64_t(1)<<24)) | bits;
    __asm__ volatile("msr fpcr, %0" : : "r"(x));
#else
    (void)bits;
#endif
}

template<class A,class B> void compareCoefficients(const A& a, const B& b) {
    check.equal(a.a,b.a,"coefficient.a");
    check.equal(a.c,b.c,"coefficient.c");
    check.equal(a.acquire,b.acquire,"coefficient.acquire");
    check.equal(a.leak,b.leak,"coefficient.leak");
    check.equal(a.contactSample,b.contactSample,"coefficient.contactSample");
    check.equal(a.windowSample,b.windowSample,"coefficient.windowSample");
    check.equal(a.contactBefore,b.contactBefore,"coefficient.contactBefore");
    check.equal(a.contactAfter,b.contactAfter,"coefficient.contactAfter");
    check.equal(a.windowPart,b.windowPart,"coefficient.windowPart");
}

template<class A,class B> void compareTouch(const A& a, const B& b, const char* prefix) {
    (void)prefix;
    check.equal(a.a,b.a,"touch.a");
    check.equal(a.b,b.b,"touch.b");
    check.equal(a.c,b.c,"touch.C.signedZeroAndWake");
}

double pressureAt(int lane, std::uint64_t n) {
    if (n>=320 && n<650 && lane%3==0) return .73;
    if (n>=2800 && n<3100 && lane%2==1) return .41;
    if (n>=5000 && n<5150 && lane%3==1) return .97;
    return 0.0;
}

int touchFrozenGrid() {
    FpRestore restore;
    constexpr std::array<int,4> rounds {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO};
    constexpr std::array<double,3> rates {48000,96000,384000};
    const auto denormal=fpDenormalModes();
    std::uint64_t fpCases=0;
    for (int round : rounds) {
        if (std::fesetround(round) != 0) continue;
        for (unsigned dm : denormal) {
            selectFpDenormal(dm);
            for (double rate : rates) for (double leakSeconds : {0.0, 3.3}) {
                ++fpCases;
                check.caseName="original-TWS-frozen-continuation";
                current::Coefficients k;
                original::Coefficients refK;
                k.prepare(rate, leakSeconds);
                refK.prepare(rate, leakSeconds);
                compareCoefficients(k,refK);
                std::array<current::TouchVoice,8> live {};
                std::array<original::TouchVoice,8> frozen {};
                for (std::uint64_t n=0; n<8192; ++n) {
                    check.sample=n;
                    if (n==128 || n==257 || n==4097) {
                        const int lane=(n==128 ? 0 : n==257 ? 3 : 5);
                        const double strike=n==4097 ? .2 : .8;
                        live[lane].trigger(strike);
                        frozen[lane].trigger(strike);
                        ++check.triggers;
                    }
                    if (n==512) { live[7]=live[0]; frozen[7]=frozen[0]; ++check.copies; }
                    if (n==2048) { live[6].reset(); frozen[6].reset(); ++check.resets; }
                    std::array<double,8> a{}, b{};
                    double topA=0.0, topB=0.0;
                    for (int lane=0; lane<8; ++lane) {
                        check.lane=lane;
                        const double pressure=pressureAt(lane,n);
                        const bool stickyProbe=lane==0 && (n==0 || n==321 || n==4097);
                        if (stickyProbe) {
                            std::feclearexcept(FE_ALL_EXCEPT);
                            std::feraiseexcept(FE_INEXACT);
                        }
                        const auto fp=fpControls();
                        double candidatePressure=pressure;
#if defined(CS80_TOUCH_IDLE_MUTATE_CROSS_LANE)
                        if (n==321 && lane==1) candidatePressure+=.125;
#endif
                        auto got=live[lane].tick(k,candidatePressure);
#if defined(CS80_TOUCH_IDLE_MUTATE_STICKY)
                        if (stickyProbe) std::feclearexcept(FE_ALL_EXCEPT);
#endif
                        if (stickyProbe)
                            check.require((std::fetestexcept(FE_ALL_EXCEPT)&FE_INEXACT)!=0,
                                          "touch.candidateStickyFlagsPreserved");
                        check.equal(fpControls(),fp,"touch.candidateFpControl");
                        const auto expected=frozen[lane].tick(refK,pressure);
                        check.equal(fpControls(),fp,"touch.frozenFpControl");
#if defined(CS80_TOUCH_IDLE_MUTATE_ZERO_SIGN)
                        if (n==0 && lane==0)
                            got.c=std::bit_cast<double>(std::bit_cast<std::uint64_t>(got.c) ^ 0x8000000000000000ull);
#endif
                        compareTouch(got,expected,"output");
                        compareTouch(live[lane].state(),frozen[lane].state(),"observable-state");
                        a[lane]=got.a;
                        b[lane]=expected.a;
                        topA=std::max(topA,a[lane]);
                        topB=std::max(topB,b[lane]);
                        if (got.a==0 && got.b==0) ++check.zeroSamples;
                        ++check.touchSamples;
                        if (check.failures) return 1;
                    }
#if defined(CS80_TOUCH_IDLE_MUTATE_BUS)
                    if (n==321) a[1]=a[0]+.2;
#endif
                    static constexpr int iterations=2;
                    const double busA=topA>0.0 ? current::busVolts(a,0.0,iterations) : 0.0;
                    const double busB=topB>0.0 ? original::busVolts(b,0.0,iterations) : 0.0;
                    check.lane=-1;
                    check.equal(busA,busB,"TRG5.busVolts");
                    ++check.busSamples;
                    if (check.failures) return 1;
                }
            }
        }
    }
    check.caseName="TWS-coverage"; check.sample=0; check.lane=0;
    check.require(fpCases>=24,"touch.roundAndDenormalCoverage");
    check.require(check.touchSamples>=1000000 && check.zeroSamples>=100000,
                  "touch.fixedStateAndMovingCoverage");
    check.require(check.copies>=24 && check.resets>=24 && check.triggers>=24,
                  "touch.copyResetTriggerCoverage");
    std::printf("Original frozen touch grid: FP-cases=%llu samples=%llu zero-exposed=%llu bus-samples=%llu copies=%llu resets=%llu triggers=%llu checks=%llu mismatches=%llu\n",
                (unsigned long long)fpCases,(unsigned long long)check.touchSamples,
                (unsigned long long)check.zeroSamples,(unsigned long long)check.busSamples,
                (unsigned long long)check.copies,(unsigned long long)check.resets,
                (unsigned long long)check.triggers,(unsigned long long)check.compared,
                (unsigned long long)check.failures);
    return check.failures==0 ? 0 : 1;
}

struct Hash64 {
    std::uint64_t value=0xcbf29ce484222325ull;
    void bits(std::uint64_t x, int width) {
        for (int b=0;b<width;++b) {
            value ^= static_cast<unsigned char>(x>>(8*b));
            value *= 0x100000001b3ull;
        }
    }
    template<class T> void put(T x) {
        if constexpr (std::is_same_v<T,float>) bits(std::bit_cast<std::uint32_t>(x),4);
        else if constexpr (std::is_same_v<T,double>) bits(std::bit_cast<std::uint64_t>(x),8);
        else if constexpr (std::is_integral_v<T>) bits(static_cast<std::uint64_t>(x),sizeof(T));
    }
};
struct EngineHashes { std::uint64_t audio=0, globals=0; };

void hashGlobalCheckpoint(Hash64& h, const cs80::Engine& engine) {
    for (int vi=0;vi<8;++vi) {
        const auto v=engine.getVoiceInfo(vi);
        h.put(v.active); h.put(v.held); h.put(v.pedalHeld);
        h.put(v.note); h.put(v.assignmentSerial);
        h.put(v.velocity); h.put(v.pressure);
        h.put(v.touchA); h.put(v.touchB); h.put(v.touchC);
        for (int rank=0;rank<2;++rank) {
            const auto r=engine.getRankInfo(vi,rank);
            h.put(r.hpfHz); h.put(r.lpfHz);
            h.put(r.filterEnvelope); h.put(r.ampEnvelope);
            h.put(r.oscillatorHz); h.put(r.keyVolts);
            h.put(r.hpfQ); h.put(r.lpfQ);
        }
    }
    const auto q=engine.getSourceSharedReadout();
    for (int rank=0;rank<2;++rank) {
        h.put(q.levelBusVolts[rank]);
        for (int v=0;v<8;++v) {
            h.put(q.levelGain[rank][v]);
            h.put(q.levelPinVolts[rank][v]);
            for (int side=0;side<2;++side) {
                h.put(q.q[rank][v][side]);
                h.put(q.qVolts[rank][v][side]);
            }
        }
    }
    h.put(q.masterResVolts);
    h.put(q.saturatedVoices);
    h.put(q.masterResSaturated);
    for (int tap=0;tap<3;++tap) {
        const auto t=engine.getOutputTapState(tap);
        h.put(t.state.capacitorVolts); h.put(t.state.previousSourceVolts);
        h.put(t.state.readoutGain);
        h.put(t.nodes.capacitorVolts); h.put(t.nodes.commonVolts);
        h.put(t.nodes.jackVolts); h.put(t.nodes.audibleJackVolts);
    }
    h.put(engine.getSubOscillatorRate());
}

cs80::ParamValues engineParams(bool performance) {
    cs80::ParamValues p;
    p[cs80::P_parityMode]=performance ? 0.0f : 1.0f;
    p[cs80::P_sourcePerformance]=performance ? 1.0f : 0.0f;
    p[cs80::P_voiceDispersion]=p[cs80::P_drift]=0.0f;
    p[cs80::P_analogNoise]=performance ? .015f : 0.0f;
    p[cs80::P_detune]=0.0f;
    p[cs80::P_fx1Type]=performance ? 5.0f : 0.0f;
    p[cs80::P_fx1Mix]=performance ? .4f : 0.0f;
    for (int rank=0;rank<2;++rank) {
        p[cs80::chParam(rank,cs80::CP_initBrill)]=.4f;
        p[cs80::chParam(rank,cs80::CP_afterBrill)]=.35f;
        p[cs80::chParam(rank,cs80::CP_initLevel)]=.25f;
        p[cs80::chParam(rank,cs80::CP_afterLevel)]=.3f;
        p[cs80::chParam(rank,cs80::CP_vS)]=1.0f;
    }
    p[cs80::P_initBend]=.6f;
    return p;
}

EngineHashes engineTrace(bool performance, int block) {
    cs80::Engine engine;
    engine.prepare(48000.0);
    engine.setParams(engineParams(performance));
    Hash64 audio, globals;
    std::array<float,512> buffer {};
    constexpr int end=150528;
    constexpr std::array<int,10> events {17,257,513,769,2049,2305,4097,4353,150001,150257};
    for (int frame=0;frame<end;) {
        switch (frame) {
            case 17: engine.noteOn(60,.8f); break;
            case 257: engine.polyPressure(60,.65f); break;
            case 513: engine.noteOff(60); break;
            case 769: engine.setRibbon(.15f); break;
            case 2049: engine.noteOn(64,.4f); break;
            case 2305: engine.noteOff(64); break;
            case 4097: engine.noteOn(67,.9f); break;
            case 4353: engine.noteOff(67); break;
            case 150001: engine.noteOn(72,.6f); break;
            case 150257: engine.noteOff(72); break;
        }
        int nextEvent=end;
        for (int event : events) if (event>frame) { nextEvent=event; break; }
        const int checkpoint=(frame/256+1)*256;
        const int count=std::min({block,end-frame,checkpoint-frame,nextEvent-frame});
        engine.render(buffer.data(),count);
        for (int i=0;i<count*2;++i) audio.put(buffer[static_cast<std::size_t>(i)]);
        frame+=count;
        if (frame==checkpoint) hashGlobalCheckpoint(globals,engine);
    }
#if defined(CS80_TOUCH_IDLE_MUTATE_ENGINE_GOLDEN)
    // Corrupt the same policy's audio fingerprint in *all* three partitions.
    // The cross-partition equality still passes; only the independent frozen
    // source digest can reject this, which guards against a self-comparison.
    if (!performance) audio.value ^= 1u;
#endif
    return {audio.value,globals.value};
}

// These are immutable host-specific observations of the unchanged feature
// source, filled only after the baseline test-only capture. They do not replace
// the cross-platform, independent frozen TWS comparator or strict V2 captures.
constexpr std::array<EngineHashes,2> recordedOriginalEngine {{
    {0x7d101956e93b525dull,0x6046acf7ccf9a6d7ull}, // PURE isolation
    {0x70ba56de3cba74adull,0xa4cedcc115867e62ull}  // sourcePerformance + modern FX/noise
}};

bool exactEngineGoldenHost() {
#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBC__) && defined(__GNUC__)
    // The whole-engine golden is deliberately scoped to the compiler and libm
    // runtime of the original capture. Every other host still runs the
    // independent frozen TWS oracle and exact 1/17/256 partition equality.
    return __GNUC__==13 && __GNUC_MINOR__==3 &&
           std::string_view(gnu_get_libc_version())=="2.39";
#else
    return false;
#endif
}

int enginePartitionOracle(bool record) {
    FpRestore restore;
    std::fesetround(FE_TONEAREST);
    selectFpDenormal(0);
    check.caseName="engine-touch-idle-global-partitions";
    const bool pinnedHost=exactEngineGoldenHost();
    std::printf("Original-engine golden scope: %s\n", pinnedHost ?
                "Linux x86_64 GCC13.3 glibc2.39; require both immutable original digests" :
                "other runtime; require frozen TWS + exact engine partition parity, no cross-host digest claim");
    for (int mode=0;mode<2;++mode) {
        const auto one=engineTrace(mode==1,1);
        const auto seventeen=engineTrace(mode==1,17);
        const auto twoFiftySix=engineTrace(mode==1,256);
        if (record)
            std::printf("Engine baseline mode=%d: {0x%016llx,0x%016llx} 17={0x%016llx,0x%016llx} 256={0x%016llx,0x%016llx}\n",
                        mode,(unsigned long long)one.audio,(unsigned long long)one.globals,
                        (unsigned long long)seventeen.audio,(unsigned long long)seventeen.globals,
                        (unsigned long long)twoFiftySix.audio,(unsigned long long)twoFiftySix.globals);
        check.equal(one.audio,seventeen.audio,"engine.audio.1vs17");
        check.equal(one.audio,twoFiftySix.audio,"engine.audio.1vs256");
        check.equal(one.globals,seventeen.globals,"engine.globals.1vs17");
        check.equal(one.globals,twoFiftySix.globals,"engine.globals.1vs256");
        if (!record && pinnedHost) {
            check.equal(one.audio,recordedOriginalEngine[mode].audio,"engine.frozenAudioBits");
            check.equal(one.globals,recordedOriginalEngine[mode].globals,"engine.frozenGlobalBits");
        }
        if (check.failures) return 1;
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const bool record=argc==2 && std::string_view(argv[1])=="--record-engine-original";
    if (!(argc==1 || record)) {
        std::fprintf(stderr,"usage: cs80_touch_idle_exact_oracle_tests [--record-engine-original]\n");
        return 2;
    }
    if (touchFrozenGrid()!=0) return 1;
    if (enginePartitionOracle(record)!=0) return 1;
    std::printf("Frozen TWS and engine global oracle: %llu checks, %llu bit mismatches\n",
                (unsigned long long)check.compared,(unsigned long long)check.failures);
    return check.failures==0 ? 0 : 1;
}

// Compile Engine.cpp independently with CS80_PHASE_ENGINE_WORK_WITNESS.
// The scalar comparator is the *original Engine branch*, selected before
// calling tickRanks; the immutable frozen phase/touch and original-host Engine
// golden fixtures are separate prerequisites, not rewritten by this test.
#include "core/Engine.h"

#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

namespace cs80::phase_cpu_witness {
extern std::uint64_t qualifiedSamples,hpVectorGroups,lpVectorGroups;
extern std::uint64_t hpScalarFallbackGroups,lpScalarFallbackGroups;
extern std::uint64_t rankRngCalls,rankRngOrderHash;
extern bool forceScalar;
}

namespace {
struct FpRestore {
    std::fenv_t saved {};
#if defined(__x86_64__) || defined(_M_X64)
    unsigned csr=_mm_getcsr();
#endif
    FpRestore() { std::fegetenv(&saved); }
    ~FpRestore() {
        std::fesetenv(&saved);
#if defined(__x86_64__) || defined(_M_X64)
        _mm_setcsr(csr);
#endif
    }
};

struct Hash {
    std::uint64_t x=0xcbf29ce484222325ull;
    void bits(std::uint64_t v,int n) {
        for (int i=0;i<n;++i) { x^=(v>>(8*i))&0xffu; x*=0x100000001b3ull; }
    }
    template<class T> void put(T v) {
        if constexpr (std::is_same_v<T,float>) bits(std::bit_cast<std::uint32_t>(v),4);
        else if constexpr (std::is_same_v<T,double>) bits(std::bit_cast<std::uint64_t>(v),8);
        else if constexpr (std::is_integral_v<T>) bits(std::uint64_t(v),sizeof(T));
    }
};

void hashSnapshot(Hash& h,const cs80::Engine& e) {
    for(int vi=0;vi<8;++vi) {
        const auto v=e.getVoiceInfo(vi);
        h.put(v.active); h.put(v.held); h.put(v.pedalHeld); h.put(v.note);
        h.put(v.assignmentSerial); h.put(v.touchA); h.put(v.touchB); h.put(v.touchC);
        for(int ch=0;ch<2;++ch) {
            const auto r=e.getRankInfo(vi,ch);
            h.put(r.hpfHz); h.put(r.lpfHz); h.put(r.filterEnvelope);
            h.put(r.ampEnvelope); h.put(r.oscillatorHz); h.put(r.keyVolts);
            h.put(r.hpfQ); h.put(r.lpfQ);
            const auto a=e.getRankAudioState(vi,ch);
            for(double d:{a.hpCircuit.input,a.hpCircuit.feedback,a.hpCircuit.bp,
                          a.hpCircuit.lp,a.lpCircuit.input,a.lpCircuit.feedback,
                          a.lpCircuit.bp,a.lpCircuit.lp}) h.put(d);
            const auto cv=e.getRankVcaState(vi,ch);
            h.put(cv.level); h.put(cv.coupling);
        }
    }
    const auto read=e.getSourceSharedReadout();
    for(int ch=0;ch<2;++ch) {
        h.put(read.levelBusVolts[ch]);
        for(int vi=0;vi<8;++vi) {
            h.put(read.levelGain[ch][vi]); h.put(read.levelPinVolts[ch][vi]);
            for(int side=0;side<2;++side) {
                h.put(read.q[ch][vi][side]); h.put(read.qVolts[ch][vi][side]);
            }
        }
    }
    for(int tap=0;tap<3;++tap) {
        const auto o=e.getOutputTapState(tap);
        h.put(o.state.capacitorVolts); h.put(o.state.previousSourceVolts);
        h.put(o.nodes.capacitorVolts); h.put(o.nodes.commonVolts);
        h.put(o.nodes.jackVolts); h.put(o.nodes.audibleJackVolts);
    }
}

struct Coverage {
    std::uint64_t outputBits=0,readoutSnapshots=0,qualified=0;
    std::uint64_t hpGroups=0,lpGroups=0, scalarFallbacks=0;
    std::uint64_t cases=0,failed=0;
};

void oneCase(int policy,int partition,bool eligibleFp,Coverage& cover) {
    cs80::ParamValues params;
    params[cs80::P_parityMode]=policy==0?1.0f:0.0f;
    params[cs80::P_sourcePerformance]=policy==1?1.0f:0.0f;
    params[cs80::P_drift]=0.0f;
    params[cs80::P_analogNoise]=0.0f;
    for(int ch=0;ch<2;++ch) {
        params[cs80::chParam(ch,cs80::CP_hpfOn)]=1.0f;
        params[cs80::chParam(ch,cs80::CP_lpfOn)]=1.0f;
    }
    cs80::Engine scalar,packed;
    scalar.prepare(48000.0); packed.prepare(48000.0);
    scalar.setParams(params); packed.setParams(params);
    std::array<float,512> original{},vector{};
    Hash originalHash,vectorHash,originalState,vectorState;
    const auto beforeQualified=cs80::phase_cpu_witness::qualifiedSamples;
    const auto beforeHp=cs80::phase_cpu_witness::hpVectorGroups;
    const auto beforeLp=cs80::phase_cpu_witness::lpVectorGroups;
    const auto beforeFallback=cs80::phase_cpu_witness::hpScalarFallbackGroups
                             +cs80::phase_cpu_witness::lpScalarFallbackGroups;
    constexpr int end=6144;
    constexpr std::array<int,4> events{17,4097,4609,5633};
    for(int frame=0;frame<end;) {
        if(frame==17 || frame==4609)
            for(int vi=0;vi<8;++vi) {
                scalar.noteOn(48+2*vi,0.8f); packed.noteOn(48+2*vi,0.8f);
            }
        if(frame==4097 || frame==5633)
            for(int vi=0;vi<8;++vi) {
                scalar.noteOff(48+2*vi); packed.noteOff(48+2*vi);
            }
        int next=end;
        for(int event:events) if(event>frame) {next=event;break;}
        const int checkpoint=((frame/256)+1)*256;
        const int n=std::min({partition,end-frame,next-frame,checkpoint-frame});
        std::fenv_t startFp{};
        std::fegetenv(&startFp);
        std::feclearexcept(FE_ALL_EXCEPT);
        cs80::phase_cpu_witness::rankRngCalls=0;
        cs80::phase_cpu_witness::rankRngOrderHash=0xcbf29ce484222325ull;
        cs80::phase_cpu_witness::forceScalar=true;
        scalar.render(original.data(),n);
        const int originalFpFlags=std::fetestexcept(FE_ALL_EXCEPT);
        const auto originalRngCalls=cs80::phase_cpu_witness::rankRngCalls;
        const auto originalRngHash=cs80::phase_cpu_witness::rankRngOrderHash;
        std::fesetenv(&startFp);
        std::feclearexcept(FE_ALL_EXCEPT);
        cs80::phase_cpu_witness::rankRngCalls=0;
        cs80::phase_cpu_witness::rankRngOrderHash=0xcbf29ce484222325ull;
        cs80::phase_cpu_witness::forceScalar=false;
        packed.render(vector.data(),n);
        const int vectorFpFlags=std::fetestexcept(FE_ALL_EXCEPT);
        if (originalFpFlags!=vectorFpFlags) {
            ++cover.failed;
            std::fprintf(stderr,"FAIL Engine IEEE sticky FP exceptions policy=%d FP=%d block=%d frame=%d flags=0x%x/0x%x\n",
                policy,eligibleFp,partition,frame,vectorFpFlags,originalFpFlags);
            return;
        }
        if (originalRngCalls!=cs80::phase_cpu_witness::rankRngCalls
            || originalRngHash!=cs80::phase_cpu_witness::rankRngOrderHash) {
            ++cover.failed;
            std::fprintf(stderr,"FAIL Engine rank RNG draw-count/value-order policy=%d FP=%d block=%d frame=%d calls=%llu/%llu hashes=0x%016llx/0x%016llx\n",
                policy,eligibleFp,partition,frame,
                (unsigned long long)cs80::phase_cpu_witness::rankRngCalls,
                (unsigned long long)originalRngCalls,
                (unsigned long long)cs80::phase_cpu_witness::rankRngOrderHash,
                (unsigned long long)originalRngHash);
            return;
        }
        for(int sample=0;sample<2*n;++sample) {
            originalHash.put(original[static_cast<std::size_t>(sample)]);
            vectorHash.put(vector[static_cast<std::size_t>(sample)]);
            ++cover.outputBits;
            if(std::bit_cast<std::uint32_t>(original[static_cast<std::size_t>(sample)])
                !=std::bit_cast<std::uint32_t>(vector[static_cast<std::size_t>(sample)])) {
                if(++cover.failed<=8)
                    std::fprintf(stderr,"FAIL Engine actual SIMD output policy=%d FP=%d block=%d frame=%d channel=%d bits=0x%08x/0x%08x\n",
                        policy,eligibleFp,partition,frame+sample/2,sample%2,
                        std::bit_cast<unsigned>(vector[static_cast<std::size_t>(sample)]),
                        std::bit_cast<unsigned>(original[static_cast<std::size_t>(sample)]));
            }
        }
        frame+=n;
        if(frame==checkpoint) {
            hashSnapshot(originalState,scalar);
            hashSnapshot(vectorState,packed);
            ++cover.readoutSnapshots;
        }
        if(cover.failed) return;
    }
    if(originalHash.x!=vectorHash.x || originalState.x!=vectorState.x) {
        ++cover.failed;
        std::fprintf(stderr,"FAIL Engine packed bit/state hash policy=%d FP=%d block=%d audio=0x%016llx/0x%016llx state=0x%016llx/0x%016llx\n",
            policy,eligibleFp,partition,(unsigned long long)vectorHash.x,(unsigned long long)originalHash.x,
            (unsigned long long)vectorState.x,(unsigned long long)originalState.x);
    }
    const auto qualified=cs80::phase_cpu_witness::qualifiedSamples-beforeQualified;
    const auto hp=cs80::phase_cpu_witness::hpVectorGroups-beforeHp;
    const auto lp=cs80::phase_cpu_witness::lpVectorGroups-beforeLp;
    const auto fallback=cs80::phase_cpu_witness::hpScalarFallbackGroups
        +cs80::phase_cpu_witness::lpScalarFallbackGroups-beforeFallback;
    cover.qualified+=qualified;
    cover.hpGroups+=hp; cover.lpGroups+=lp; cover.scalarFallbacks+=fallback;
    if(policy!=2 && eligibleFp) {
        if(qualified==0 || hp!=qualified*4 || lp!=qualified*4 || fallback!=0) {
            ++cover.failed;
            std::fprintf(stderr,"FAIL Engine real backend work policy=%d FP=%d group=%d: qualified=%llu hp=%llu lp=%llu scalarFallback=%llu\n",
                policy,eligibleFp,partition,(unsigned long long)qualified,
                (unsigned long long)hp,(unsigned long long)lp,(unsigned long long)fallback);
        }
    } else if(qualified!=0 || hp!=0 || lp!=0 || fallback!=0) {
        ++cover.failed;
        std::fprintf(stderr,"FAIL Engine forbidden foreign/Legacy SIMD policy=%d FP=%d block=%d\n",
            policy,eligibleFp,partition);
    }
    ++cover.cases;
}

int run() {
    FpRestore restore;
    Coverage cover;
    for(int supported=0;supported<3;++supported) {
        std::fesetround(supported==0?FE_DOWNWARD:FE_TONEAREST);
#if defined(__x86_64__) || defined(_M_X64)
        // Third mode: x87 reports nearest while SSE is explicitly DOWNWARD.
        // A backend checking only fegetround() is unsafe in this mode.
        _mm_setcsr((_mm_getcsr()&~0xe040u)
                   |(supported?0x8040u:0u)|(supported==2?0x2000u:0u));
#endif
        for(int policy=0;policy<3;++policy)
            for(int partition:{1,17,256}) {
                oneCase(policy,partition,supported==1,cover);
                if(cover.failed) return 1;
            }
    }
    const bool ok=cover.cases==27 && cover.qualified>5000 && cover.hpGroups>20000
               && cover.lpGroups>20000 && cover.readoutSnapshots==27*24
               && cover.scalarFallbacks==0 && cover.failed==0;
    std::printf("Engine true phase SIMD: cases=%llu output-exact-bits=%llu state-readout-checkpoints=%llu qualified-samples=%llu actual-vector-HP-groups=%llu actual-vector-LP-groups=%llu scalar-fallbacks=%llu failures=%llu result=%s\n",
        (unsigned long long)cover.cases,(unsigned long long)cover.outputBits,
        (unsigned long long)cover.readoutSnapshots,(unsigned long long)cover.qualified,
        (unsigned long long)cover.hpGroups,(unsigned long long)cover.lpGroups,
        (unsigned long long)cover.scalarFallbacks,(unsigned long long)cover.failed,
        ok?"PASS":"FAIL");
    return ok?0:1;
}
}

int main() { return run(); }

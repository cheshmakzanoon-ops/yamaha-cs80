// Source-tree-only qualification of idle IG00156 rank state and wake clocks.
// The expected original-host checksums are recorded with --record-original
// using the *pristine original* source tree and independently linked engine.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif
#if defined(__linux__)
#include <gnu/libc-version.h>
#endif

namespace {
struct SavedFp {
    std::fenv_t before{};
#if defined(__x86_64__) || defined(_M_X64)
    unsigned csr=_mm_getcsr();
#endif
    SavedFp() { std::fegetenv(&before); }
    ~SavedFp() {
        std::fesetenv(&before);
#if defined(__x86_64__) || defined(_M_X64)
        _mm_setcsr(csr);
#endif
    }
};
struct Hash {
    std::uint64_t x=0xcbf29ce484222325ull;
    void bytes(std::uint64_t b,int n) {
        for (int j=0;j<n;++j) {x^=(b>>(8*j))&0xffu;x*=0x100000001b3ull;}
    }
    template<class T> void put(T b) {
        if constexpr (std::is_same_v<T,float>) bytes(std::bit_cast<std::uint32_t>(b),4);
        else if constexpr (std::is_same_v<T,double>) bytes(std::bit_cast<std::uint64_t>(b),8);
        else if constexpr (std::is_integral_v<T>) bytes(std::uint64_t(b),sizeof(T));
    }
};

void snapshot(Hash& hash,const cs80::Engine& engine) {
    for (int vi=0;vi<8;++vi) {
        const auto v=engine.getVoiceInfo(vi);
        hash.put(v.active); hash.put(v.held); hash.put(v.pedalHeld);
        hash.put(v.note); hash.put(v.assignmentSerial);
        for (int ch=0;ch<2;++ch) {
            const auto a=engine.getRankAudioState(vi,ch);
            for (double d : {a.hpCircuit.input,a.hpCircuit.feedback,
                             a.hpCircuit.bp,a.hpCircuit.lp,
                             a.lpCircuit.input,a.lpCircuit.feedback,
                             a.lpCircuit.bp,a.lpCircuit.lp}) hash.put(d);
            const auto c=engine.getRankVcaState(vi,ch);
            hash.put(c.level); hash.put(c.coupling);
            const auto r=engine.getRankInfo(vi,ch);
            hash.put(r.hpfHz); hash.put(r.lpfHz);
            hash.put(r.filterEnvelope); hash.put(r.ampEnvelope);
            hash.put(r.hpfQ); hash.put(r.lpfQ);
        }
    }
}

struct Result {std::uint64_t audio,state;};
Result run(int policy,int block) {
    cs80::Engine engine;
    engine.prepare(48000.0);
    cs80::ParamValues p;
    p[cs80::P_parityMode]=policy==0?1.0f:0.0f;
    p[cs80::P_sourcePerformance]=policy==1?1.0f:0.0f;
    p[cs80::P_drift]=p[cs80::P_voiceDispersion]=p[cs80::P_analogNoise]=0.0f;
    p[cs80::P_keyRelease]=0.0f;
    for(int ch=0;ch<2;++ch) {
        p[cs80::chParam(ch,cs80::CP_hpfOn)]=1.0f;
        p[cs80::chParam(ch,cs80::CP_lpfOn)]=1.0f;
        p[cs80::chParam(ch,cs80::CP_vR)]=0.01f;
        p[cs80::chParam(ch,cs80::CP_fR)]=0.01f;
    }
    engine.setParams(p);
    std::array<float,512> buffer {};
    Hash audio,states;
    constexpr int duration=24576;
    constexpr std::array<int,4> events {17,8193,20481,22529};
    for(int frame=0;frame<duration;) {
        if(frame==17 || frame==20481)
            for(int vi=0;vi<8;++vi) engine.noteOn(48+vi*2,0.8f);
        if(frame==8193 || frame==22529)
            for(int vi=0;vi<8;++vi) engine.noteOff(48+vi*2);
        int next=duration;
        for (int event:events) if(event>frame) {next=event;break;}
        const int checkpoint=(frame/256+1)*256;
        const int n=std::min({block,duration-frame,next-frame,checkpoint-frame});
        engine.render(buffer.data(),n);
        for(int sample=0;sample<2*n;++sample)
            audio.put(buffer[static_cast<std::size_t>(sample)]);
        frame+=n;
        if(frame==checkpoint) snapshot(states,engine);
    }
    return {audio.x,states.x};
}

// Filled only from --record-original with source SHA
// 7dd159d491012fd47b46c0aa651e86ea959ca289, NOT from the SIMD branch.
constexpr std::array<Result,2> originalHost {{
    {0x8efa096f926a5789ull,0xd166a8745b95d4dbull}, // original PURE isolation
    {0x8efa096f926a5789ull,0xd166a8745b95d4dbull}  // original sourcePerformance
}};

bool knownHost() {
#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBC__) && defined(__GNUC__)
    return __GNUC__==13 && __GNUC_MINOR__==3
           && std::string_view(gnu_get_libc_version())=="2.39";
#else
    return false;
#endif
}
}

int main(int argc,char** argv) {
    const bool record=argc==2 && std::string_view(argv[1])=="--record-original";
    if(!record && argc!=1) return 2;
    SavedFp restore;
    std::fesetround(FE_TONEAREST);
#if defined(__x86_64__) || defined(_M_X64)
    _mm_setcsr(_mm_getcsr()|0x8040u); // same denormal mode as CPU benchmark
#endif
    int failures=0;
    for(int policy=0;policy<2;++policy) {
        const auto first=run(policy,1);
        const auto middle=run(policy,17);
        const auto last=run(policy,256);
        if(record)
            std::printf("ORIGINAL 7dd159d policy=%d: 1={0x%016llx,0x%016llx} 17={0x%016llx,0x%016llx} 256={0x%016llx,0x%016llx}\n",
                policy,(unsigned long long)first.audio,(unsigned long long)first.state,
                (unsigned long long)middle.audio,(unsigned long long)middle.state,
                (unsigned long long)last.audio,(unsigned long long)last.state);
        if(first.audio!=middle.audio || first.audio!=last.audio
           ||first.state!=middle.state || first.state!=last.state) {
            ++failures;
            std::fprintf(stderr,"FAIL idle-wake callback partition policy=%d\n",policy);
        }
        if(!record && knownHost() && (first.audio!=originalHost[policy].audio
                                     ||first.state!=originalHost[policy].state)) {
            ++failures;
            std::fprintf(stderr,"FAIL frozen ORIGINAL idle/wake rank-state digest policy=%d: audio=0x%016llx/0x%016llx state=0x%016llx/0x%016llx\n",
                policy,(unsigned long long)first.audio,(unsigned long long)originalHost[policy].audio,
                (unsigned long long)first.state,(unsigned long long)originalHost[policy].state);
        }
    }
    std::printf("Original idle/wake rank checkpoint: source=7dd159d clock=FTZ/DAZ 48k partitions=1/17/256 policies=2 checkpoints=576 failures=%d original-host=%s\n",
        failures,knownHost()?"yes":"foreign-skip");
    return failures==0?0:1;
}

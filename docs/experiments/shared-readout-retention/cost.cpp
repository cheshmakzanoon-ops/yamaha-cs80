// Focused shared-readout screen. No plugin host, public callback, or DAW meter.
#define main original_benchmark_main_not_run
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <sched.h>
#include <xmmintrin.h>

namespace {
std::uint64_t count_hits=0;
std::uint64_t ns() { timespec t{}; if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)) std::abort();
    return static_cast<std::uint64_t>(t.tv_sec)*1000000000ull+t.tv_nsec; }
}
extern "C" void shared_cache_observe(unsigned result) { if (result==2) ++count_hits; }

int main(int argc,char**argv) {
    if(argc!=3) return 2;
    const int index=std::atoi(argv[1]); bool timed=std::strcmp(argv[2],"time")==0;
    if(index<0||index>=4||(!timed&&std::strcmp(argv[2],"check")!=0)) return 2;
    const char* label[]={"factory-moving","panel-moving","factory-held","pure-held"};
    ScopedDenormals fp;
    constexpr int warm=1024, measured=512;
    Case c;c.rate=48000;c.block=64;c.selector=index==1?13:0;c.isolated=index==3;c.sourcePerformance=index!=3;
    const bool moving=index<2;c.performanceProfile=moving?1:0;
    auto frame=makeFrame(c,c.selector);frame.effectiveValues[P_drift]=.37f;
    frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_driftModel]=0.f;
    Engine eng;eng.prepare(c.rate);if(!eng.setControlFrame(frame)) return 4;eng.reset();
    for(int v=0;v<8;v++)eng.noteOn(notes[v],.8f,moving?v+2:1);
    const int cpu=sched_getcpu();std::array<float,128> pcm{};
    std::uint64_t ns_sum=0,empty_sum=0,hash=1469598103934665603ull;unsigned nonzero=0,voices=0;
    for(int b=0;b<warm+measured;b++) {
        const bool measure=b>=warm;
        if(b==warm) count_hits=0;
        const auto before=timed&&measure?ns():0;
        if(moving && b%4==0) {
            const int v=(b/4)%8; const float x=float((b*13)%97)/96.f;
            eng.channelPressure(v+2,x);eng.pitchBend(v+2,x*.1f);eng.controlChange(74,x,v+2);
        }
        if(b==128||b==384||b==1100) if(!eng.setControlFrame(frame)) return 4;
        eng.render(pcm.data(),64);
        if(!measure) continue;
        if(timed) {const auto after=ns();if(after<=before)return 5;ns_sum+=after-before;}
        for(float x:pcm){if(!std::isfinite(x))return 6;nonzero+=x!=0;hash^=std::bit_cast<std::uint32_t>(x);hash*=1099511628211ull;}
        voices+=static_cast<unsigned>(eng.activeVoices());
    }
    if(timed)for(int j=0;j<measured;j++){auto a=ns();auto b=ns();if(b<=a)return 7;empty_sum+=b-a;}
    const int end=sched_getcpu();if(cpu<0||cpu!=end||(!nonzero)||voices!=8*measured) return 8;
    if((timed&&(ns_sum<=empty_sum||!empty_sum))||(!timed&&(ns_sum||empty_sum))) return 9;
    std::printf("%s,%d,%d,%llu,%llu,%016llx,%u,%u,%x,%llu\n",label[index],cpu,end,
                (unsigned long long)ns_sum,(unsigned long long)empty_sum,
                (unsigned long long)hash,nonzero,voices,_mm_getcsr(),(unsigned long long)count_hits);
}

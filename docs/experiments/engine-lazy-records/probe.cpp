#define main archived_benchmark_main_not_run
#include "tools/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <valgrind/callgrind.h>
#include "observe_engine.h"
using namespace cs80;
namespace {
void fail(const char* s) { std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}
struct Emit { FILE* dest; void operator()(const char*,std::uint64_t u){if(std::fwrite(&u,sizeof u,1,dest)!=1) fail("write");} };
modern::ModernConfig route() {
    modern::ModernConfig m;
    using S=modern::ModSource;using D=modern::ModDestination;using T=modern::ModTarget;
    m.slots[0]={S::filterEnvelope,S::mseg1,D::pitch,T::both};
    m.slots[1]={S::velocity,S::ampEnvelope,D::pan,T::rankII};
    m.slots[2]={S::mseg2,S::constant,D::subDepth,T::both};
    m.msegs[1].trigger=modern::MsegTrigger::free;m.msegs[1].sync=true;
    return m;
}
}
int main(int argc,char**argv) {
    if(argc!=4)return 2;
    const int scene=std::atoi(argv[1]),window=std::atoi(argv[2]);if(scene<0||scene>8||window<0||window>2)return 2;
    auto* stream=std::fopen(argv[3],"wb");if(!stream)fail("open");Emit emit{stream};
    Case c;c.rate=48000;c.block=64;c.isolated=scene==6;c.sourcePerformance=scene!=6&&scene!=7;c.selector=scene==4?13:0;
    auto f=makeFrame(c,c.selector);auto& p=f.effectiveValues;
    p[P_drift]=.37f;p[P_voiceDispersion]=.18f;p[P_driftModel]=0;
    if(scene==4||scene==3||scene==8) {p[P_mod1Amount]=.82f;p[P_mod2Amount]=.67f;p[P_mod3Amount]=.87f;}
    ScopedDenormals normal;Engine e;e.prepare(c.rate);if(!e.setControlFrame(f))fail("frame");e.reset();
    const auto m=route();if(scene==4||scene==3)e.setModernConfig(m);
    if(e.isModernLayerActive()!=(scene==3||scene==4))fail("matrix admission");
    std::array<float,128> out{};
    const auto chord=[&](int n){for(int i=0;i<n;++i)e.noteOn(notes[i],scene==1?.82f:(.32f+i*.081f),1);};
    if(scene!=0)chord(scene==5?1:8);
    for(int b=-32;b<5;++b) {
        if(b==-12&&scene==8)e.setModernConfig(m);
        if(b==-8&&scene==8){for(auto n:notes)e.noteOff(n);e.setModernConfig({});}
        if(b==-2&&scene==8)chord(8);
        if(b==1&&scene==8)e.setModernConfig(m);
        if(b==3&&scene==8)e.setModernConfig({});
        if(scene==3||scene==4||scene==8){e.setTransport({100.0,2.+(b+32)*64./48000.*100./60.,true,true});e.channelPressure(1,float((b+32)%17)/16);}
        if(!e.setControlFrame(f))fail("callback frame");
        if(b==0) {CALLGRIND_START_INSTRUMENTATION;}
        const bool observe=b>=0;const bool sample=observe&&((window!=2&&b<4)||(window!=0&&b==4));
        if(sample)CALLGRIND_TOGGLE_COLLECT;
        e.render(out.data(),64);
        if(sample)CALLGRIND_TOGGLE_COLLECT;
        if(b==4){CALLGRIND_DUMP_STATS;CALLGRIND_STOP_INSTRUMENTATION;}
        if(observe){
            const auto csr=_mm_getcsr();unsigned short cw,sw;asm volatile("fnstcw %0; fnstsw %1":"=m"(cw),"=m"(sw)::"memory");
            emit("",csr);emit("",cw);emit("",sw);emit("",errno);
            for(float x:out){if(!std::isfinite(x)){fail("finite");}wsc_staging_audit::field(emit,"pcm",x);}
            wsc_staging_audit::engineOrderingState(emit,e);
        }
    }
    return std::fclose(stream)!=0;
}

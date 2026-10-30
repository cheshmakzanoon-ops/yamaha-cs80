// Independent policy/model audit: numerical parity assertions, not measured-hardware claims.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#if defined(_MSC_VER)
#include <malloc.h>
#endif
#include <vector>

namespace allocation_audit {
thread_local bool watching=false;
thread_local std::size_t calls=0;
void record() noexcept {if(watching)++calls;}
}
void* operator new(std::size_t n) {allocation_audit::record();if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete[](void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
void operator delete[](void*p,std::size_t) noexcept {std::free(p);}
void* operator new(std::size_t n,std::align_val_t alignment) {
    allocation_audit::record();const auto a=static_cast<std::size_t>(alignment);
    if(n==0)n=1;
    if(n>static_cast<std::size_t>(-1)-(a-1))throw std::bad_alloc();
#if defined(_MSC_VER)
    void*p=_aligned_malloc(n,a);
#else
    void*p=std::aligned_alloc(a,(n+a-1)/a*a);
#endif
    if(p) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t a) {return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
void operator delete[](void*p,std::align_val_t a) noexcept {::operator delete(p,a);}
void operator delete(void*p,std::size_t,std::align_val_t a) noexcept {::operator delete(p,a);}
void operator delete[](void*p,std::size_t,std::align_val_t a) noexcept {::operator delete(p,a);}

using namespace cs80;
namespace {
int checks=0, failures=0;
void check(bool ok,const char* what) { ++checks; if(!ok) {if(failures<40)std::fprintf(stderr,"FAIL: %s\n",what);++failures;} }
ControlFrame neutral(bool isolated) {
    ControlFrame f;
    auto& p=f.effectiveValues;
    p[P_parityMode]=isolated?1.f:0.f; p[P_sourcePerformance]=isolated?0.f:1.f;
    p[P_keyRelease]=0; p[P_subOriginalMode]=1; p[P_ringSpeed]=37;
    p[P_outputMode]=1; p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=0;
    p[P_mpeOn]=p[P_arpOn]=0; p[P_tuningRef]=440;
    p[P_modWheelSub]=p[P_modWheelVcf]=0;
    for(int i:{P_lfo1Depth,P_lfo2Depth,P_lfo3Depth,P_lfo1Dest,P_lfo2Dest,P_lfo3Dest,
               P_fx1Type,P_fx2Type,P_fx3Type,P_fx1Mix,P_fx2Mix,P_fx3Mix})p[i]=0;
    for(int i:{P_macro1,P_macro2,P_macro3,P_macro4})p[i]=.5f;
    p[P_brilliance]=.23f; p[P_resonance]=.37f; p[P_masterSustain]=0;
    p[P_subVco]=.14f; p[P_subVcf]=.21f; p[P_subVca]=.12f;
    p[P_subAtSpeed]=.25f; p[P_subAtVco]=.18f; p[P_subAtVcf]=.17f;
    p[P_ringAmount]=.16f; p[P_ringDepth]=.22f; p[P_footMode]=1;
    for(int rank=0;rank<2;++rank) {p[chParam(rank,CP_hpfOn)]=p[chParam(rank,CP_lpfOn)]=1;p[chParam(rank,CP_pan)]=0;}
    return f;
}
std::vector<float> render(Engine& e,int frames,int block=0,const ControlFrame* repeated=nullptr) {
    std::vector<float> result(static_cast<size_t>(frames)*2);
    if(block<=0)block=std::max(1,frames);
    for(int at=0;at<frames;at+=block){if(repeated)check(e.setControlFrame(*repeated),"repeated frame accepted");e.render(result.data()+at*2,std::min(block,frames-at));}
    return result;
}
void init(Engine& e,double rate,const ControlFrame& f) {e.prepare(rate);check(e.setControlFrame(f),"initial frame accepted");}
bool same(const Engine::RankAudioState&a,const Engine::RankAudioState&b) {
    return a.phase==b.phase&&a.hpf.ic1==b.hpf.ic1&&a.hpf.ic2==b.hpf.ic2&&a.lpf.ic1==b.lpf.ic1&&a.lpf.ic2==b.lpf.ic2;
}
bool same(const Engine::RankEnvelopeState&a,const Engine::RankEnvelopeState&b) {
    return a.filter.stage==b.filter.stage&&a.filter.value==b.filter.value&&a.filter.elapsed==b.filter.elapsed
        &&a.amplitude.stage==b.amplitude.stage&&a.amplitude.value==b.amplitude.value&&a.amplitude.elapsed==b.amplitude.elapsed;
}
void compareModels() {
    for(double rate:{44100.,48000.,96000.})for(int rank=0;rank<2;++rank)for(int selector=0;selector<14;++selector){
        auto isolated=neutral(true),performance=neutral(false);
        isolated.hardwareSources[rank]=performance.hardwareSources[rank]=selector;
        Engine a,b;init(a,rate,isolated);init(b,rate,performance);
        check(a.getRenderPolicy()==RenderPolicy::isolation&&b.getRenderPolicy()==RenderPolicy::sourcePerformance,"distinct explicit policies");
        check(a.getSourceToneInfo(rank).route==b.getSourceToneInfo(rank).route,"neutral policy split retains same physical route");
        for(int note:{36,61,87}){a.noteOn(note,.73f);b.noteOn(note,.73f);a.polyPressure(note,.31f);b.polyPressure(note,.31f);}
        a.setExpression(.62f);b.setExpression(.62f);a.setRibbon(-.08f);b.setRibbon(-.08f);
        check(render(a,127)==render(b,127,17,&performance),"neutral source performance equals isolation exactly, repeated blocks");
        for(int voice=0;voice<3;++voice)for(int ch=0;ch<2;++ch){
            check(same(a.getRankAudioState(voice,ch),b.getRankAudioState(voice,ch)),"neutral physical audio states exact");
            check(same(a.getRankEnvelopeState(voice,ch),b.getRankEnvelopeState(voice,ch)),"neutral precise envelopes exact");
        }
        a.noteOff(36);b.noteOff(36);check(render(a,43)==render(b,43,7),"neutral release remains bit-exact");
        auto nextI=isolated,nextP=performance;
        nextI.hardwareSources[rank]=nextP.hardwareSources[rank]=(selector+1)%14;
        nextI.effectiveValues[chParam(rank,CP_fA)]=nextP.effectiveValues[chParam(rank,CP_fA)]=.12f;
        nextI.effectiveValues[chParam(rank,CP_vS)]=nextP.effectiveValues[chParam(rank,CP_vS)]=.21f;
        check(a.setControlFrame(nextI)&&b.setControlFrame(nextP),"live source/port updates accepted");
        check(render(a,251)==render(b,251,13),"neutral live targets and timing match physical isolation");
        a.allNotesOff(false);b.allNotesOff(false);check(render(a,73)==render(b,73,11),"neutral panic/dormant control and output tails match");
        b.prepare(rate*2);check(b.getRenderPolicy()==RenderPolicy::sourcePerformance&&b.getSourceToneInfo(rank).selector==nextP.hardwareSources[rank],"prepare retains source-performance identity");
    }
}
void priorityAndTransitions() {
    for(int from=0;from<3;++from)for(int to=0;to<3;++to){
        auto f=neutral(false);f.hardwareSources={1,3};
        f.effectiveValues[P_parityMode]=from==2?1.f:0.f;f.effectiveValues[P_sourcePerformance]=from==1?1.f:0.f;
        Engine e;init(e,48000,f);e.noteOn(60,.8f);render(e,83);
        const auto audio=e.getRankAudioState(0,0); const auto env=e.getRankEnvelopeState(0,0);
        auto next=f;next.effectiveValues[P_parityMode]=to==2?1.f:0.f;next.effectiveValues[P_sourcePerformance]=to==1?1.f:0.f;
        check(e.setControlFrame(next),"policy transition accepted");
        if(from==to)check(e.activeVoices()==1&&same(audio,e.getRankAudioState(0,0))&&same(env,e.getRankEnvelopeState(0,0)),"unchanged policy retains voice and exact state");
        else {check(e.activeVoices()==0,"effective policy transition retires old voices");for(int v=0;v<8;++v)for(int r=0;r<2;++r)check(same(e.getRankAudioState(v,r),{}),"effective policy transition clears oscillator/filter state");}
    }
    auto isolated=neutral(true);isolated.hardwareSources={1,3};
    auto retained=isolated;retained.effectiveValues[P_sourcePerformance]=1;
    Engine a,b;init(a,48000,isolated);init(b,48000,isolated);
    for(auto*e:{&a,&b}){e->noteOn(60,.8f);e->setExpression(.41f);render(*e,79);}
    const auto count=b.getSourceCompileCount();check(b.setControlFrame(retained),"retained source flag accepted in isolation");
    check(b.getRenderPolicy()==RenderPolicy::isolation&&b.isParityMode()&&b.activeVoices()==1,"isolation dominates without resetting retained flag change");
    check(b.getSourceCompileCount()==count,"retained flag change does not recompile sources");
    check(render(a,511)==render(b,511,13,&retained),"retained source flag is bit-inert under isolation");
    retained.effectiveValues[P_parityMode]=0;check(b.setControlFrame(retained),"exit isolation accepted");
    check(b.getRenderPolicy()==RenderPolicy::sourcePerformance&&b.activeVoices()==0,"exit isolation activates retained source policy with reset");
}
void overlayOnceAndOwnership() {
    auto f=neutral(false);f.hardwareSources={1,3};auto&p=f.effectiveValues;
    p[P_subVcf]=p[P_subAtVcf]=p[P_subVca]=0;p[P_brilliance]=.25f;p[P_macro1]=.75f;p[P_modWheelVcf]=.3f;
    p[P_kbdBrillianceLow]=.2f;p[P_kbdBrillianceHigh]=.3f;
    Engine a,b;init(a,48000,f);init(b,48000,f);
    a.noteOn(48,.71f);b.noteOn(48,.71f);a.polyPressure(48,.32f);b.polyPressure(48,.32f);
    constexpr float timbre=.35f,wheel=.4f;
    a.controlChange(74,timbre);b.controlChange(74,timbre);a.controlChange(1,wheel);b.controlChange(1,wheel);
    auto altered=f;for(int rank=0;rank<2;++rank)for(int local=0;local<CP_count;++local){
        if(local==CP_feet||local==CP_pan||local==CP_hpfOn||local==CP_lpfOn)continue;
        const auto&d=kChannelDefs[local];altered.effectiveValues[chParam(rank,local)]=d.def==d.min?d.max:d.min;
    }
    check(b.setControlFrame(altered),"stored programmer perturbation accepted");
    check(render(a,197)==render(b,197),"all26 local controls remain source-owned with modern controllers active");
    for(int rank=0;rank<2;++rank){
        const auto trace=a.getSourceCutoffReadout(rank);const auto info=a.getRankInfo(0,rank);
        const float macro=(p[P_macro1]-.5f)*2.f;
        const float brilliance=clampf(p[P_brilliance]+macro*.75f,-3.f,3.f)
            +timbre*.8f+wheel*p[P_modWheelVcf]*1.5f
            +keyboardBrillianceOctaves(p[P_kbdBrillianceLow],p[P_kbdBrillianceHigh],keyboardContourWeights(48));
        check(info.hpfHz==nominalFilterCutoff(info.keyVolts,static_cast<float>(trace.effectiveCv[0][0])+brilliance),"HP adds modern brilliance/timbre/wheel/keyboard exactly once after source CV");
        check(info.lpfHz==nominalFilterCutoff(info.keyVolts,static_cast<float>(trace.effectiveCv[0][1])+brilliance),"LP adds modern brilliance/timbre/wheel/keyboard exactly once after source CV");
    }
    auto bypass=f;bypass.effectiveValues[chParam(0,CP_hpfOn)]=bypass.effectiveValues[chParam(0,CP_lpfOn)]=0;
    bypass.effectiveValues[chParam(0,CP_pan)]=1;
    Engine loaded,passed;init(loaded,48000,f);init(passed,48000,bypass);
    for(auto*e:{&loaded,&passed}){e->noteOn(60,.8f);render(*e,31);}
    const auto x=loaded.getSourceCutoffReadout(0),y=passed.getSourceCutoffReadout(0);
    check(x.pinVolts==y.pinVolts&&x.effectiveCv==y.effectiveCv&&x.loadedBusVolts==y.loadedBusVolts,"audio bypass/pan overlays never remove physical source receiver loads");
    check(!passed.getRankInfo(0,0).hpfEnabled&&!passed.getRankInfo(0,0).lpfEnabled,"source-performance audio bypass remains explicit overlay");
}
void realTimeAllocationBoundary() {
    Engine e;auto f=neutral(false);e.prepare(48000);
    std::array<float,128> block{};bool accepted=true,finite=true;
    allocation_audit::calls=0;allocation_audit::watching=true;
    for(int pass=0;pass<2;++pass)for(int policy=0;policy<3;++policy)for(int selector=0;selector<14;++selector){
        f.effectiveValues[P_parityMode]=policy==2?1.f:0.f;
        f.effectiveValues[P_sourcePerformance]=policy==1?1.f:0.f;
        f.hardwareSources={selector,(selector+3)%14};
        accepted=e.setControlFrame(f)&&accepted;
        e.noteOn(60,.8f,2);e.controlChange(64,1,2);e.polyPressure(60,.4f,2);
        e.pitchBend(2,.1f);e.render(block.data(),64);e.noteOff(60,2);e.controlChange(64,0,2);
        for(float value:block)finite=std::isfinite(value)&&finite;
        e.allNotesOff(false);e.render(block.data(),64);
    }
    allocation_audit::watching=false;
    check(accepted&&finite,"RT allocation fixture covers accepted source/policy and finite render");
    check(allocation_audit::calls==0,"source/policy compile/reset, controllers and render issue no C++ new/new[] calls after prepare");
}
}
int main(){compareModels();priorityAndTransitions();overlayOnceAndOwnership();realTimeAllocationBoundary();std::printf("Independent source-performance audit: %d checks, %d failures\n",checks,failures);return failures?1:0;}

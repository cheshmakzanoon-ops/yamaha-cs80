// Initial/released IL selection and assignment continuity.
// Existing limits registered in completion-dsp.json at 85ccd9a, before evaluation.
#include "core/Engine.h"
#include "Ws1Tolerances.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks=0, failures=0;
void check(bool pass, const char* label) { ++checks; if(!pass) {++failures;std::cerr<<"FAIL: "<<label<<'\n';} }
void render(Engine& engine, int frames)
{
    std::array<float,512> buffer {};
    while (frames>0) {const int n=std::min(frames,256);engine.render(buffer.data(),n);frames-=n;}
}
ControlFrame fixture(bool pure, int selector, float quality)
{
    ParamValues p;
    p[P_sourcePerformance]=1; p[P_parityMode]=pure?1.0f:0.0f;
    p[P_oversampling]=quality; p[P_chorusOn]=p[P_drift]=p[P_voiceDispersion]=0;
    for(int rank : {0,1}) {
        p[chParam(rank,CP_fIl)]=rank==0?.2f:.6f;
        p[chParam(rank,CP_fAl)]=.5f;
        p[chParam(rank,CP_fA)]=.08f; p[chParam(rank,CP_fD)]=.3f; p[chParam(rank,CP_fR)]=.01f;
        p[chParam(rank,CP_vA)]=.01f; p[chParam(rank,CP_vR)]=.2f;
    }
    return selector<0?ControlFrame{p}:ControlFrame::programmer(p,{selector,selector});
}
std::array<float,2> targets(const ControlFrame& frame)
{
    std::array<float,2> values {};
    for(int rank : {0,1}) {
        if(frame.hasProgrammerPositions[rank]) {
            const auto buses=memory_panels::compileBuses(rank,frame.hardwareSources[rank],frame.programmerPositions[rank]);
            // The declared source receiver map is TP1=-0.5*V_IL volts and
            // z=3/5*TP1, independently of FilterEnv's stored capacitor state.
            values[rank]=static_cast<float>(-.3*buses.sources[10].volts);
        } else values[rank]=-3.0f*frame.effectiveValues[chParam(rank,CP_fIl)];
    }
    return values;
}
double releaseTime(const ControlFrame& frame)
{
    double result=0;
    for(int rank : {0,1}) {
        double time=frame.effectiveValues[chParam(rank,CP_fR)];
        if(frame.hasProgrammerPositions[rank]) {
            const auto buses=memory_panels::compileBuses(rank,frame.hardwareSources[rank],frame.programmerPositions[rank]);
            time=10*std::pow(10.,-3*std::clamp(buses.sources[14].volts/10.,0.,1.));
        }
        result=std::max(result,time);
    }
    // The source-performance policy keeps the existing KEY RELEASE and
    // MASTER SUSTAIN overlays; Pure masks these software timing additions.
    // Twenty release times means the effective selected time, not only the
    // rank's R control (KEY RELEASE defaults to 0.18 seconds).
    if (frame.effectiveValues[P_parityMode] < .5f)
        result=std::max({result,double(frame.effectiveValues[P_keyRelease]),
                       10.0*double(frame.effectiveValues[P_masterSustain])});
    return result;
}
using Snapshot=std::array<std::array<float,2>,8>;
Snapshot snapshot(const Engine& engine)
{
    Snapshot values {};
    for(int voice=0;voice<8;++voice) for(int rank : {0,1})
        values[voice][rank]=engine.getRankEnvelopeState(voice,rank).filter.value;
    return values;
}
double delta(const Snapshot& a,const Snapshot& b)
{
    double worst=0;
    for(int voice=0;voice<8;++voice) for(int rank : {0,1}) worst=std::max(worst,std::abs(double(a[voice][rank])-b[voice][rank]));
    return worst;
}
void run(const ws1::Tolerances& tol)
{
    double worstSteady=0,worstAssignment=0;
    for(double rate : {44100.,48000.,96000.}) for(bool pure : {false,true})
        for(float quality : {0.f,1.f,2.f}) for(int selector : {-1,11,12,13}) {
            auto frame=fixture(pure,selector,quality);
            Engine engine; engine.prepare(rate); check(engine.setControlFrame(frame),"cold frame accepted"); engine.reset();
            // Reset defines the initial charge. A simultaneous first strike
            // must preserve it even before a single sample has elapsed.
            auto before=snapshot(engine); engine.noteOn(60,.8f);
            worstAssignment=std::max(worstAssignment,delta(before,snapshot(engine)));
            engine.reset();
            render(engine,static_cast<int>(std::ceil(rate*20*releaseTime(frame)))+64);
            const auto expected=targets(frame);
            before=snapshot(engine);
            for(int voice=0;voice<8;++voice) for(int rank : {0,1}) {
                worstSteady=std::max(worstSteady,std::abs(double(before[voice][rank])-expected[rank]));
                check(engine.getRankEnvelopeState(voice,rank).filter.stage==FilterEnv::Stage::Idle,"untriggered IL tracking does not activate a voice");
            }
            // A control target is distinct from charge, including a zero-frame
            // IL retarget followed immediately by first assignment.
            frame.effectiveValues[chParam(0,CP_fIl)]=.75f;
            frame.effectiveValues[chParam(1,CP_fIl)]=.3f;
            if(selector>=0) frame=ControlFrame::programmer(frame.effectiveValues,{selector,selector});
            check(engine.setControlFrame(frame),"retarget frame accepted");
            check(delta(before,snapshot(engine))<=tol["assignment_filter_eg_jump_max_abs"],"new IL target cannot overwrite charge without elapsed time");
            for(int note=48;note<56;++note) engine.noteOn(note,.8f);
            worstAssignment=std::max(worstAssignment,delta(before,snapshot(engine)));
            render(engine,static_cast<int>(rate*.02));
            for(int note=48;note<56;++note) engine.noteOff(note);
            render(engine,std::max(1,static_cast<int>(rate*.001)));
            before=snapshot(engine); engine.noteOn(75,.8f);
            worstAssignment=std::max(worstAssignment,delta(before,snapshot(engine)));
            // SUSTAIN II retires old audio; the separately connected filter
            // capacitor continues its released selection, without reset.
            engine.reset(); frame.effectiveValues[P_sustainMode]=1;
            check(engine.setControlFrame(frame),"Sustain II frame accepted");
            for(int note=48;note<56;++note) engine.noteOn(note,.8f);
            render(engine,static_cast<int>(rate*.02));
            for(int note=48;note<56;++note) engine.noteOff(note);
            render(engine,std::max(1,static_cast<int>(rate*.001)));
            check(engine.getVoiceInfo(0).active && !engine.getVoiceInfo(0).held
                      && engine.getRankEnvelopeState(0,0).filter.stage==FilterEnv::Stage::Release,
                  "Sustain II fixture holds a released tail");
            before=snapshot(engine); engine.noteOn(75,.8f);
            worstAssignment=std::max(worstAssignment,delta(before,snapshot(engine)));
        }
    check(worstSteady<=tol["cold_filter_eg_state_max_abs"],"released untriggered filter EG settles to IL within original limit");
    check(worstAssignment<=tol["assignment_filter_eg_jump_max_abs"],"first/reused/Sustain-II assignments preserve every filter capacitor exactly");
    std::printf("cold IL worst settled error %.12g; assignment capacitor jump %.12g\n",worstSteady,worstAssignment);
    auto legacy=fixture(false,-1,1); legacy.effectiveValues[P_sourcePerformance]=0;
    Engine engine;engine.prepare(48000);check(engine.setControlFrame(legacy),"Legacy frame accepted");engine.reset();
    render(engine,48000);
    for(int rank : {0,1}) check(engine.getRankEnvelopeState(0,rank).filter.value==0,"Legacy untriggered Idle remains zero");
    engine.noteOn(60,.8f);
    for(int rank : {0,1}) check(engine.getRankEnvelopeState(0,rank).filter.value==targets(legacy)[rank],"Legacy first assignment retains historical IL reset");
}
}
int main(int argc,char** argv)
{
    try {const ws1::Tolerances tol(argc>1?argv[1]:"tests/parity/tolerances-v2/completion-dsp.json","click_continuity");run(tol);}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
    std::printf("Cold filter envelope: %d checks, %d failures\n",checks,failures);return failures?1:0;
}

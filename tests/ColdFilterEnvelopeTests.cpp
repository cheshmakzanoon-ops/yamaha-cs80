// Initial/released IL selection and assignment continuity.
// Existing limits registered in completion-dsp.json at 85ccd9a, before evaluation.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
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
double filterMemoryDelta(Ig00156Stage::State a, Ig00156Stage::State b)
{
    return std::max({std::abs(a.input-b.input),std::abs(a.feedback-b.feedback),
                     std::abs(a.bp-b.bp),std::abs(a.lp-b.lp)});
}
void originalBassPhrase(const ws1::Tolerances& tol)
{
    ParamValues program;
    bool found=false;
    for(const auto& preset:factoryPresets()) if(preset.name=="Bass Stack") {program=preset.values;found=true;}
    check(found,"original Bass Stack fixture exists");
    struct Event {int sample;bool on;int note;};
    constexpr double rate=44100.;
    constexpr int block=64; // engine_audit phrase event quantization
    std::vector<Event> events;
    const auto add=[&](double t,bool on,int note) {events.push_back({static_cast<int>(std::ceil(t*rate/block))*block,on,note});};
    add(.2,true,60);add(1.4,false,60);
    for(int note:{48,52,55,60}) add(3.,true,note);
    for(int note:{48,52,55,60}) add(4.4,false,note);
    add(6.,true,60);add(6.2,false,60);add(6.25,true,60);add(6.55,false,60);
    for(int note:{36,43,48,52,55,60,64,67}) add(7.3,true,note);
    for(int note:{36,43,48,52,55,60,64,67}) add(8.5,false,note);
    for(int note:{48,52,55,60}) add(8.65,true,note);
    int coldAssignments=0,warmAssignments=0;
    double worstWarm=0;
    for(bool pure:{false,true}) {
        auto values=program;values[P_sourcePerformance]=1;values[P_parityMode]=pure?1.f:0.f;
        // Deliberately retain the original physical-unit PANEL coordinate.
        // This test cannot benefit from the new electrical programmer mapping.
        const ControlFrame frame {values};
        Engine engine;engine.prepare(rate);check(engine.setControlFrame(frame),"original Bass physical frame accepted");engine.reset();
        size_t next=0;
        for(int sample=0;sample<static_cast<int>(9*rate);sample+=block) {
            while(next<events.size()&&events[next].sample<=sample) {
                const auto event=events[next++];
                if(!event.on) {engine.noteOff(event.note);continue;}
                const auto before=snapshot(engine);
                std::array<Engine::VoiceInfo,8> voices;
                std::array<std::array<Engine::RankAudioState,2>,8> audio;
                std::array<std::array<Engine::RankEnvelopeState,2>,8> envelopes;
                std::array<std::array<Ig00151Vca::State,2>,8> vca;
                for(int voice=0;voice<8;++voice) {
                    voices[voice]=engine.getVoiceInfo(voice);
                    for(int rank:{0,1}) {
                        audio[voice][rank]=engine.getRankAudioState(voice,rank);
                        envelopes[voice][rank]=engine.getRankEnvelopeState(voice,rank);
                        vca[voice][rank]=engine.getRankVcaState(voice,rank);
                    }
                }
                engine.noteOn(event.note,.8f);
                check(delta(before,snapshot(engine))<=tol["assignment_filter_eg_jump_max_abs"],
                      "original physical Bass phrase retains filter charge at every note-on");
                for(int voice=0;voice<8;++voice) {
                    const auto after=engine.getVoiceInfo(voice);
                    if(voices[voice].held||!after.held||after.note!=event.note) continue;
                    if(event.sample==static_cast<int>(std::ceil(6.25*rate/block))*block)
                        std::printf("original physical Bass retrigger: policy=%s voice=%d active_before=%d filter_before=%.12g filter_after=%.12g\n",
                            pure?"pure":"source",voice,voices[voice].active,
                            envelopes[voice][0].filter.value,engine.getRankEnvelopeState(voice,0).filter.value);
                    if(!voices[voice].active) {++coldAssignments;continue;}
                    ++warmAssignments;
                    for(int rank:{0,1}) {
                        const auto a=engine.getRankAudioState(voice,rank);
                        const auto e=engine.getRankEnvelopeState(voice,rank);
                        const auto c=engine.getRankVcaState(voice,rank);
                        const double jump=std::max({filterMemoryDelta(a.hpCircuit,audio[voice][rank].hpCircuit),
                            filterMemoryDelta(a.lpCircuit,audio[voice][rank].lpCircuit),
                            double(std::abs(a.phase-audio[voice][rank].phase)),
                            double(std::abs(e.filter.value-envelopes[voice][rank].filter.value)),
                            double(std::abs(e.amplitude.value-envelopes[voice][rank].amplitude.value)),
                            double(std::abs(c.level-vca[voice][rank].level)),std::abs(c.coupling-vca[voice][rank].coupling)});
                        worstWarm=std::max(worstWarm,jump);
                    }
                }
            }
            check(engine.setControlFrame(frame),"original phrase frame reapplied");
            render(engine,std::min(block,static_cast<int>(9*rate)-sample));
        }
    }
    check(coldAssignments>0&&warmAssignments>0,"original phrase exercises both cold and warm circuit assignments");
    check(worstWarm<=tol["assignment_filter_eg_jump_max_abs"],
          "original Bass warm assignments retain oscillator, both EGs, VCA and filter memories exactly");
    std::printf("original physical Bass phrase: %d cold, %d warm assignments; maximum warm capacitor/phase jump %.12g\n",
                coldAssignments,warmAssignments,worstWarm);
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
    originalBassPhrase(tol);
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

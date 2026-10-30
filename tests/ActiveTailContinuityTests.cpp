// Source-informed assignment policy only; no hardware waveform assertion.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* what) { ++checks; if (!ok) { ++failures; std::printf("FAIL: %s\n", what); } }
bool equal(Svf::State a, Svf::State b) { return a.ic1 == b.ic1 && a.ic2 == b.ic2; }
bool zero(Svf::State a) { return a.ic1 == 0 && a.ic2 == 0; }
bool equal(Engine::RankAudioState a, Engine::RankAudioState b) { return a.phase == b.phase && equal(a.hpf,b.hpf) && equal(a.lpf,b.lpf); }
ParamValues settings(bool isolation = true) {
    ParamValues p; p[P_parityMode] = isolation ? 1.0f : 0.0f;
    p[P_sustainMode] = 0; p[P_keyRelease] = 0;
    for (int r=0;r<2;++r) { p[chParam(r,CP_vR)] = 0.8f; p[chParam(r,CP_fR)] = 0.8f; p[chParam(r,CP_saw)] = 1; }
    return p;
}
void init(Engine& e, double rate, const ParamValues& p) { e.setParams(p); e.prepare(rate); }
std::vector<float> render(Engine& e, int frames, int block=0) {
    std::vector<float> out(static_cast<size_t>(frames)*2);
    if (block<=0) block=frames;
    for (int i=0;i<frames;i+=block) e.render(out.data()+i*2,std::min(block,frames-i));
    return out;
}
void fill(Engine& e) { for (int i=0;i<8;++i) e.noteOn(48+i,0.85f); }
void resetState(Engine& e, int v) { for(int r=0;r<2;++r) { auto s=e.getRankAudioState(v,r); check(zero(s.hpf)&&zero(s.lpf),"assignment clears HP and LP in excluded cases"); } }
}
int main() {
    for(double rate : {44100.0,48000.0,96000.0,192000.0}) {
        for(bool isolation : {false,true}) for(int replacement : {48,100}) {
            Engine e; init(e,rate,settings(isolation)); fill(e); render(e,997); e.noteOff(48); render(e,31);
            check(e.getVoiceInfo(0).active && !e.getVoiceInfo(0).held,"active unheld tail fixture");
            std::array<Engine::RankAudioState,2> before;
            std::array<Adsr::State,2> envBefore;
            for(int r=0;r<2;++r) { before[r]=e.getRankAudioState(0,r); check(!zero(before[r].hpf)&&!zero(before[r].lpf),"both filter memories nonzero before reuse"); }
            for(int r=0;r<2;++r) { envBefore[r]=e.getRankEnvelopeState(0,r).amplitude; check(envBefore[r].stage==Adsr::Stage::Release&&envBefore[r].value>0.01f,"amplitude envelope is a sounding release tail before reuse"); }
            e.noteOn(replacement,0.6f);
            for(int r=0;r<2;++r) {
                // The VCA envelope of a reused tail restarts its attack from the level it has reached
                // (source policies, inferred); legacy zeroes it, which steps the whole tail out in one sample.
                auto env=e.getRankEnvelopeState(0,r).amplitude;
                check(env.stage==Adsr::Stage::Attack,"reused circuit attacks");
                check(isolation ? env.value==envBefore[r].value : env.value==0.0f,"amplitude envelope policy at assignment");
            }
            for(int r=0;r<2;++r) {
                auto after=e.getRankAudioState(0,r);
                check(isolation ? after.phase==before[r].phase : after.phase!=before[r].phase,"phase policy at assignment");
                check(isolation ? equal(after.hpf,before[r].hpf) : zero(after.hpf),"HP memory policy at assignment");
                check(isolation ? equal(after.lpf,before[r].lpf) : zero(after.lpf),"LP memory policy at assignment");
            }
            if(isolation) {
                render(e,64,16);
                for(int r=0;r<2;++r) check(e.getRankEnvelopeState(0,r).amplitude.value>=envBefore[r].value,"reused-tail attack never dips below the tail level");
            }
            for(float x:render(e,1024)) check(std::isfinite(x),"reuse render finite");
        }
        // Cold, fully inactive, sustain-II-cleared, and panic-cleared circuits
        // must retain legacy assignment behavior rather than preserve frozen state.
        for(int scenario=0;scenario<4;++scenario) {
            Engine e; auto p=settings();
            if(scenario==1) for(int r=0;r<2;++r) p[chParam(r,CP_vR)]=0;
            if(scenario==2) p[P_sustainMode]=1;
            init(e,rate,p);
            if(scenario) {
                fill(e); render(e,997); e.noteOff(48);
                if(scenario==1) { render(e,static_cast<int>(rate*2)); check(!e.getVoiceInfo(0).active,"fully inactive fixture"); }
                if(scenario==3) e.controlChange(120,0,1);
            }
            e.noteOn(100,0.7f); resetState(e,0);
        }
        // A ninth held note must not mutate any circuit at event time.
        Engine held; init(held,rate,settings()); fill(held); render(held,997);
        std::array<std::array<Engine::RankAudioState,2>,8> snap;
        for(int v=0;v<8;++v) for(int r=0;r<2;++r) snap[v][r]=held.getRankAudioState(v,r);
        held.noteOn(100,1);
        for(int v=0;v<8;++v) { check(held.getVoiceInfo(v).note==48+v,"ninth held note ignored"); for(int r=0;r<2;++r) check(equal(snap[v][r],held.getRankAudioState(v,r)),"held state protected"); }
        held.reset();
        for(int v=0;v<8;++v) for(int r=0;r<2;++r) { auto s=held.getRankAudioState(v,r); check(s.phase==0&&zero(s.hpf)&&zero(s.lpf),"explicit reset clears all audio state"); }
        // MIDI pedal hold is a legacy extension; isolation ignores CC64.
        Engine pedal; init(pedal,rate,settings(false)); fill(pedal); render(pedal,997); pedal.controlChange(64,1,1); pedal.noteOff(48);
        check(pedal.getVoiceInfo(0).pedalHeld,"pedal hold fixture"); auto ps=pedal.getRankAudioState(0,0); pedal.noteOn(100,1);
        check(pedal.getVoiceInfo(0).note==48&&equal(ps,pedal.getRankAudioState(0,0)),"pedal-held circuit protected");
        // High resonance + pitch jumps, with identical timed events independent
        // of render block partition. No reset at the internal block boundary.
        auto p=settings(); p[P_resonance]=1;
        for(int r=0;r<2;++r) { p[chParam(r,CP_hpfRes)]=1; p[chParam(r,CP_lpfRes)]=1; }
        Engine a,b; init(a,rate,p); init(b,rate,p); fill(a); fill(b);
        check(render(a,997)==render(b,997,17),"initial block partition"); a.noteOff(48); b.noteOff(48);
        check(render(a,31)==render(b,31,7),"tail block partition"); a.noteOn(120,1); b.noteOn(120,1);
        auto x=render(a,2048),y=render(b,2048,19); check(x==y,"reused-tail block partition");
        for(float value:x) check(std::isfinite(value),"high Q pitch jump finite");
        p[P_parityMode]=0; a.setParams(p);
        check(a.activeVoices()==0,"mode transition clears voices");
        for(int v=0;v<8;++v) for(int r=0;r<2;++r) { auto s=a.getRankAudioState(v,r); check(s.phase==0&&zero(s.hpf)&&zero(s.lpf),"mode transition resets state"); }
    }
    // Same number/order of draws even when isolation skips the assignment reset.
    Engine legacy, isolated; init(legacy,48000,settings(false)); init(isolated,48000,settings());
    fill(legacy); fill(isolated); render(legacy,997); render(isolated,997);
    legacy.noteOff(48); isolated.noteOff(48); legacy.noteOn(100,1); isolated.noteOn(100,1);
    // Panic makes the next cyclic circuit inactive, so its random phase exposes
    // the stream after the retained/discarded legacy draw (both ranks).
    legacy.controlChange(120,0,1); isolated.controlChange(120,0,1);
    legacy.noteOn(101,1); isolated.noteOn(101,1);
    for(int r=0;r<2;++r) check(legacy.getRankAudioState(1,r).phase==isolated.getRankAudioState(1,r).phase,"preservation retains legacy RNG consumption");
    Engine e; check(equal(e.getRankAudioState(-1,0),{}),"invalid diagnostic index"); check(equal(e.getRankAudioState(0,2),{}),"invalid diagnostic rank");
    std::printf("Active-tail continuity: %d checks, %d failures\n",checks,failures); return failures ? 1 : 0;
}

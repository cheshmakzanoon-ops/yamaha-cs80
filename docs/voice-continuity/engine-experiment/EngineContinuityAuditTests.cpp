// Independent immutable-428 Engine experiment audit. Not a production target.
// Compile WITHOUT CS80_CONTINUITY_AUDIT_CANDIDATE against pristine428 for the
// identical raw corpus; define it only against the separately patched snapshot.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

using namespace cs80;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* what){++checks;if(!ok){if(failures<30)std::fprintf(stderr,"FAIL: %s\n",what);++failures;}}
ControlFrame frame(int policy=1,int source=13) {
    ControlFrame f;f.hardwareSources={source,source};auto& p=f.effectiveValues;
    p[P_parityMode]=policy==2?1.f:0.f;p[P_sourcePerformance]=policy==1?1.f:0.f;
    p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=0;p[P_keyRelease]=p[P_masterSustain]=0;
    p[P_tuningRef]=440;p[P_sustainMode]=0;p[P_mpeOn]=1;p[P_arpOn]=0;
    for(int x:std::initializer_list<int>{P_subVco,P_subVcf,P_subVca,P_subAtVco,P_subAtVcf,P_subAtSpeed,
               P_lfo1Depth,P_lfo2Depth,P_lfo3Depth,P_ringAmount,P_ringDepth,
               P_fx1Mix,P_fx2Mix,P_fx3Mix,P_fx1Type,P_fx2Type,P_fx3Type,
               P_kbdBrillianceLow,P_kbdBrillianceHigh,P_kbdLevelLow,P_kbdLevelHigh})p[x]=0;
    for(int x:{P_macro1,P_macro2,P_macro3,P_macro4})p[x]=.5f;
    for(int r=0;r<2;++r){p[chParam(r,CP_vR)]=0;p[chParam(r,CP_fR)]=.8f;
        p[chParam(r,CP_fA)]=.03f;p[chParam(r,CP_fD)]=.7f;
        p[chParam(r,CP_noise)]=0;p[chParam(r,CP_saw)]=1;p[chParam(r,CP_pulse)]=1;
        p[chParam(r,CP_hpfOn)]=p[chParam(r,CP_lpfOn)]=1;}
    return f;
}
std::vector<float> render(Engine& e,int n,int block=0,const ControlFrame* repeated=nullptr) {
    std::vector<float> out(static_cast<std::size_t>(n)*2);if(block<=0)block=std::max(n,1);
    for(int i=0;i<n;i+=block){if(repeated)check(e.setControlFrame(*repeated),"repeated frame accepted");
        e.render(out.data()+i*2,std::min(block,n-i));}
    return out;
}
void fill(Engine& e){for(int i=0;i<8;++i)e.noteOn(48+i,.4f+i*.06f,i+2);}
[[maybe_unused]] bool equal(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
[[maybe_unused]] bool equal(Svf::State a,Svf::State b){return equal(a.ic1,b.ic1)&&equal(a.ic2,b.ic2);}
[[maybe_unused]] bool equal(Engine::RankAudioState a,Engine::RankAudioState b){return equal(a.phase,b.phase)&&equal(a.hpf,b.hpf)&&equal(a.lpf,b.lpf);}
[[maybe_unused]] bool equal(FilterEnv::State a,FilterEnv::State b){return a.stage==b.stage&&equal(a.value,b.value)&&equal(a.elapsed,b.elapsed);}
[[maybe_unused]] bool equal(Adsr::State a,Adsr::State b){return a.stage==b.stage&&equal(a.value,b.value)&&equal(a.elapsed,b.elapsed);}

// 126 ordinary configurations: three rates/policies x14 source pairs. The
// masked variant has84 configurations and never enters Source Performance.
// These are new evidence fixtures, not edits to historical golden tests.
int rawCorpus(const char* path,int kind) {
    FILE* file=std::fopen(path,"wb");if(!file)return 2;
    std::uint64_t bytes=0;int cases=0;
    for(double rate:{44100.,48000.,96000.})for(int policy=0;policy<3;++policy)for(int source=0;source<14;++source){
        if(kind==2&&policy==1)continue;
        auto f=frame(policy,source);f.hardwareSources[1]=(source+5)%14;auto& p=f.effectiveValues;
        p[P_analogNoise]=.21f;p[P_voiceDispersion]=.31f;p[P_drift]=.17f;
        p[P_subVco]=.23f;p[P_subVcf]=.27f;p[P_subVca]=.19f;p[P_ringAmount]=.35f;p[P_ringDepth]=.15f;
        p[P_macro1]=.71f;p[P_macro2]=.32f;p[P_macro3]=.65f;p[P_macro4]=.54f;
        p[P_lfo1Depth]=.32f;p[P_lfo1Dest]=2;p[P_fx1Type]=2;p[P_fx1Mix]=.4f;p[P_footMode]=1;
        p[chParam(0,CP_noise)]=.27f;p[chParam(1,CP_noise)]=.39f;
        Engine e;
#if defined(CS80_CONTINUITY_AUDIT_CANDIDATE)
        if(kind!=0){experimental::ExperimentalVoiceContinuityProfile profile;profile.enabled=kind==2;
            if(!e.setExperimentalVoiceContinuity(profile))return 3;}
#endif
        e.prepare(rate);if(!e.setControlFrame(f))return 3;e.setExpression(.56f);fill(e);
        for(int stage=0;stage<14;++stage){
            if(stage==1){e.controlChange(1,.37f);e.polyPressure(48,.42f,2);e.pitchBend(3,.3f);e.controlChange(74,.25f,4);}
            if(stage==2){e.controlChange(64,1,2);for(int i=0;i<4;++i)e.noteOff(48+i,i+2);e.setRibbon(-.12f);}
            if(stage==3){f.hardwareSources[0]=(source+1)%14;p[P_volume]=.32f;p[chParam(0,CP_fA)]=.23f;
                p[chParam(0,CP_vS)]=.4f;p[chParam(0,CP_hpfCutoff)]=1700;p[chParam(0,CP_lpfCutoff)]=6400;if(!e.setControlFrame(f))return 3;}
            if(stage==4){e.controlChange(64,0,2);e.allNotesOff(true);}
            if(stage==6){e.allNotesOff(false);p[chParam(0,CP_hpfOn)]=0;p[chParam(1,CP_lpfOn)]=0;if(!e.setControlFrame(f))return 3;}
            if(stage==8){e.noteOn(69,.82f,2);e.noteOn(72,.62f,3);e.pitchBend(2,-.21f);e.polyPressure(69,.8f,2);}
            if(stage==9){f.hardwareSources={11,12};if(!e.setControlFrame(f))return 3;e.noteOff(69,2);}
            if(stage==10)e.prepare(rate);
            if(stage==11){e.noteOn(96,.91f,3);e.noteOn(44,.43f,4);e.controlChange(120,0,3);}
            if(stage==12){const int next=kind==2?(policy==0?2:0):(policy+1)%3;
                p[P_parityMode]=next==2?1.f:0.f;p[P_sourcePerformance]=next==1?1.f:0.f;
                if(!e.setControlFrame(f))return 3;
                e.noteOn(63,.7f,2);}
            if(stage==13){e.controlChange(123,0,2);e.setRibbon(.21f);}
            const int n=(stage==5||stage==7)?2048:257+stage*37;
            const auto out=render(e,n,stage%2?17:0);
            for(float value:out)if(!std::isfinite(value))return 4;
            if(std::fwrite(out.data(),sizeof(float),out.size(),file)!=out.size())return 5;
            bytes+=out.size()*sizeof(float);
        }
        ++cases;
    }
    const bool ok=std::fclose(file)==0;
    std::printf("Engine continuity raw corpus: %d cases, %llu bytes, kind%d\n",cases,static_cast<unsigned long long>(bytes),kind);
    return ok?0:6;
}
#if defined(CS80_CONTINUITY_AUDIT_CANDIDATE)
using Profile=experimental::ExperimentalVoiceContinuityProfile;
using Circuit=experimental::ContinuousVoiceCircuit;
bool equal(const Circuit::State& a,const Circuit::State& b){
    if(a.prepared!=b.prepared||!equal(a.sampleRate,b.sampleRate)||!equal(a.heldKeyVolts,b.heldKeyVolts)||a.samples!=b.samples)return false;
    for(int r=0;r<2;++r)if(!equal(a.ranks[r].phase,b.ranks[r].phase)||!equal(a.ranks[r].hpf,b.ranks[r].hpf)||!equal(a.ranks[r].lpf,b.ranks[r].lpf))return false;
    return true;
}
bool equal(const std::vector<float>& a,const std::vector<float>& b){
    return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](float x,float y){return equal(x,y);});
}
void finite(const std::vector<float>& out){for(float x:out)check(std::isfinite(x),"render finite");}
void setup(Engine& e,const ControlFrame& f,Profile p={},double rate=48000.){
    p.enabled=true;check(e.setExperimentalVoiceContinuity(p),"profile accepted");e.prepare(rate);
    check(e.setControlFrame(f),"frame accepted");check(e.isExperimentalVoiceContinuityActive(),"effective Source Performance");
}
int findVoice(Engine& e,int note){for(int v=0;v<8;++v)if(e.getVoiceInfo(v).note==note)return v;return -1;}
void allStatesEqual(Engine& a,Engine& b,const char* message){for(int v=0;v<8;++v){
    check(equal(a.getExperimentalVoiceCircuitState(v),b.getExperimentalVoiceCircuitState(v)),message);
    for(int r=0;r<2;++r){check(equal(a.getRankEnvelopeState(v,r).filter,b.getRankEnvelopeState(v,r).filter),message);
        check(equal(a.getRankEnvelopeState(v,r).amplitude,b.getRankEnvelopeState(v,r).amplitude),message);}}}
void testColdAndTransactions(){
    Profile p;p.enabled=true;p.coldMidiNote=37;p.initialKeyVolts=.28125f;p.phaseSeed=911;p.idleNoiseSeed=417;
    auto f=frame();Engine a,b;setup(a,f,p);setup(b,f,p);
    Rng phase(p.phaseSeed);
    for(int v=0;v<8;++v){const auto s=a.getExperimentalVoiceCircuitState(v);const auto vi=a.getVoiceInfo(v);
        check(s.prepared&&equal(s.sampleRate,96000.f)&&s.samples==0,"explicit prepared cold clock");
        check(equal(s.heldKeyVolts,p.initialKeyVolts),"explicit cold K independent of MIDI note");
        check(!vi.active&&!vi.held&&!vi.pedalHeld&&vi.note==37&&vi.channel==1,"explicit cold allocation state");
        check(equal(vi.velocity,0)&&equal(vi.pressure,0)&&equal(vi.bend,0)&&equal(vi.ribbonRatio,1),"cold expression reset");
        for(int r=0;r<2;++r){check(equal(s.ranks[r].phase,phase.nextUnipolar()),"explicit cold phase seed order");
            check(equal(s.ranks[r].hpf,{})&&equal(s.ranks[r].lpf,{}),"cold SVF state zero");}}
    check(!a.getExperimentalVoiceCircuitState(-1).prepared&&!a.getExperimentalVoiceCircuitState(8).prepared,"invalid circuit indices inert");
    finite(render(a,31));finite(render(b,31));allStatesEqual(a,b,"cold deterministic clock");
    fill(a);fill(b);render(a,97);render(b,97);
    std::vector<Profile> invalid;for(int version:{-1,0,2}){auto q=p;q.version=version;invalid.push_back(q);}
    for(int note:{-1,128}){auto q=p;q.coldMidiNote=note;invalid.push_back(q);}
    for(float k:{-1.f,1024.001f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto q=p;q.initialKeyVolts=k;invalid.push_back(q);}
    {auto q=p;q.phaseSeed=0;invalid.push_back(q);q=p;q.idleNoiseSeed=0;invalid.push_back(q);}
    for(const auto& q:invalid){check(!a.setExperimentalVoiceContinuity(q),"invalid profile rejected");allStatesEqual(a,b,"invalid profile leaves current state untouched");
        check(equal(render(a,7),render(b,7)),"invalid profile leaves future audio untouched");allStatesEqual(a,b,"invalid profile leaves future hidden state untouched");}
    check(a.setExperimentalVoiceContinuity(p),"identical profile accepted");allStatesEqual(a,b,"identical profile is no-op");
    // Dirty glide/controllers/contours must not survive explicit reset of new idle clocks.
    a.pitchBend(2,.7f);a.polyPressure(48,.9f,2);a.controlChange(74,.8f,2);render(a,57);a.reset();b.reset();
    allStatesEqual(a,b,"reset resets newly clocked states");check(equal(render(a,211),render(b,211)),"dirty/fresh reset future audio exact");
    auto q=p;q.phaseSeed+=1;check(a.setExperimentalVoiceContinuity(q),"effective seed edit accepted");
    Engine fresh;setup(fresh,f,q);allStatesEqual(a,fresh,"effective profile change reinitializes");
    check(equal(render(a,33),render(fresh,33)),"effective profile change future exact");
    check(a.getExperimentalRejectedSamples()==0,"valid cold profile no rejected samples");
    // Cold zero is a valid held node, not a divide-by-zero or inferred last key.
    q.initialKeyVolts=0;Engine zero;setup(zero,f,q);const auto before=zero.getExperimentalVoiceCircuitState(0);
    finite(render(zero,29));const auto after=zero.getExperimentalVoiceCircuitState(0);
    check(equal(after.heldKeyVolts,0)&&after.samples==58,"cold zero K held while unassigned");
    check(equal(before.ranks[0].phase,after.ranks[0].phase),"zero K does not invent oscillator pitch");
    zero.noteOn(60,.8f,2);render(zero,1);check(zero.getExperimentalRejectedSamples()==0,"zero cold K then note accepted");
}
void testClocksAndPanic(){
    for(int source:{0,7,11,12,13}){auto f=frame(1,source);Engine a,b;setup(a,f);setup(b,f);
        fill(a);fill(b);render(a,311);render(b,311);a.allNotesOff(false);b.allNotesOff(false);
        // Identical downstream histories, different hidden circuit excitation: no
        // new audio may leak across the retired allocation gate. Whole-output
        // immediate zero is NOT asserted (decimator/PRA/FX histories remain).
        auto changed=f;changed.effectiveValues[chParam(0,CP_noise)]=1;
        changed.effectiveValues[chParam(1,CP_hpfOn)]=0;
        changed.effectiveValues[chParam(1,CP_lpfOn)]=0;
        check(b.setControlFrame(changed),"post-panic local frame accepted");
        const auto s0=a.getExperimentalVoiceCircuitState(0);
        const auto ao=render(a,113),bo=render(b,113);check(equal(ao,bo),"panic separate audio gate blocks changed idle circuits");finite(ao);
        bool different=false;
        for(int v=0;v<8;++v){const auto s=a.getExperimentalVoiceCircuitState(v);
            check(!a.getVoiceInfo(v).active,"panic allocation remains inactive");
            check(s.samples==s0.samples+226,"all eight cold/retired clocks advance");
            for(int r=0;r<2;++r){const auto audio=a.getRankAudioState(v,r);
                check(equal(audio.phase,s.ranks[r].phase)&&equal(audio.hpf,s.ranks[r].hpf)&&equal(audio.lpf,s.ranks[r].lpf),"audio diagnostic routes to actual kernel");
                check(a.getRankEnvelopeState(v,r).amplitude.stage==Adsr::Stage::Idle,"panic stale amplitude reset idle");
                different|=!equal(s.ranks[r].lpf,b.getExperimentalVoiceCircuitState(v).ranks[r].lpf);}}
        // PANEL/MEMORY local noise is owned by controls; source columns may override it.
        if(source>=11)check(different,"muted bypassed circuit state responds to controls");
        check(a.getExperimentalRejectedSamples()==0&&b.getExperimentalRejectedSamples()==0,"panic/idle valid domain");
    }
    auto f=frame();f.effectiveValues[chParam(0,CP_hpfOn)]=f.effectiveValues[chParam(0,CP_lpfOn)]=0;
    f.effectiveValues[chParam(1,CP_hpfOn)]=f.effectiveValues[chParam(1,CP_lpfOn)]=0;
    Engine e;setup(e,f);const auto before=e.getExperimentalVoiceCircuitState(0);render(e,117);const auto after=e.getExperimentalVoiceCircuitState(0);
    for(int r=0;r<2;++r){check(!equal(before.ranks[r].phase,after.ranks[r].phase),"mute+bypass oscillator continues");
        check(!equal(before.ranks[r].hpf,after.ranks[r].hpf),"mute+bypass HP memory continues");
        check(!equal(before.ranks[r].lpf,after.ranks[r].lpf),"mute+bypass LP memory continues");}
}
void testEnvelopeOwnership(){
    for(int source:{0,1,3,6,7,9}){Engine a,b;auto f=frame(1,source);setup(a,f);b.prepare(48000);check(b.setControlFrame(f),"source oracle frame");fill(a);fill(b);
        for(int stage=0;stage<9;++stage){if(stage==2){a.allNotesOff(true);b.allNotesOff(true);}if(stage==4){a.allNotesOff(false);b.allNotesOff(false);}
            if(stage==6){f.effectiveValues[chParam(0,CP_fR)]=.43f;check(a.setControlFrame(f)&&b.setControlFrame(f),"source live EG frame");}
            render(a,103+stage*11);render(b,103+stage*11);
            for(int v=0;v<8;++v)for(int r=0;r<2;++r)check(equal(a.getRankEnvelopeState(v,r).filter,b.getRankEnvelopeState(v,r).filter),"source filter EG has exactly existing single owner");}
    }
    for(int source:{11,12,13}){Engine a,b;auto shortFrame=frame(1,source),longFrame=shortFrame;
        for(int r=0;r<2;++r)longFrame.effectiveValues[chParam(r,CP_vR)]=1;
        setup(a,shortFrame);b.prepare(48000);check(b.setControlFrame(longFrame),"PANEL long VCA oracle frame");fill(a);fill(b);
        render(a,3072);render(b,3072);a.allNotesOff(true);b.allNotesOff(true);
        for(int stage=0;stage<6;++stage){render(a,1024);render(b,1024);for(int v=0;v<8;++v)for(int r=0;r<2;++r)
            check(equal(a.getRankEnvelopeState(v,r).filter,b.getRankEnvelopeState(v,r).filter),"PANEL/MEMORY retired filter EG exactly one tick");}
        check(!a.getVoiceInfo(0).active&&b.getVoiceInfo(0).active,"short/long VCA oracle distinguishes retirement");
    }
}
void testKeyAndExpression(){
    for(int mode=0;mode<2;++mode){auto f=frame();f.effectiveValues[P_sustainMode]=static_cast<float>(mode);
        f.effectiveValues[P_portamento]=1;f.effectiveValues[P_glideMode]=1;
        Engine e;setup(e,f);e.noteOn(48,.8f,2);render(e,97);const int v=findVoice(e,48);check(v>=0,"assigned voice found");
        const float key=e.getExperimentalVoiceCircuitState(v).heldKeyVolts;e.noteOff(48,2);render(e,701);
        const float released=e.getExperimentalVoiceCircuitState(v).heldKeyVolts;
        check(mode==0?equal(key,released):released<key,"SI holds at release / SII per-circuit upstream glide continues");
        e.noteOn(100,.7f,3);render(e,47);const float afterOther=e.getExperimentalVoiceCircuitState(v).heldKeyVolts;
        check(mode==0?equal(key,afterOther):afterOther<=released,"new other-circuit note does not overwrite retained key target");
        check(equal(e.getRankInfo(v,0).keyVolts,afterOther)&&equal(e.getRankInfo(v,1).keyVolts,afterOther),"both rank diagnostics expose actual held K");
        check(e.getExperimentalRejectedSamples()==0,"key mapping remains accepted");
    }
    // Disconnected member expressions no longer drive an idle physical circuit;
    // shared/global bend remains a separate explicit overlay.
    auto f=frame();Engine a,b;setup(a,f);setup(b,f);fill(a);fill(b);render(a,100);render(b,100);
    a.pitchBend(2,.8f);a.polyPressure(48,.9f,2);a.controlChange(74,.7f,2);
    a.allNotesOff(false);b.allNotesOff(false);render(a,1);render(b,1);
    const auto vi=a.getVoiceInfo(findVoice(a,48));check(equal(vi.velocity,0)&&equal(vi.pressure,0)&&equal(vi.bend,0),"retired member expression clears");
    allStatesEqual(a,b,"pre-panic member controls cannot leave idle modulation residue");
}
void testPartitioningAndMask(){
    for(int source:{0,7,11,13}){auto f=frame(1,source);f.effectiveValues[P_analogNoise]=.22f;
        f.effectiveValues[chParam(0,CP_noise)]=.3f;f.effectiveValues[chParam(1,CP_noise)]=.7f;
        Engine a,b;setup(a,f);setup(b,f);fill(a);fill(b);
        for(int stage=0;stage<10;++stage){if(stage==2){a.noteOff(48,2);b.noteOff(48,2);}if(stage==3){a.allNotesOff(false);b.allNotesOff(false);}
            if(stage==5){a.noteOn(69,.7f,4);b.noteOn(69,.7f,4);}if(stage==6){f.hardwareSources={13,12};check(a.setControlFrame(f)&&b.setControlFrame(f),"partition selector frame");}
            if(stage==7){a.controlChange(64,1,4);b.controlChange(64,1,4);a.noteOff(69,4);b.noteOff(69,4);}if(stage==8){a.controlChange(64,0,4);b.controlChange(64,0,4);}
            check(equal(render(a,127+stage*13),render(b,127+stage*13,7,&f)),"partition/repeated same frame audio bit exact");
            allStatesEqual(a,b,"partition/repeated same frame states bit exact");}
        check(a.getExperimentalRejectedSamples()==0&&b.getExperimentalRejectedSamples()==0,"partition/noise no rejected samples");
    }
    for(int policy:{0,2}){auto f=frame(policy);Engine a,b;a.prepare(48000);b.prepare(48000);check(a.setControlFrame(f)&&b.setControlFrame(f),"masked frame");fill(a);fill(b);render(a,107);render(b,107);
        Profile p;p.enabled=true;p.phaseSeed=66;p.initialKeyVolts=0;check(a.setExperimentalVoiceContinuity(p),"masked profile edit accepted");
        check(!a.isExperimentalVoiceContinuityActive(),"Legacy/Isolation mask experiment");
        check(!a.getExperimentalVoiceCircuitState(0).prepared,"masked circuit diagnostic absent");
        check(equal(render(a,211),render(b,211)),"masked edit does not reset or alter audible state");
    }
}
void testTransitionsResetAndNoise(){
    auto f=frame();f.effectiveValues[P_sustainMode]=1;
    f.effectiveValues[P_portamento]=.8f;f.effectiveValues[P_glideMode]=1;
    f.effectiveValues[P_kbdBrillianceLow]=.3f;f.effectiveValues[P_kbdLevelHigh]=.4f;
    Profile p;p.enabled=true;p.coldMidiNote=31;p.initialKeyVolts=.125f;
    Engine dirty,fresh;setup(dirty,f,p);setup(fresh,f,p);
    fill(dirty);render(dirty,317);dirty.noteOn(110,.8f,14);dirty.pitchBend(14,.7f);
    dirty.controlChange(74,.9f,14);dirty.polyPressure(110,.8f,14);render(dirty,313);
    dirty.reset();fresh.reset();check(equal(render(dirty,401),render(fresh,401)),"dirty reset output equals cold reference");
    allStatesEqual(dirty,fresh,"SII exposes deterministic reset glide/key/contour fields");
    dirty.prepare(44100);fresh.prepare(44100);check(dirty.setControlFrame(f)&&fresh.setControlFrame(f),"reprepare frame");
    check(equal(render(dirty,117),render(fresh,117)),"reprepare/new rate cold future exact");
    allStatesEqual(dirty,fresh,"reprepare resets new states");
    for(int policy:{0,2}){auto masked=frame(policy);check(dirty.setControlFrame(masked),"exit experimental policy");
        check(!dirty.isExperimentalVoiceContinuityActive()&&!dirty.getExperimentalVoiceCircuitState(0).prepared,"policy exit hides kernel");
        dirty.noteOn(100,.8f,8);render(dirty,79);check(dirty.setControlFrame(f),"reenter experimental policy");
        Engine ref;setup(ref,f,p,44100);allStatesEqual(dirty,ref,"policy reentry freshly initializes circuits");
        check(equal(render(dirty,83),render(ref,83)),"policy reentry future exact");}
    // Different surrogate idle streams cannot shift the shared audible/output RNG.
    // An unplayed idle circuit is allowed to differ; no physical noise identity is claimed.
    auto noisy=frame();noisy.effectiveValues[P_analogNoise]=.3f;
    noisy.effectiveValues[chParam(0,CP_noise)]=.6f;noisy.effectiveValues[chParam(1,CP_noise)]=.8f;
    Profile pa,pb;pa.enabled=pb.enabled=true;pb.idleNoiseSeed=pa.idleNoiseSeed+1;
    Engine a,b;setup(a,noisy,pa);setup(b,noisy,pb);a.noteOn(60,.8f,2);b.noteOn(60,.8f,2);
    check(equal(render(a,513),render(b,513)),"idle-only noise seed cannot perturb shared audible/output RNG");
    const int av=findVoice(a,60);check(equal(a.getExperimentalVoiceCircuitState(av),b.getExperimentalVoiceCircuitState(av)),"active circuit identical despite idle stream seed");
    bool idleDiff=false;for(int v=0;v<8;++v)if(v!=av)idleDiff|=!equal(a.getExperimentalVoiceCircuitState(v),b.getExperimentalVoiceCircuitState(v));
    check(idleDiff,"separate idle surrogate actually excites hidden memories");
}
void testExpandedKernelDomain(){
    Circuit c,reference;check(c.prepare(96000,.5f,{.2f,.7f})&&reference.prepare(96000,.5f,{.2f,.7f}),"domain oracle prepare");
    Circuit::Controls ctrl;ctrl.ranks[0].vcoMultiplier=262144;ctrl.ranks[1].vcoMultiplier=262144;
    check(c.processSample(ctrl).accepted&&reference.processSample(ctrl).accepted,"expanded exact upper multiplier accepted");
    const auto before=c.getState();ctrl.ranks[1].vcoMultiplier=std::nextafter(262144.f,std::numeric_limits<float>::infinity());
    check(!c.processSample(ctrl).accepted,"nextafter upper multiplier rejected");check(equal(before,c.getState()),"invalid second rank preserves entire circuit");
    ctrl.ranks[1].vcoMultiplier=262144;check(c.processSample(ctrl).accepted&&reference.processSample(ctrl).accepted,"valid upper multiplier resumes");
    check(equal(c.getState(),reference.getState()),"invalid domain leaves future hidden coefficient evolution unchanged");
    // Interface extremes, not a proof of all parameter combinations or physical accuracy.
    for(int sign:{-1,1}){auto f=frame();auto& p=f.effectiveValues;p[P_pitchBendRange]=48;p[P_mpeBendRange]=96;p[P_masterTune]=2.f*sign;
        p[P_subVco]=p[P_subAtVco]=p[P_modWheelSub]=1;p[P_subOriginalMode]=1;p[P_subOriginalFunction]=5;p[P_subExtLevel]=1;
        p[P_lfo1Depth]=p[P_lfo2Depth]=p[P_lfo3Depth]=1;p[P_lfo1Dest]=p[P_lfo2Dest]=p[P_lfo3Dest]=1;
        p[P_voiceDispersion]=p[P_drift]=1;p[P_ribbon]=static_cast<float>(sign);
        for(int r=0;r<2;++r)p[chParam(r,CP_feet)]=sign>0?0.f:5.f;
        Engine e;setup(e,f);e.noteOn(sign>0?127:0,1,2);e.pitchBend(1,static_cast<float>(sign));e.pitchBend(2,static_cast<float>(sign));e.controlChange(1,1);e.polyPressure(sign>0?127:0,1,2);
        std::array<float,2048> external {};std::array<float,4096> audio {};Rng input(919);
        for(float& x:external)x=input.nextUInt()&1?1.f:-1.f;
        e.render(audio.data(),2048,external.data());
        check(std::all_of(audio.begin(),audio.end(),[](float x){return std::isfinite(x);}),"external SUB extreme output finite");
        check(e.getExperimentalRejectedSamples()==0,"extreme interface fixture accepted without silent rejection");}
}
void testBoundedLongRun(){
    for(double rate:{8000.,768000.}){auto f=frame();Engine e;setup(e,f,{},rate);fill(e);finite(render(e,127));e.allNotesOff(false);finite(render(e,127));
        check(e.getExperimentalRejectedSamples()==0,"Engine supported rate endpoints accepted");
        for(int v=0;v<8;++v)check(e.getExperimentalVoiceCircuitState(v).samples==508,"rate endpoints exact internal clocks");}
    auto f=frame();f.effectiveValues[P_resonance]=1;
    for(int r=0;r<2;++r){f.effectiveValues[chParam(r,CP_hpfRes)]=f.effectiveValues[chParam(r,CP_lpfRes)]=1;
        f.effectiveValues[chParam(r,CP_hpfOn)]=f.effectiveValues[chParam(r,CP_lpfOn)]=0;f.effectiveValues[chParam(r,CP_noise)]=.8f;}
    Engine e;setup(e,f);fill(e);render(e,1024);e.allNotesOff(false);
    bool allFinite=true;std::uint64_t frames=0;std::array<float,512> audio {};
    for(int block=0;block<563;++block){e.render(audio.data(),256);frames+=256;
        allFinite&=std::all_of(audio.begin(),audio.end(),[](float x){return std::isfinite(x);});}
    check(allFinite,"three-second high-Q/noisy muted bypass run finite");
    for(int v=0;v<8;++v){const auto s=e.getExperimentalVoiceCircuitState(v);check(s.samples==2*(1024+frames),"long idle exact clocks");
        for(const auto& r:s.ranks)check(std::isfinite(r.phase)&&std::isfinite(r.hpf.ic1)&&std::isfinite(r.hpf.ic2)&&std::isfinite(r.lpf.ic1)&&std::isfinite(r.lpf.ic2),"long idle hidden state finite");}
    check(e.getExperimentalRejectedSamples()==0,"long idle zero rejections");
}
int audit(){testColdAndTransactions();testClocksAndPanic();testEnvelopeOwnership();testKeyAndExpression();testPartitioningAndMask();testTransitionsResetAndNoise();testExpandedKernelDomain();testBoundedLongRun();
    std::printf("Engine continuity independent audit: %d checks, %d failures\n",checks,failures);return failures?1:0;}
#endif

} // namespace

int main(int argc,char** argv) {
    if(argc==3){const std::string mode=argv[1];if(mode=="--raw")return rawCorpus(argv[2],0);
        if(mode=="--raw-disabled")return rawCorpus(argv[2],1);
        if(mode=="--raw-masked")return rawCorpus(argv[2],2);}
#if defined(CS80_CONTINUITY_AUDIT_CANDIDATE)
    if(argc==1)return audit();
#endif
    std::fprintf(stderr,"usage: audit --raw|--raw-disabled|--raw-masked FILE\n");return 2;
}

// Independent bounded unchanged-path witness. No padded object serialization.
// Ordinary expression after accepted release on a current effective MPE member
// is deliberately excluded. See COVERAGE.md for the non-exhaustive scope.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
using namespace cs80;
namespace {
std::string context;
std::uint64_t checks=0, failures=0, scenarios=0;
void check(bool x, const char* why) {
    ++checks;
    if (!x) { ++failures; std::fprintf(stderr,"FAIL %s: %s\n",context.c_str(),why); }
}
struct Writer {
    FILE* f=nullptr; std::uint64_t scalars=0, bytes=0;
    explicit Writer(const char* p):f(std::fopen(p,"wb")) { if (!f) std::exit(2); }
    void u64(std::uint64_t n) {
        // Fixed little-endian fields, never native structs or padding.
        std::array<unsigned char,8> b{};
        for (int k=0;k<8;++k) b[k]=static_cast<unsigned char>(n>>(8*k));
        if (std::fwrite(b.data(),1,b.size(),f)!=b.size()) std::exit(2);
        ++scalars; bytes+=8;
    }
    void i(int n) { u64(static_cast<std::uint64_t>(static_cast<std::int64_t>(n))); }
    void b(bool n) { u64(n?1:0); }
    void d(double n) { check(std::isfinite(n),"finite serialized double"); u64(std::bit_cast<std::uint64_t>(n)); }
    void x(float n) { check(std::isfinite(n),"finite serialized float"); u64(std::bit_cast<std::uint32_t>(n)); }
    void finish() { check(std::fflush(f)==0,"flush capture"); check(std::fclose(f)==0,"close capture"); f=nullptr; }
};
void state(Writer& w, const Engine& e) {
    w.i(e.activeVoices()); w.i(static_cast<int>(e.getRenderPolicy()));
    w.b(e.isParityMode()); w.i(e.getVoiceMode()); w.i(e.getOversamplingFactor());
    w.i(e.getLatencySamples()); w.d(e.getHostSampleRate()); w.d(e.getInternalSampleRate());
    w.x(e.getDcBlockCoefficient()); w.x(e.getRingFrequency()); w.x(e.getRingEnvelope());
    w.x(e.getSubOscillatorValue()); w.x(e.getSubOscillatorRate());
    w.b(e.hasFreeCircuit()); w.b(e.hasFreeCircuitForRouting()); w.b(e.isModernLayerActive());
    w.i(e.getMpeZones().lowerMemberCount()); w.i(e.getMpeZones().upperMemberCount());
    for (bool upper:{false,true}) { w.x(e.getMpeZones().masterRange(upper)); w.x(e.getMpeZones().memberRange(upper)); }
    const auto& g=e.getModernGlobalOffsets(); w.x(g.subSpeedRatio); w.x(g.subDepthGain);
    for (auto x:g.fxMix) w.x(x);
    for (int m=0;m<3;++m) w.d(e.getModernMsegDisplayPosition(m));
    for (int v=0;v<8;++v) {
        const auto a=e.getVoiceInfo(v);
        w.b(a.active); w.i(a.note); w.i(a.channel); w.x(a.velocity); w.x(a.pressure); w.x(a.bend);
        w.x(a.ribbonRatio); w.b(a.held); w.b(a.pedalHeld); w.x(a.touchA); w.x(a.touchB); w.x(a.touchC);
        for (int m=0;m<3;++m) w.x(e.getModernMsegValue(v,m));
        for (int r=0;r<2;++r) {
            const auto z=e.getRankInfo(v,r);
            for (float x:{z.hpfHz,z.lpfHz,z.filterEnvelope,z.ampEnvelope,z.oscillatorHz,z.keyVolts,z.hpfQ,z.lpfQ}) w.x(x);
            w.b(z.hpfEnabled); w.b(z.lpfEnabled);
            const auto o=e.getModernRankOffsets(v,r);
            for (float x:{o.pitch,o.pulseWidth,o.hpCutoff,o.lpCutoff,o.hpQ,o.lpQ,o.level,o.pan}) w.x(x);
            const auto s=e.getRankAudioState(v,r);
            for (float x:{s.phase,s.hpf.ic1,s.hpf.ic2,s.lpf.ic1,s.lpf.ic2}) w.x(x);
            for (auto f:{s.hpCircuit,s.lpCircuit}) for (double x:{f.input,f.feedback,f.bp,f.lp}) w.d(x);
            const auto aenv=e.getRankEnvelopeState(v,r);
            w.i(static_cast<int>(aenv.amplitude.stage)); w.x(aenv.amplitude.value); w.x(aenv.amplitude.elapsed);
            w.i(static_cast<int>(aenv.filter.stage)); w.x(aenv.filter.value); w.x(aenv.filter.elapsed);
            const auto& osc=e.getRankOscillatorState(v,r);
            for (double x:{osc.charge,osc.resetRemainingSeconds,osc.inputBaselineVolts,osc.triangleBaselineVolts,
                osc.heldFrequencyHz,osc.heldThresholdVolts,osc.elapsedSeconds}) w.d(x);
            w.b(osc.sawEnabled); w.b(osc.pulseEnabled);
            for (const auto& lane:osc.residual) for (double x:lane) w.d(x);
            w.u64(osc.residualCursor); w.u64(osc.resetCount);
        }
    }
    const auto l=e.getListeningSample(); w.x(l.left); w.x(l.right);
    for (int c=0;c<2;++c) {
        const auto s=e.getPraWahState(c);
        for (auto cap:s.capacitors) { w.d(cap.voltage); w.d(cap.current); }
        w.d(s.sampleRate); w.d(s.opticalOhms); w.d(s.targetOhms); w.x(s.wetMix);
    }
    for (int t=0;t<3;++t) {
        const auto s=e.getOutputTapState(t); const auto n=s.nodes;
        for (double x:{s.state.capacitorVolts,s.state.previousSourceVolts,s.state.readoutGain,
            n.capacitorInputVolts,n.capacitorVolts,n.commonVolts,n.highTapVolts,n.lowTapVolts,n.jackVolts,
            n.sourceAmps,n.highSeriesAmps,n.lowSeriesAmps,n.shuntAmps,n.loadAmps,n.audibleJackVolts,s.sampleRate}) w.d(x);
        w.i(static_cast<int>(s.mode));
    }
}
struct Run {
    Writer& w; Engine& e; int partition; int stage=0;
    void snap() { w.i(stage++); state(w,e); }
    void render(int frames, bool sounding=true) {
        std::vector<float> out(static_cast<std::size_t>(frames)*2,std::numeric_limits<float>::quiet_NaN());
        constexpr std::array<int,7> chunks{1,7,31,3,64,2,127};
        for (int done=0,k=0;done<frames;++k) {
            const int n=std::min(frames-done,partition?chunks[k%chunks.size()]:frames);
            e.render(out.data()+2*done,n); done+=n;
        }
        if (sounding) check(std::any_of(out.begin(),out.end(),[](float x){return std::abs(x)>1.e-8f;}),"non-silent continuation");
        w.i(frames); for (auto x:out) w.x(x); snap();
    }
};
void rpn(Engine& e,int ch,int num,int val) {
    e.controlChange(101,0,ch); e.controlChange(100,num/127.f,ch); e.controlChange(6,val/127.f,ch);
    e.controlChange(101,1,ch); e.controlChange(100,1,ch);
}
void zones(Engine& e,int lower,int upper) {
    rpn(e,1,6,0); rpn(e,16,6,0); if(lower)rpn(e,1,6,lower); if(upper)rpn(e,16,6,upper);
}
void expression(Engine& e,int ch,float sign=1) {
    e.pitchBend(ch,sign*.125f); e.channelPressure(ch,.625f); e.controlChange(74,.75f,ch);
}
int find(const Engine& e,int note,int ch) {
    for(int v=0;v<8;++v) {auto x=e.getVoiceInfo(v); if(x.active&&x.note==note&&x.channel==ch)return v;} return -1;
}
Engine::VoiceInfo voice(const Engine& e,int note,int ch) {
    int v=find(e,note,ch); check(v>=0,"expected allocated active voice"); return e.getVoiceInfo(v);
}
void held(const Engine& e,int note,int ch) { check(voice(e,note,ch).held,"accepted key held"); }
void released(const Engine& e,int note,int ch,bool pedal=false) {
    const auto v=voice(e,note,ch); check(!v.held,"accepted key release"); check(v.pedalHeld==pedal,"expected pedal ownership");
}
ParamValues patch(int policy,int quality,bool mpe) {
    ParamValues p; p[P_sourcePerformance]=policy!=0; p[P_parityMode]=policy==2; p[P_mpeOn]=mpe;
    p[P_oversampling]=static_cast<float>(quality); p[P_kasScanTiming]=p[P_arpOn]=0;
    p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=p[P_detune]=p[P_chorusOn]=p[P_initBend]=0;
    p[P_keyRelease]=5; p[P_mpeBendRange]=48; p[P_pitchBendRange]=2;
    p[P_sustainMode]=0; p[P_masterSustain]=0; p[P_sustainAssign]=p[P_footswitchPlugged]=1; p[P_footswitchPressed]=0;
    p[P_mod1Amount]=1; p[P_mod2Amount]=.15f; p[P_ribbonCcEnable]=0;
    for(int r=0;r<2;++r) {
        p[chParam(r,CP_vA)]=p[chParam(r,CP_fA)]=.001f;
        p[chParam(r,CP_vS)]=1; p[chParam(r,CP_vR)]=p[chParam(r,CP_fR)]=2;
        p[chParam(r,CP_pulse)]=1; p[chParam(r,CP_afterBrill)]=.2f;
    }
    return p;
}
const char* names[]={"split-held-fresh","lower-full-endpoint","upper-full-endpoint","conventional-tails",
    "manager-gap-tails","explicit-cc121","mpe-mode-transition","member-to-gap","member-to-manager","lifecycle"};
void scenario(Writer& w,int kind,int policy,int quality,int rate,int partition) {
    context=std::string(names[kind])+"/policy="+std::to_string(policy)+"/quality="+std::to_string(quality)+"/rate="+std::to_string(rate);
    ++scenarios; w.i(kind); w.i(policy); w.i(quality); w.i(rate);
    Engine e; e.prepare(rate); auto p=patch(policy,quality,kind!=3); e.setParams(p);
    modern::ModernConfig config;
    config.slots[0]={modern::ModSource::timbre,modern::ModSource::none,modern::ModDestination::pulseWidth,modern::ModTarget::both};
    config.slots[1]={modern::ModSource::polyPressure,modern::ModSource::none,modern::ModDestination::lpCutoff,modern::ModTarget::both};
    e.setModernConfig(config);
    Run r{w,e,partition}; r.snap();
    int a=2,b=15;
    if(kind==1){zones(e,15,0);a=2;b=16;}
    else if(kind==2||kind==8){zones(e,0,15);a=1;b=15;}
    else zones(e,2,2);
    // All first onsets lack expression history. No deferred scanner or ARP gates.
    e.noteOn(60,.8f,a); e.noteOn(67,.7f,b); held(e,60,a); held(e,67,b); r.render(512);
    if(policy!=2 && kind!=3) {
        check(voice(e,60,a).bend==0 && voice(e,60,a).pressure==0,"fresh member controls neutral");
    }
    expression(e,a); expression(e,b,-1); r.snap(); r.render(97);
    check(voice(e,60,a).pressure==.625f,"held channel pressure routed");
    if(policy!=2 && kind!=3) {
        check(voice(e,60,a).bend==6 && voice(e,67,b).bend==-6,"held member bends routed");
        check(e.getModernRankOffsets(find(e,60,a),0).pulseWidth>.01f,"held timbre reaches modern destination");
    }
    // Held update at a different value; captures immediate target and ramp state.
    e.pitchBend(a,.0625f); e.channelPressure(a,.375f); e.controlChange(74,.25f,a);
    r.snap(); r.render(193);
    if(kind==3||kind==5) e.controlChange(64,1,b);
    e.noteOff(60,a); e.noteOff(67,b);
    released(e,60,a); released(e,67,b,(kind==3||kind==5)&&policy!=2); r.snap(); r.render(257);
    if(kind<=2) {
        // Ordinary post-release member expression intentionally absent.
        e.polyPressure(60,.45f,a); r.snap(); r.render(129);
    } else if(kind==3) {
        // Effective conventional mode, including released and pedal-held voices.
        expression(e,a,-1); expression(e,b); r.snap(); r.render(257);
        check(voice(e,60,a).pressure==.625f,"conventional released pressure updated");
        e.controlChange(64,0,b); r.render(193);
    } else if(kind==4) {
        e.noteOn(72,.65f,8); r.render(193); e.noteOff(72,8); released(e,72,8); r.render(129);
        expression(e,8); expression(e,1,-1); expression(e,16); r.snap(); r.render(257);
        check(voice(e,72,8).pressure==.625f,"gap released pressure updated");
        check(voice(e,60,a).pressure==.625f,"lower manager released pressure updated");
        if(policy!=2) check(voice(e,67,b).pressure==.625f,"upper manager released pressure updated");
    } else if(kind==5) {
        // Explicit CC121 is unchanged and must reset nonzero released targets.
        if(policy!=2) {
            check(voice(e,60,a).bend!=0&&voice(e,60,a).pressure!=0,"released reset precondition nonzero");
            check(voice(e,67,b).bend!=0&&voice(e,67,b).pressure!=0,"pedal reset precondition nonzero");
        }
        e.controlChange(121,0,a); e.controlChange(121,0,b); r.snap();
        if(policy!=2) {
            check(voice(e,60,a).bend==0&&voice(e,60,a).pressure==0,"explicit released reset");
            check(voice(e,67,b).bend==0&&voice(e,67,b).pressure==0,"explicit pedal reset");
            released(e,67,b);
        }
        r.render(257);
        if(policy!=2) check(e.getModernRankOffsets(find(e,60,a),0).pulseWidth>.01f,"CC121 retains CC74");
    } else if(kind==6) {
        p[P_mpeOn]=0; e.setParams(p); expression(e,a,-1); expression(e,b); r.snap(); r.render(193);
        p[P_mpeOn]=1; e.setParams(p); r.snap(); r.render(129);
        // No ordinary expression targets the old released member voices here.
        e.noteOn(72,.8f,3); held(e,72,3); r.render(257);
    } else if(kind==7) {
        zones(e,0,2); // old lower member becomes a gap; upper voice remains released and untouched.
        if(policy!=2) check(e.getMpeZones().role(a)==MpeZoneLayout::Role::none,"current role is gap");
        expression(e,a,-1); r.snap(); r.render(257);
        check(voice(e,60,a).pressure==.625f,"new gap updates released voice");
    } else if(kind==8) {
        zones(e,2,0); // old upper endpoint channel 1 becomes lower manager.
        if(policy!=2) check(e.getMpeZones().role(a)==MpeZoneLayout::Role::lowerMaster,"current role is manager");
        expression(e,a,-1); r.snap(); r.render(257);
        check(voice(e,60,a).pressure==.625f,"new manager own-channel released pressure");
    } else {
        e.reset(); r.snap(); e.noteOn(60,.75f,a); held(e,60,a); r.render(257);
        e.setNonRealtime(true); r.snap(); e.noteOn(64,.8f,b); r.render(257);
        e.setNonRealtime(false); e.prepare(rate); e.setParams(p); e.setModernConfig(config); r.snap();
        e.noteOn(69,.8f,2); held(e,69,2); r.render(257);
    }
    // One zero-frame callback and autonomous continuation, always identical scope.
    float guard[2]={.123f,-.456f}; e.render(guard,0);
    check(guard[0]==.123f&&guard[1]==-.456f,"zero-frame callback preserves output"); r.snap(); r.render(389);
}
}
int main(int argc,char**argv) {
    if(argc!=3)return 2;
    const int partition=std::atoi(argv[2]);
    if(partition<0||partition>1)return 2;
    Writer w(argv[1]); w.u64(0x4d5045434f4d5031ULL);
    for(int rate:{44100,48000,96000}) for(int policy:{0,1,2}) for(int quality:{0,1,2})
        for(int kind=0;kind<10;++kind) scenario(w,kind,policy,quality,rate,partition);
    w.finish(); std::printf("scenarios=%llu checks=%llu failures=%llu scalars=%llu bytes=%llu partition=%d\n",
        (unsigned long long)scenarios,(unsigned long long)checks,(unsigned long long)failures,
        (unsigned long long)w.scalars,(unsigned long long)w.bytes,partition);
    return failures?1:0;
}

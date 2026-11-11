// Product policy: MPE Appendix D permits maximum combination; it does not
// mandate it. Combine raw member/per-key and zone-manager pressure, then apply
// the existing controller curve once. This is an actual-core target regression.
#include "core/Engine.h"
#include "core/ControllerCurves.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
std::string context;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s: %s\n", context.c_str(), label); }
}
ParamValues patch(int policy) {
    ParamValues p;
    p[P_sourcePerformance] = float(policy);
    p[P_parityMode] = 0; p[P_mpeOn] = 1;
    p[P_analogNoise] = p[P_voiceDispersion] = p[P_drift] = p[P_detune] = 0;
    p[P_kasScanTiming] = p[P_arpOn] = p[P_chorusOn] = p[P_initBend] = 0;
    p[P_masterSustain] = 0; p[P_keyRelease] = 5;
    p[P_sustainAssign] = p[P_footswitchPlugged] = 1;
    p[P_footswitchPressed] = 0;
    p[P_pressureCurve] = p[P_pressureThreshold] = 0;
    return p;
}
void rpn(Engine& e, int ch, int count) {
    e.controlChange(101, 0, ch); e.controlChange(100, 6.0f/127, ch);
    e.controlChange(6, count/127.0f, ch);
}
void layout(Engine& e, int lower=2, int upper=2) {
    rpn(e, 1, 0); rpn(e, 16, 0);
    if (lower) rpn(e, 1, lower);
    if (upper) rpn(e, 16, upper);
}
void setup(Engine& e, ParamValues p) { e.prepare(48000); e.setParams(p); layout(e); }
void render(Engine& e) {
    std::array<float, 256> out;
    out.fill(std::numeric_limits<float>::quiet_NaN()); e.render(out.data(), 128);
    check(std::all_of(out.begin(), out.end(), [](float x){return std::isfinite(x);}), "render writes finite samples");
}
int find(const Engine& e, int note, int ch) {
    for (int i=0; i<8; ++i) {
        const auto v=e.getVoiceInfo(i);
        if (v.active && v.note==note && v.channel==ch) return i;
    }
    return -1;
}
void target(const Engine& e, int note, int ch, float raw, float curve=0, float threshold=0) {
    const int i=find(e,note,ch); check(i>=0,"expected active circuit exists");
    if (i<0) return;
    const float expected=applyPressureCurve(raw,curve,threshold), actual=e.getVoiceInfo(i).pressure;
    const bool same=std::isfinite(actual) && std::bit_cast<unsigned>(actual)==std::bit_cast<unsigned>(expected);
    if (!same) std::printf("  note=%d ch=%d actual=%g expected=%g raw=%g\n",note,ch,actual,expected,raw);
    check(same,"exact combined pressure target");
}
void begin(const std::string& name,int policy,int ch) {
    context=std::string(name)+" policy="+std::to_string(policy)+" member="+std::to_string(ch);
}
void liveAndOnset() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool managerFirst:{false,true}) {
        begin(managerFirst?"manager first":"member first",policy,ch);
        Engine e; setup(e,patch(policy)); const int manager=ch==2?1:16, other=ch==2?15:2;
        e.noteOn(60,.8f,ch); e.noteOn(61,.8f,other); e.noteOn(62,.8f,8); e.noteOn(63,.8f,manager);
        if(managerFirst) e.channelPressure(manager,.7f);
        e.channelPressure(ch,.4f);
        if(!managerFirst) e.channelPressure(manager,.7f);
        target(e,60,ch,.7f); target(e,61,other,0); target(e,62,8,0);
        target(e,63,manager,.7f); // Manager's own conventional voice still receives it.
        e.channelPressure(manager,.2f); target(e,60,ch,.4f);
        e.channelPressure(manager,0); target(e,60,ch,.4f); target(e,63,manager,0);
        e.channelPressure(manager,.9f); target(e,60,ch,.9f);
        e.channelPressure(ch,.1f); target(e,60,ch,.9f);
        e.channelPressure(manager,0); target(e,60,ch,.1f);
        e.channelPressure(manager,.6f); e.noteOn(64,.8f,ch); target(e,64,ch,.6f);
        e.channelPressure(ch,.8f); e.noteOn(65,.8f,ch); target(e,65,ch,.8f);
        render(e); target(e,65,ch,.8f);
    }
    for(int policy:{0,1}) {
        begin("independent simultaneous zones",policy,2); Engine e; setup(e,patch(policy));
        e.channelPressure(1,.6f); e.channelPressure(16,.8f);
        e.channelPressure(2,.7f); e.channelPressure(15,.2f);
        e.noteOn(60,.8f,2); e.noteOn(61,.8f,15);
        target(e,60,2,.7f); target(e,61,15,.8f);
        e.channelPressure(1,0); target(e,60,2,.7f); target(e,61,15,.8f);
        e.channelPressure(16,0); target(e,61,15,.2f);
    }
}
void releasedAndPoly() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool pedal:{false,true}) {
        begin(pedal?"pedal held":"release tail",policy,ch); Engine e; setup(e,patch(policy));
        const int manager=ch==2?1:16;
        e.channelPressure(ch,.6f); e.channelPressure(manager,.8f);
        e.noteOn(60,.8f,ch); render(e);
        if(pedal) e.controlChange(64,1,ch);
        e.noteOff(60,ch);
        const int i=find(e,60,ch); check(i>=0,"release fixture remains active");
        if(i>=0) {const auto v=e.getVoiceInfo(i);check(!v.held && v.pedalHeld==pedal,"accepted release/pedal fixture");}
        e.channelPressure(ch,.1f); target(e,60,ch,.8f);
        e.channelPressure(manager,.3f); target(e,60,ch,.6f);
        e.channelPressure(manager,0); target(e,60,ch,.6f);
        e.channelPressure(manager,.9f); target(e,60,ch,.9f);
        e.noteOn(64,.8f,ch); target(e,64,ch,.9f);
        e.channelPressure(manager,0); target(e,60,ch,.6f); target(e,64,ch,.1f);
        // Internal/UI core poly pressure is a per-key component, not a new
        // member-channel cache. It retains its existing released-note scope.
        e.polyPressure(60,.45f,ch); target(e,60,ch,.45f); target(e,64,ch,.1f);
        e.channelPressure(manager,.7f); target(e,60,ch,.7f);
        e.polyPressure(60,.2f,ch); target(e,60,ch,.7f);
        e.channelPressure(manager,0); target(e,60,ch,.2f);
        e.noteOn(67,.8f,ch); target(e,67,ch,.1f);
    }
}
void curvesAndInputs() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(float curve:{-.65f,.7f}) {
        begin("raw components and one curve",policy,ch); Engine e; auto p=patch(policy);
        p[P_pressureCurve]=curve; p[P_pressureThreshold]=.15f; setup(e,p);
        const int manager=ch==2?1:16;
        e.channelPressure(ch,.4f); e.channelPressure(manager,.7f);
        e.noteOn(60,.8f,ch); target(e,60,ch,.7f,curve,.15f);
        e.channelPressure(manager,0); target(e,60,ch,.4f,curve,.15f);
        e.polyPressure(60,.6f,ch); target(e,60,ch,.6f,curve,.15f);
        e.channelPressure(manager,.2f); target(e,60,ch,.6f,curve,.15f);
        for(float x:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
            e.channelPressure(manager,x);
        target(e,60,ch,.6f,curve,.15f);
        e.channelPressure(manager,2); target(e,60,ch,1,curve,.15f);
        e.channelPressure(manager,-1); target(e,60,ch,.6f,curve,.15f);
        p[P_pressureCurve]=-curve; p[P_pressureThreshold]=.25f; e.setParams(p);
        e.channelPressure(manager,.3f); target(e,60,ch,.6f,-curve,.25f);
        e.noteOn(64,.8f,ch); target(e,64,ch,.4f,-curve,.25f);
    }
}
void lifecycle() {
    for(int policy:{0,1}) for(int transition=0;transition<4;++transition) {
        begin("lifecycle "+std::to_string(transition),policy,2);
        Engine e; auto p=patch(policy); setup(e,p);
        e.channelPressure(1,.8f); e.channelPressure(2,.4f);
        if(transition==0) e.reset();
        if(transition==1) {p[P_mpeOn]=0;e.setParams(p);p[P_mpeOn]=1;e.setParams(p);}
        if(transition==2) {rpn(e,1,0);rpn(e,1,2);}
        if(transition==3) {p[P_parityMode]=1;e.setParams(p);p[P_parityMode]=0;e.setParams(p);}
        e.noteOn(60,.8f,2); target(e,60,2,0);
    }
    for(int policy:{0,1}) {
        begin("same manager retained; new member",policy,2); Engine e; setup(e,patch(policy));
        e.channelPressure(1,.8f); rpn(e,1,3); e.noteOn(60,.8f,4); target(e,60,4,.8f);
        begin("controller inheritance",policy,2); Engine source,dest;
        setup(source,patch(policy)); setup(dest,patch(policy));
        source.channelPressure(1,.8f);source.channelPressure(2,.4f);
        dest.inheritControllerStateFrom(source); dest.noteOn(60,.8f,2);target(dest,60,2,.8f);
        dest.channelPressure(1,0);target(dest,60,2,.4f);
    }
}
void controllerResets() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool released:{false,true}) {
        begin(released?"CC121 released":"CC121 held",policy,ch);
        Engine e; setup(e,patch(policy)); const int manager=ch==2?1:16;
        e.channelPressure(ch,.6f); e.channelPressure(manager,.8f);
        e.noteOn(60,.8f,ch); render(e); if(released) e.noteOff(60,ch);
        e.controlChange(121,0,manager); target(e,60,ch,.6f);
        e.noteOn(64,.8f,ch); target(e,64,ch,.6f);
        e.channelPressure(manager,.8f); e.controlChange(121,0,ch);
        target(e,60,ch,.8f); target(e,64,ch,.8f);
        e.noteOn(67,.8f,ch); target(e,67,ch,.8f);
        e.channelPressure(manager,0);
        target(e,60,ch,0); target(e,64,ch,0); target(e,67,ch,0);
    }
}
void conventionalOriginDualManager() {
    for(int policy:{0,1}) {
        begin("conventional-origin channel 16 dual manager",policy,16);
        Engine e; auto p=patch(policy); setup(e,p);
        check(e.getMpeZones().role(1)==MpeZoneLayout::Role::lowerMaster
              && e.getMpeZones().role(16)==MpeZoneLayout::Role::upperMaster,
              "both actual manager roles configured while MPE is effective");
        p[P_mpeOn]=0; e.setParams(p);
        // This note captures the conventional lower-zone identity before MPE
        // becomes effective. Enabling MPE preserves that existing circuit.
        e.noteOn(60,.8f,16); render(e);
        const int original=find(e,60,16);
        p[P_mpeOn]=1; e.setParams(p);
        check(e.getMpeZones().role(1)==MpeZoneLayout::Role::lowerMaster
              && e.getMpeZones().role(16)==MpeZoneLayout::Role::upperMaster,
              "both actual manager roles retained across mode edges");
        check(find(e,60,16)==original && original>=0,"mode edge preserves original circuit");
        e.polyPressure(60,.2f,16); target(e,60,16,.2f);
        // Existing routing accepts BOTH the captured lower-zone manager and
        // own-channel upper manager. Neither may overwrite the local raw key.
        e.channelPressure(16,.8f); target(e,60,16,.8f);
        e.channelPressure(1,.5f); target(e,60,16,.8f);
        e.channelPressure(16,0); target(e,60,16,.5f);
        e.channelPressure(1,0); target(e,60,16,.2f);
        // Contrast a note born as the actual upper manager: manager pressure
        // is a floor, never a retained local component of its own note.
        e.noteOn(64,.8f,16); target(e,64,16,0);
        e.channelPressure(16,.8f); target(e,64,16,.8f);
        e.channelPressure(16,0); target(e,64,16,0);
        target(e,60,16,.2f);
    }
}
void conventional() {
    for(int policy:{0,1}) {
        begin("MPE off",policy,2);Engine e;auto p=patch(policy);p[P_mpeOn]=0;setup(e,p);
        e.channelPressure(1,.8f); e.channelPressure(2,.6f); e.noteOn(60,.8f,2);target(e,60,2,0);
        e.channelPressure(2,.6f);target(e,60,2,.6f);
        e.channelPressure(1,.9f);target(e,60,2,.9f); // Historical non-MPE channel-1 master.
        e.channelPressure(2,.2f);target(e,60,2,.2f);
        e.polyPressure(60,.4f,2);target(e,60,2,.4f);
        e.channelPressure(2,0);target(e,60,2,0);
    }
}
}
int main() {
    liveAndOnset();releasedAndPoly();curvesAndInputs();lifecycle();controllerResets();conventionalOriginDualManager();conventional();
    std::printf("MPE pressure combination: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}

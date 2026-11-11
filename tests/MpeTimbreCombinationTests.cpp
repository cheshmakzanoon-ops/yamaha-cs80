// Product policy: MPE section 2.2.8 / Appendix D permits Max combination;
// it does not mandate it. Combine member and manager CC74 without a curve.
// Keep the existing zero default. Inspect actual core targets, not audio proxies.
#include "core/Engine.h"
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
void cc74(Engine& e, int ch, float value) { e.controlChange(74,value,ch); }
void target(const Engine& e, int note, int ch, float expected) {
    const int i=find(e,note,ch); check(i>=0,"expected active circuit exists");
    if (i<0) return;
    const float actual=e.getVoiceInfo(i).timbre;
    const bool same=std::isfinite(actual) && std::bit_cast<unsigned>(actual)==std::bit_cast<unsigned>(expected);
    if (!same) std::printf("  note=%d ch=%d actual=%g expected=%g\n",note,ch,actual,expected);
    check(same,"exact combined timbre target");
}
void begin(const std::string& name,int policy,int ch) {
    context=std::string(name)+" policy="+std::to_string(policy)+" member="+std::to_string(ch);
}
void liveAndOnset() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool managerFirst:{false,true}) {
        begin(managerFirst?"manager first":"member first",policy,ch);
        Engine e; setup(e,patch(policy)); const int manager=ch==2?1:16, other=ch==2?15:2;
        e.noteOn(60,.8f,ch); e.noteOn(61,.8f,other); e.noteOn(62,.8f,8); e.noteOn(63,.8f,manager);
        if(managerFirst) cc74(e,manager,.7f);
        cc74(e,ch,.4f);
        if(!managerFirst) cc74(e,manager,.7f);
        target(e,60,ch,.7f); target(e,61,other,0); target(e,62,8,0);
        target(e,63,manager,.7f); // Manager's own conventional voice still receives it.
        cc74(e,manager,.2f); target(e,60,ch,.4f);
        cc74(e,manager,0); target(e,60,ch,.4f); target(e,63,manager,0);
        cc74(e,manager,.9f); target(e,60,ch,.9f);
        cc74(e,ch,.1f); target(e,60,ch,.9f);
        cc74(e,manager,0); target(e,60,ch,.1f);
        cc74(e,manager,.6f); e.noteOn(64,.8f,ch); target(e,64,ch,.6f);
        cc74(e,ch,.8f); e.noteOn(65,.8f,ch); target(e,65,ch,.8f);
        render(e); target(e,65,ch,.8f);
    }
    for(int policy:{0,1}) {
        begin("independent simultaneous zones",policy,2); Engine e; setup(e,patch(policy));
        cc74(e,1,.6f); cc74(e,16,.8f);
        cc74(e,2,.7f); cc74(e,15,.2f);
        e.noteOn(60,.8f,2); e.noteOn(61,.8f,15);
        target(e,60,2,.7f); target(e,61,15,.8f);
        cc74(e,1,0); target(e,60,2,.7f); target(e,61,15,.8f);
        cc74(e,16,0); target(e,61,15,.2f);
    }
}
void managerOwnOnset() {
    for(int policy:{0,1}) for(int manager:{1,16}) {
        begin("manager input before own onset",policy,manager);
        Engine e; setup(e,patch(policy));
        cc74(e,manager,.7f); e.noteOn(60,.8f,manager);
        target(e,60,manager,.7f);
        cc74(e,manager,0); target(e,60,manager,0);
        // A manager floor must not also become a retained note-local input.
        render(e); target(e,60,manager,0);
    }
}
void audibleConsumer() {
    for(int policy:{0,1}) for(int ch:{2,15}) {
        begin("matrix and non-silent PCM reference",policy,ch);
        Engine combined, high, low; auto p=patch(policy);
        p[P_mod1Amount]=1; p[P_ribbonCcEnable]=0;
        for(int rank=0;rank<2;++rank) {
            p[chParam(rank,CP_saw)]=0; p[chParam(rank,CP_pulse)]=1;
            p[chParam(rank,CP_pw)]=.2f; p[chParam(rank,CP_pwmDepth)]=0;
            p[chParam(rank,CP_vA)]=p[chParam(rank,CP_fA)]=.001f;
            p[chParam(rank,CP_vS)]=1;
        }
        for(Engine* e:{&combined,&high,&low}) {
            setup(*e,p);
            modern::ModernConfig config;
            config.slots[0]={modern::ModSource::timbre,modern::ModSource::none,
                            modern::ModDestination::pulseWidth,modern::ModTarget::both};
            e->setModernConfig(config);
        }
        const int manager=ch==2?1:16;
        cc74(combined,manager,.7f); cc74(combined,ch,.4f);
        cc74(high,ch,.7f); cc74(low,ch,.4f);
        for(Engine* e:{&combined,&high,&low}) e->noteOn(60,.8f,ch);
        // Identical deterministic histories except for the CC74 sources.
        // Let attack and control smoothing settle before comparing actual PCM.
        for(int block=0;block<32;++block)
            for(Engine* e:{&combined,&high,&low}) render(*e);
        for(Engine* e:{&combined,&high,&low}) {
            const int voice=find(*e,60,ch);
            check(voice>=0,"matrix consumer owns active witness");
            if(voice<0) continue;
            const float expected=(e==&low?.4f:.7f)*modern::kPulseWidthRange;
            for(int rank=0;rank<2;++rank) {
                const float actual=e->getModernRankOffsets(voice,rank).pulseWidth;
                check(std::isfinite(actual) && std::bit_cast<unsigned>(actual)==std::bit_cast<unsigned>(expected),
                      "each actual rank matrix consumes combined timbre");
            }
        }
        std::array<float,1024> a,b,c;
        a.fill(std::numeric_limits<float>::quiet_NaN()); b=a; c=a;
        combined.render(a.data(),512); high.render(b.data(),512); low.render(c.data(),512);
        bool finite=true, equal=true; float peak=0, referencePeak=0, difference=0;
        for(size_t i=0;i<a.size();++i) {
            finite &= std::isfinite(a[i]) && std::isfinite(b[i]) && std::isfinite(c[i]);
            equal &= std::bit_cast<unsigned>(a[i])==std::bit_cast<unsigned>(b[i]);
            peak=std::max(peak,std::abs(a[i])); referencePeak=std::max(referencePeak,std::abs(c[i]));
            difference=std::max(difference,std::abs(a[i]-c[i]));
        }
        check(finite,"all three PCM witnesses written and finite");
        check(peak>1e-6f && referencePeak>1e-6f,"both compared PCM levels are non-silent");
        check(equal,"manager .7 plus member .4 equals member .7 alone by PCM bits");
        check(difference>1e-6f,"combined PCM differs from member .4 alone");
    }
}
void releasedAndReuse() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool pedal:{false,true}) {
        begin(pedal?"pedal held":"release tail",policy,ch); Engine e; setup(e,patch(policy));
        const int manager=ch==2?1:16;
        cc74(e,ch,.6f); cc74(e,manager,.8f);
        e.noteOn(60,.8f,ch); render(e);
        if(pedal) e.controlChange(64,1,ch);
        e.noteOff(60,ch);
        const int i=find(e,60,ch); check(i>=0,"release fixture remains active");
        if(i>=0) {const auto v=e.getVoiceInfo(i);check(!v.held && v.pedalHeld==pedal,"accepted release/pedal fixture");}
        cc74(e,ch,.1f); target(e,60,ch,.8f);
        cc74(e,manager,.3f); target(e,60,ch,.6f);
        cc74(e,manager,0); target(e,60,ch,.6f);
        cc74(e,manager,.9f); target(e,60,ch,.9f);
        e.noteOn(64,.8f,ch); target(e,64,ch,.9f);
        cc74(e,manager,0); target(e,60,ch,.6f); target(e,64,ch,.1f);
        e.noteOn(67,.8f,ch); target(e,67,ch,.1f);
    }
}
void boundedInputs() {
    for(int policy:{0,1}) for(int ch:{2,15}) {
        begin("finite bounded inputs",policy,ch); Engine e; setup(e,patch(policy));
        const int manager=ch==2?1:16;
        cc74(e,ch,.4f); cc74(e,manager,.7f); e.noteOn(60,.8f,ch);
        for(int sender:{ch,manager}) {
            cc74(e,sender,std::numeric_limits<float>::quiet_NaN());
            cc74(e,sender,std::numeric_limits<float>::infinity());
        }
        target(e,60,ch,.7f);
        cc74(e,manager,2); target(e,60,ch,1);
        cc74(e,manager,-1); target(e,60,ch,.4f);
        cc74(e,ch,2); target(e,60,ch,1);
        cc74(e,ch,-1); target(e,60,ch,0);
        e.noteOn(64,.8f,ch); target(e,64,ch,0);
    }
}
void lifecycle() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(int transition=0;transition<4;++transition) {
        begin("lifecycle "+std::to_string(transition),policy,ch);
        Engine e; auto p=patch(policy); setup(e,p);
        const int manager=ch==2?1:16;
        cc74(e,manager,.8f); cc74(e,ch,.4f);
        if(transition==0) e.reset();
        if(transition==1) {p[P_mpeOn]=0;e.setParams(p);p[P_mpeOn]=1;e.setParams(p);}
        if(transition==2) {rpn(e,manager,0);rpn(e,manager,2);}
        if(transition==3) {p[P_parityMode]=1;e.setParams(p);p[P_parityMode]=0;e.setParams(p);}
        e.noteOn(60,.8f,ch); target(e,60,ch,0);
    }
    for(int policy:{0,1}) for(int ch:{2,15}) {
        begin("same manager retained; new member",policy,ch); Engine e; setup(e,patch(policy));
        const int manager=ch==2?1:16, added=ch==2?4:13;
        cc74(e,manager,.8f); rpn(e,manager,3); e.noteOn(60,.8f,added); target(e,60,added,.8f);
        begin("controller inheritance",policy,ch); Engine source,dest;
        setup(source,patch(policy)); setup(dest,patch(policy));
        cc74(source,manager,.8f);cc74(source,ch,.4f);
        dest.inheritControllerStateFrom(source); dest.noteOn(60,.8f,ch);target(dest,60,ch,.8f);
        cc74(dest,manager,0);target(dest,60,ch,.4f);
    }
}
void controllerResets() {
    for(int policy:{0,1}) for(int ch:{2,15}) for(bool released:{false,true}) {
        begin(released?"CC121 released":"CC121 held",policy,ch);
        Engine e; setup(e,patch(policy)); const int manager=ch==2?1:16;
        cc74(e,ch,.6f); cc74(e,manager,.8f);
        e.noteOn(60,.8f,ch); render(e); if(released) e.noteOff(60,ch);
        // Preserve existing reset-all-controllers policy: CC121 does not reset
        // CC74, either the manager floor or member cache / note-local target.
        e.controlChange(121,0,manager); target(e,60,ch,.8f);
        e.controlChange(121,0,ch); target(e,60,ch,.8f);
        e.noteOn(64,.8f,ch); target(e,64,ch,.8f);
        cc74(e,manager,0); target(e,60,ch,.6f); target(e,64,ch,.6f);
        e.noteOn(67,.8f,ch); target(e,67,ch,.6f);
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
        e.noteOn(60,.8f,16); cc74(e,16,.2f); render(e);
        const int original=find(e,60,16);
        p[P_mpeOn]=1; e.setParams(p);
        check(e.getMpeZones().role(1)==MpeZoneLayout::Role::lowerMaster
              && e.getMpeZones().role(16)==MpeZoneLayout::Role::upperMaster,
              "both actual manager roles retained across mode edges");
        check(find(e,60,16)==original && original>=0,"mode edge preserves original circuit");
        target(e,60,16,.2f);
        // Existing routing accepts BOTH the captured lower-zone manager and
        // own-channel upper manager. Neither may overwrite the note-local value.
        cc74(e,16,.8f); target(e,60,16,.8f);
        cc74(e,1,.5f); target(e,60,16,.8f);
        cc74(e,16,0); target(e,60,16,.5f);
        cc74(e,1,0); target(e,60,16,.2f);
        // Contrast a note born as the actual upper manager: manager timbre
        // is a floor, never a retained local component of its own note.
        e.noteOn(64,.8f,16); target(e,64,16,0);
        cc74(e,16,.8f); target(e,64,16,.8f);
        cc74(e,16,0); target(e,64,16,0);
        target(e,60,16,.2f);
    }
}
void conventional() {
    for(int policy:{0,1}) {
        begin("MPE off",policy,2);Engine e;auto p=patch(policy);p[P_mpeOn]=0;setup(e,p);
        cc74(e,1,.8f); cc74(e,2,.6f); e.noteOn(60,.8f,2);target(e,60,2,0);
        cc74(e,2,.6f);target(e,60,2,.6f);
        cc74(e,1,.9f);target(e,60,2,.9f); // Historical non-MPE channel-1 master.
        cc74(e,2,.2f);target(e,60,2,.2f);
        cc74(e,2,0);target(e,60,2,0);
    }
    for(int policy:{0,1}) {
        begin("PURE ignores CC74",policy,2); Engine e; auto p=patch(policy);
        p[P_parityMode]=1; setup(e,p);
        cc74(e,1,.8f); cc74(e,2,.6f); e.noteOn(60,.8f,2); target(e,60,2,0);
        cc74(e,1,.9f); cc74(e,2,.7f); target(e,60,2,0);
        e.controlChange(121,0,1); e.controlChange(121,0,2); target(e,60,2,0);
        render(e); target(e,60,2,0);
    }
}
}
int main() {
    liveAndOnset();managerOwnOnset();audibleConsumer();releasedAndReuse();boundedInputs();lifecycle();controllerResets();conventionalOriginDualManager();conventional();
    std::printf("MPE timbre combination: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}

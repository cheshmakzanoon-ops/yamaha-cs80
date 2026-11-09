// Supplemental held-input witness; no physical-circuit or whole-state claim.
#include "core/Engine.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
using namespace cs80;
namespace {
int checks=0, failures=0;
void check(bool v,const char* name) { ++checks; if(!v) { if(failures<20) std::printf("FAIL: %s\n",name); ++failures; } }
bool exact(float a,float b) { return std::isfinite(a)&&std::isfinite(b)&&std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b); }
void tick(Engine& e) { float out[2]={NAN,NAN}; e.render(out,1); check(std::isfinite(out[0])&&std::isfinite(out[1]),"render writes finite stereo"); }
void sameSub(const Engine& a,const Engine& b) { check(exact(a.getSubOscillatorRate(),b.getSubOscillatorRate()),"SUB rate bits match"); check(exact(a.getSubOscillatorValue(),b.getSubOscillatorValue()),"SUB output bits match"); }
void configure(Engine& e,int policy,int original,double rate) {
    ParamValues p; p[P_sourcePerformance]=static_cast<float>(policy); p[P_mpeOn]=1;
    p[P_parityMode]=p[P_analogNoise]=p[P_voiceDispersion]=p[P_drift]=p[P_detune]=0;
    p[P_pressureCurve]=p[P_pressureThreshold]=p[P_initBend]=p[P_chorusOn]=p[P_arpOn]=p[P_kasScanTiming]=0;
    p[P_subOriginalMode]=static_cast<float>(original); p[P_subSpeed]=7; p[P_subAtSpeed]=1;
    p[P_keyRelease]=.5f; e.prepare(rate); e.setParams(p);
}
}
int main() {
    int scenarios=0;
    for(int policy:{0,1}) for(int original:{0,1}) for(bool pedal:{false,true})
    for(double rate:{44100.,48000.,96000.,192000.}) {
        ++scenarios; Engine retained,replay,neutral;
        configure(retained,policy,original,rate); configure(replay,policy,original,rate); configure(neutral,policy,original,rate);
        retained.channelPressure(2,.8f);
        for(int n=0;n<128;++n) { tick(retained);tick(replay);tick(neutral);sameSub(retained,replay);sameSub(retained,neutral); }
        check(retained.activeVoices()==0,"cached pressure alone owns no active voice");
        retained.noteOn(60,.8f,2); replay.noteOn(60,.8f,2);neutral.noteOn(60,.8f,2);replay.channelPressure(2,.8f);
        check(retained.getVoiceInfo(0).held&&retained.getVoiceInfo(0).pressure==.8f,"accepted held onset inherits pressure");
        bool positive=false;
        for(int n=0;n<static_cast<int>(rate/10);++n) { tick(retained);tick(replay);tick(neutral);sameSub(retained,replay); positive|=retained.getSubOscillatorRate()>neutral.getSubOscillatorRate()+.001f; }
        check(positive,"held retained pressure actually increases shared SUB speed");
        if(pedal) { retained.controlChange(64,1.0f,2);replay.controlChange(64,1.0f,2); }
        retained.noteOff(60,2);replay.noteOff(60,2);
        check(!retained.getVoiceInfo(0).held,"released onset no longer supplies held force");
        check(!pedal||retained.getVoiceInfo(0).pedalHeld,"pedal case retains sound without a held gate");
        // Existing Circuit TWS state decays on both sides. Clear one voice's
        // target and send a stronger member event to the other: neither is
        // allowed to inject fresh force after key release.
        replay.channelPressure(2,0);retained.channelPressure(2,1);
        for(int n=0;n<static_cast<int>(rate/40);++n) { tick(retained);tick(replay);sameSub(retained,replay); }
    }
    std::printf("MPE shared pressure input: %d scenarios, %d checks, %d failures\n",scenarios,checks,failures);
    return failures?1:0;
}

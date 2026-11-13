#include "core/Engine.h"
#include <cmath>
#include <cstdio>
#include <memory>

namespace {
constexpr int rate=48000, followFrames=12000;
int checks=0,failures=0;
void check(bool ok,const char* label,int scenario)
{++checks;if(!ok)++failures;std::printf("%s case=%d %s\n",ok?"PASS":"FAIL",scenario,label);}
struct Observation {int note=-1,count=0;bool pedalHeld=false;};
Observation observe(const cs80::Engine& e)
{
    Observation o;
    for(int v=0;v<8;++v){auto i=e.getVoiceInfo(v);o.pedalHeld|=i.pedalHeld;
        if(i.active&&i.held){o.note=i.note;++o.count;}}
    return o;
}
bool tick(cs80::Engine& e)
{float out[2]{};e.render(out,1);return std::isfinite(out[0])&&std::isfinite(out[1]);}
void run(int scenario)
{
    auto e=std::make_unique<cs80::Engine>();cs80::ParamValues p;
    p[cs80::P_parityMode]=0;p[cs80::P_sourcePerformance]=0;
    p[cs80::P_kasScanTiming]=0;p[cs80::P_oversampling]=0;
    p[cs80::P_voiceDispersion]=p[cs80::P_drift]=p[cs80::P_analogNoise]=0;
    p[cs80::P_arpOn]=1;p[cs80::P_arpMode]=0;p[cs80::P_arpOctaves]=2;p[cs80::P_arpRate]=16;
    e->prepare(rate);e->setParams(p);e->controlChange(64,1.0f,1);
    e->noteOn(60,.7f,1);e->noteOn(64,.7f,1);
    bool finite=true,pedalClear=true,single=true;
    const int editFrame=scenario==1?6500:1000;
    for(int i=0;i<editFrame;++i){finite=tick(*e)&&finite;auto o=observe(*e);pedalClear&=!o.pedalHeld;single&=o.count<=1;}
    auto before=observe(*e);
    check(e->getOversamplingFactor()==2&&e->getInternalSampleRate()==96000,"fixed Legacy internal rate",scenario);
    check(before.count==1&&before.note==(scenario==1?72:60),"positive current generated-note witness",scenario);
    // Cases 0/1 remove the source of current root/octave. Case 2 removes
    // the other source while root 60 is still gated.
    const int removed=scenario==2?64:60,remaining=scenario==2?60:64;
    e->noteOff(removed,1);
    auto immediate=observe(*e);
    check(scenario==2?(immediate.count==1&&immediate.note==60):immediate.count==0,
          "current-source release stops gate; other-source release preserves it",scenario);
    bool allowed=true,sawRoot=false,sawOctave=false;
    for(int i=0;i<followFrames;++i){finite=tick(*e)&&finite;auto o=observe(*e);
        pedalClear&=!o.pedalHeld;single&=o.count<=1;
        if(o.count){allowed&=o.note==remaining||o.note==remaining+12;sawRoot|=o.note==remaining;sawOctave|=o.note==remaining+12;}}
    check(allowed&&sawRoot&&sawOctave,"remaining source continues both octaves without released source",scenario);
    e->noteOff(remaining,1);
    bool stopped=observe(*e).count==0;
    for(int i=0;i<followFrames;++i){finite=tick(*e)&&finite;auto o=observe(*e);stopped&=o.count==0;pedalClear&=!o.pedalHeld;}
    check(stopped,"final source release stops pattern despite pedal down",scenario);
    e->controlChange(64,0.0f,1);
    bool noRevival=true;
    for(int i=0;i<followFrames;++i){finite=tick(*e)&&finite;auto o=observe(*e);noRevival&=o.count==0;pedalClear&=!o.pedalHeld;}
    check(noRevival,"pedal-up cannot revive released ARP sources",scenario);
    check(pedalClear,"generated gates never become pedalHeld tails",scenario);
    check(finite&&single,"finite audio and single generated held gate",scenario);
}
}
int main(){for(int i=0;i<3;++i)run(i);std::printf("RESULT checks=%d failures=%d\n",checks,failures);return failures?1:0;}

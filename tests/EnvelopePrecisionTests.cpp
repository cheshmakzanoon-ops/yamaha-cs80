// Numerical accuracy of the existing inferred envelope law, not hardware calibration.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
using namespace cs80;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok){++failures;std::cerr<<text<<'\n';}}
void primitives(float rate){
    Adsr attack,decay,release; FilterEnv filterAttack,filterRelease;
    attack.noteOn(20,20,.5,20);filterAttack.noteOn(-3,3,20,20,20,0);
    decay.noteOn(.001,20,.5,20);release.noteOn(.001,.001,1,20);
    filterRelease.noteOn(-3,3,.001,.001,20,0);
    for(int i=0;i<int(rate*.02f);++i){decay.tick(rate,false,true);release.tick(rate,false,true);filterRelease.tick(rate,false,true);}
    const double decayStart=decay.getValue();release.noteOff();filterRelease.noteOff();
    auto repeated=decay;
    for(int i=1;i<=static_cast<int>(rate*40);++i){
        attack.tick(rate,false,true);filterAttack.tick(rate,false,true);
        decay.tick(rate,false,true);release.tick(rate,false,true);filterRelease.tick(rate,false,true);
        // Repeated host state transactions must not discard sub-float progress.
        if(i%127==0){repeated.setSustain(.5);repeated.setTimes(.001,20,20);}
        repeated.tick(rate,false,true);
        if(i%4096==0)check(repeated.getValue()==decay.getValue(),"repeated setters preserve sub-float trajectory");
        if(i==static_cast<int>(rate*10)){
            const double c=std::exp(-3.0);
            check(std::abs(attack.getValue()-(1-c))<1e-7,"long Attack analytic law");
            check(std::abs(filterAttack.getValue()-(3-6*c))<3e-7,"filter Attack analytic law");
            check(std::abs(decay.getValue()-(.5+(decayStart-.5)*c))<1e-7,"nonzero Decay analytic law");
            check(std::abs(release.getValue()-c)<1e-7,"Release analytic law");
            check(std::abs(filterRelease.getValue()-(-3+3*c))<3e-7,"nonzero filter Release analytic law");
        }
        if(i==static_cast<int>(rate*20)){
            check(std::abs(attack.getState().elapsed-20)<2e-6,"20s clock remains accurate at max supported rate");
            check(std::abs(filterAttack.getState().elapsed-20)<2e-6,"filter clock does not saturate16s");
        }
    }
    check(attack.getState().stage!=Adsr::Stage::Attack,"untouched Attack terminates");
    check(filterAttack.getState().stage!=FilterEnv::Stage::Attack,"untouched filter Attack terminates");
    check(decay.getState().stage==Adsr::Stage::Sustain&&decay.getValue()==.5f,"nonzero Decay reaches endpoint");
    check(release.isIdle()&&release.getValue()==0,"Release reaches endpoint");
    check(filterRelease.isIdle()&&filterRelease.getValue()==-3,"nonzero filter Release reaches endpoint");
    check(repeated.getValue()==decay.getValue()&&repeated.getState().elapsed==decay.getState().elapsed,"repeated setters preserve accumulated trajectory");
    // Reset/new onset clears numerical residuals and both clocks.
    attack.reset();filterAttack.reset();Adsr clean;FilterEnv cleanF;
    attack.noteOn(.1,.2,.4,.3);clean.noteOn(.1,.2,.4,.3);
    filterAttack.noteOn(-1,2,.1,.2,.3,0);cleanF.noteOn(-1,2,.1,.2,.3,0);
    for(int i=0;i<1000;++i)check(attack.tick(rate,true,true)==clean.tick(rate,true,true)&&filterAttack.tick(rate,true,true)==cleanF.tick(rate,true,true),"reset clears precise state");
}
void integration(){
    auto p=defaultValues();p[P_parityMode]=1;p[P_masterSustain]=0;
    for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=p[chParam(ch,CP_fA)]=20;p[chParam(ch,CP_vD)]=p[chParam(ch,CP_fD)]=20;p[chParam(ch,CP_vS)]=.5;p[chParam(ch,CP_fIl)]=p[chParam(ch,CP_fAl)]=1;}
    Engine e;e.setParams(p);e.prepare(768000);e.noteOn(60,.8);
    std::array<float,1536> out{};
    for(int i=0;i<25000;++i){e.render(out.data(),768);if(i==19999)for(int ch=0;ch<2;++ch){auto s=e.getRankEnvelopeState(0,ch);check(std::abs(s.amplitude.elapsed-20)<2e-6&&std::abs(s.filter.elapsed-20)<2e-6,"both Engine rank clocks pass16s");}}
    for(int ch=0;ch<2;++ch){auto s=e.getRankEnvelopeState(0,ch);check(s.amplitude.stage==Adsr::Stage::Decay&&s.filter.stage==FilterEnv::Stage::Decay,"both Engine ranks complete long Attack");}
    e.reset();check(e.activeVoices()==0,"Engine reset clears precise voices");
    p[P_parityMode]=0;e.setParams(p);e.noteOn(60,.8);e.render(out.data(),1);p[P_parityMode]=1;e.setParams(p);check(e.activeVoices()==0,"mode switch resets numerical path lifetime");
}
}
int main(){for(float rate:{96000.f,384000.f,1536000.f})primitives(rate);integration();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

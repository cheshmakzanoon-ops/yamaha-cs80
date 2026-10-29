// State-continuous live target selection is an explicit engineering inference,
// not a measurement of IG00151/152/159 sequence or current laws.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks=0, failures=0;
void check(bool ok,const char* message){++checks;if(!ok){if(failures<30)std::cerr<<message<<'\n';++failures;}}
template<class S>bool same(S a,S b){return a.stage==b.stage&&a.value==b.value&&a.elapsed==b.elapsed;}
bool same(Engine::RankEnvelopeState a,Engine::RankEnvelopeState b){return same(a.amplitude,b.amplitude)&&same(a.filter,b.filter);}
bool same(Engine::RankAudioState a,Engine::RankAudioState b){return a.phase==b.phase&&a.hpf.ic1==b.hpf.ic1&&a.hpf.ic2==b.hpf.ic2&&a.lpf.ic1==b.lpf.ic1&&a.lpf.ic2==b.lpf.ic2;}
void near(float a,double b,const char* message){check(std::abs(a-b)<2e-6,message);}
std::vector<float> render(Engine& e,int frames,int block=0,const ParamValues* repeated=nullptr){
    std::vector<float> out(static_cast<size_t>(frames)*2);if(block==0)block=std::max(1,frames);
    for(int n=0;n<frames;n+=block){if(repeated)e.setParams(*repeated);e.render(out.data()+n*2,std::min(block,frames-n));}return out;
}
void fixture(Adsr& a,FilterEnv& f,int phase,float rate){
    a.noteOn(phase==0?1.f:.001f,phase==2?.001f:2.f,.3f,2.f);
    f.noteOn(-.6f,1.5f,phase==0?1.f:.001f,phase==2?.001f:2.f,2.f,0);
    if(phase==4){a.reset();f.reset();return;}
    for(int n=0;n<static_cast<int>(rate*.08f);++n){a.tick(rate);f.tick(rate);}
    if(phase==3){a.noteOff();f.noteOff();for(int n=0;n<static_cast<int>(rate*.06f);++n){a.tick(rate);f.tick(rate);}}
    check(static_cast<int>(a.getState().stage)==phase+1,"amplitude phase fixture");
    check(static_cast<int>(f.getState().stage)==phase+1,"filter phase fixture");
}
void primitive(float rate){
    for(int phase=0;phase<5;++phase){
        Adsr a;FilterEnv f;fixture(a,f,phase,rate);
        if(phase==2)a.setTimes(.001f,2,2);
            for(int step=0;step<12;++step){
            const float s=step%2?.05f:.95f,il=step%2?-2.8f:0.f,al=step%2?0.f:2.9f;
            auto beforeA=a.getState();auto beforeF=f.getState();auto uneditedA=a;auto uneditedF=f;
            a.setSustain(s);f.setLevels(il,al);
            check(same(beforeA,a.getState())&&same(beforeF,f.getState()),"target setter preserves exact stage/value/time");
            auto onceA=a;auto onceF=f;a.setSustain(s);f.setLevels(il,al);
            const float nextA=a.tick(rate),nextF=f.tick(rate);
            check(nextA==onceA.tick(rate)&&nextF==onceF.tick(rate),"same-target setters preserve accumulator progress");
            if(phase==0||phase==3||phase==4)check(nextA==uneditedA.tick(rate),"inactive amplitude target cannot alter current selection");
            if(phase==1||phase==2||phase==4)check(nextF==uneditedF.tick(rate),"inactive filter targets cannot alter steady/idle selection");
            const double c=std::exp(-6.0/(2.0*rate));
            if(step==0&&(phase==1||phase==2))near(nextA,s+(beforeA.value-s)*c,"decay/sustain uses new target and existing decay law");
            if(step==0&&phase==0)near(nextF,al+(beforeF.value-al)*std::exp(-6.0/rate),"attack uses new AL from retained value");
            if(step==0&&phase==3)near(nextF,beforeF.value+(il-beforeF.value)*(1-c),"release uses new IL from retained value");
            for(int n=0;n<128;++n){
                const float prevA=a.getValue(),prevF=f.getValue();a.tick(rate);f.tick(rate);
                check(std::isfinite(a.getValue())&&std::isfinite(f.getValue()),"rapid retarget remains finite");
                check(a.getValue()>=0&&a.getValue()<=1&&f.getValue()>=-3&&f.getValue()<=3,"rapid retarget remains bounded");
                if(phase==1||phase==2)check(std::abs(a.getValue()-s)<=std::abs(prevA-s)+1e-7f,"S tracking is monotone toward current target");
                if(phase==0&&f.getState().stage==FilterEnv::Stage::Attack)check(std::abs(f.getValue()-al)<=std::abs(prevF-al)+1e-7f,"AL tracking monotone including target crossing");
                if(phase==3&&f.getState().stage==FilterEnv::Stage::Release)check(std::abs(f.getValue()-il)<=std::abs(prevF-il)+1e-7f,"IL tracking monotone including target crossing");
            }
        }
        // noteOn and reset clear all live tracking/residual state.
        Adsr cleanA;FilterEnv cleanF;a.reset();f.reset();
        a.noteOn(.1f,.2f,.4f,.3f);cleanA.noteOn(.1f,.2f,.4f,.3f);
        f.noteOn(-1,2,.1f,.2f,.3f,0);cleanF.noteOn(-1,2,.1f,.2f,.3f,0);
        for(int n=0;n<2000;++n)check(a.tick(rate)==cleanA.tick(rate)&&f.tick(rate)==cleanF.tick(rate),"reset/noteOn remove residual live history");
    }
    // IL set while attack is unselected must become the release target later.
    Adsr a;FilterEnv f;fixture(a,f,0,rate);f.setLevels(-2.f,1.5f);f.noteOff();const float previous=f.getValue();
    check(f.tick(rate)<previous,"stored IL is selected on noteOff without retrigger");
    // A completed filter release remains selected even while Stage is Idle.
    FilterEnv tail;tail.noteOn(-.6f,1.5f,.001f,.001f,.001f,0);
    for(int n=0;n<static_cast<int>(rate*.1f);++n)tail.tick(rate);
    tail.noteOff();for(int n=0;n<static_cast<int>(rate*.1f);++n)tail.tick(rate);
    check(tail.isIdle(),"completed-release fixture");auto idle=tail.getState();
    tail.setTimes(1,1,1);tail.setLevels(-2,2);
    check(same(idle,tail.getState()),"completed release IL setter retains exact idle state");
    const float first=tail.tick(rate);check(tail.isIdle()&&first<idle.value&&first>-2,"completed release tracks IL without reactivation");
    tail.setLevels(0,0);const float second=tail.tick(rate);
    check(tail.isIdle()&&second>first&&second<0,"completed release tracks reversed IL continuously");
    tail.reset();tail.setLevels(-2.5f,2.5f);
    for(int n=0;n<100;++n)check(tail.tick(rate)==0&&tail.isIdle(),"reset alone clears completed release selection");
    FilterEnv freshF;
    tail.noteOn(-1,1,.1f,.2f,.3f,0);freshF.noteOn(-1,1,.1f,.2f,.3f,0);
    tail.setLevels(-2,2);for(int n=0;n<100;++n)tail.tick(rate);
    tail.noteOn(-1,1,.1f,.2f,.3f,0); // No reset: clear prior live attack state.
    for(int n=0;n<1000;++n)check(tail.tick(rate)==freshF.tick(rate),"filter noteOn alone clears live target history");
    Adsr reused,freshA;reused.noteOn(.001f,.001f,.3f,.001f);
    for(int n=0;n<static_cast<int>(rate*.1f);++n)reused.tick(rate);
    reused.setSustain(.9f);reused.tick(rate);reused.noteOff();
    for(int n=0;n<static_cast<int>(rate*.1f);++n)reused.tick(rate);
    check(reused.isIdle()&&reused.getValue()==0,"amp reuse starts with matched idle state");
    reused.noteOn(.001f,.001f,.4f,.2f);freshA.noteOn(.001f,.001f,.4f,.2f);
    for(int n=0;n<static_cast<int>(rate*.02f);++n)check(reused.tick(rate)==freshA.tick(rate),"amplitude noteOn alone clears live sustain history");
    // No new time law for completely unchanged controls.
    Adsr plainA,repeatA;FilterEnv plainF,repeatF;fixture(plainA,plainF,0,rate);repeatA=plainA;repeatF=plainF;
    for(int n=0;n<2000;++n){repeatA.setSustain(.3f);repeatF.setLevels(-.6f,1.5f);check(plainA.tick(rate)==repeatA.tick(rate)&&plainF.tick(rate)==repeatF.tick(rate),"unchanged targets retain original arithmetic");}
}
ParamValues settings(bool isolation,int phase){
    auto p=defaultValues();p[P_parityMode]=isolation?1.f:0.f;p[P_masterSustain]=p[P_keyRelease]=0;p[P_sustainMode]=p[P_sustainAssign]=0;
    for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=p[chParam(ch,CP_fA)]=phase==0?1.f:.001f;p[chParam(ch,CP_vD)]=p[chParam(ch,CP_fD)]=phase==2?.001f:2.f;p[chParam(ch,CP_vS)]=.3f;p[chParam(ch,CP_fIl)]=.2f;p[chParam(ch,CP_fAl)]=.5f;p[chParam(ch,CP_vR)]=p[chParam(ch,CP_fR)]=2.f;}return p;
}
void engine(double rate){
    for(bool isolated:{false,true})for(int phase=0;phase<4;++phase){
        auto p=settings(isolated,phase);Engine edited,control,partition;
        for(auto* e:{&edited,&control,&partition}){e->setParams(p);e->prepare(rate);e->noteOn(60,.8f);render(*e,static_cast<int>(rate*.08));if(phase==3){e->noteOff(60);render(*e,static_cast<int>(rate*.06));}}
        for(int step=0;step<4;++step){
            std::array<Engine::RankEnvelopeState,2> before;std::array<Engine::RankAudioState,2> audio;
            for(int ch=0;ch<2;++ch){before[ch]=edited.getRankEnvelopeState(0,ch);audio[ch]=edited.getRankAudioState(0,ch);p[chParam(ch,CP_vS)]=step%2?.1f:.9f-ch*.1f;p[chParam(ch,CP_fIl)]=step%2?.1f:.9f-ch*.1f;p[chParam(ch,CP_fAl)]=step%2?.1f:.9f-ch*.1f;}
            edited.setParams(p);partition.setParams(p);
            for(int ch=0;ch<2;++ch){check(same(before[ch],edited.getRankEnvelopeState(0,ch)),"both ranks retain exact envelope state on preset-like target transaction");check(same(audio[ch],edited.getRankAudioState(0,ch)),"targets do not replace oscillator/filter state");}
            check(edited.getVoiceInfo(0).note==60&&edited.getVoiceInfo(0).held==(phase!=3),"target transaction preserves voice identity/gate");
            auto out=render(edited,257),split=render(partition,257,17,&p),base=render(control,257);
            check(out==split,"target trajectories are bit-identical across block partition/repeated setters");
            for(int ch=0;ch<2;++ch){auto e=edited.getRankEnvelopeState(0,ch),c=control.getRankEnvelopeState(0,ch);check(same(e,partition.getRankEnvelopeState(0,ch)),"partition exact state identity");
                if(!isolated)check(same(e,c),"legacy target automation remains onset-latched");
                else if(step==0){if(phase==1||phase==2)check(e.amplitude.value>c.amplitude.value,"both ranks audibly use live S");if(phase==0)check(e.filter.value>c.filter.value,"both ranks use live AL");if(phase==3)check(e.filter.value<c.filter.value,"both ranks use live IL");}}
            if(!isolated)check(out==base,"legacy target automation remains raw-audio identical");
            else if(step==0)check(out!=base,"isolation target transaction changes audio");
            for(float x:out)check(std::isfinite(x),"live target audio finite");
        }
        edited.allNotesOff(false);edited.setParams(p);render(edited,32);check(edited.activeVoices()==0,"target setters do not resurrect idle notes");
    }
}
void completedFilterTail(double rate){
    auto p=settings(true,2);
    for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vR)]=5;p[chParam(ch,CP_fR)]=.001f;}
    Engine e;e.setParams(p);e.prepare(rate);e.noteOn(60,.8f);render(e,static_cast<int>(rate*.1));
    e.noteOff(60);render(e,static_cast<int>(rate*.1));
    for(int ch=0;ch<2;++ch){auto state=e.getRankEnvelopeState(0,ch);check(state.filter.stage==FilterEnv::Stage::Idle&&state.amplitude.stage==Adsr::Stage::Release,"filter finishes before audible VCA tail");p[chParam(ch,CP_fIl)]=.8f;}
    auto before=e.getRankEnvelopeState(0,0);e.setParams(p);check(same(before,e.getRankEnvelopeState(0,0)),"completed filter tail target transaction continuous");render(e,1);
    for(int ch=0;ch<2;++ch){auto state=e.getRankEnvelopeState(0,ch);check(state.filter.stage==FilterEnv::Stage::Idle&&state.filter.value<before.filter.value,"both completed filter ranks track IL under VCA tail");}
    check(e.getVoiceInfo(0).active&&!e.getVoiceInfo(0).held,"completed filter tracking preserves released audible voice");
}
void extremePrecision(){
    constexpr float rate=1536000, seconds=42.87094f;
    FilterEnv f;f.noteOn(-3,0,seconds,seconds,seconds,0);
    // Retarget late after shortening A: the old absolute timeout is already due.
    for(int n=0;n<10000;++n)f.tick(rate);
    f.setTimes(.001f,seconds,seconds);const auto before=f.getState();f.setLevels(-3,3);
    check(same(before,f.getState()),"late AL update preserves state");f.tick(rate);
    check(f.getState().stage==FilterEnv::Stage::Attack&&f.getValue()<0,"old attack timeout cannot snap to moved AL");
    f.setTimes(seconds,seconds,seconds);
    bool bounded=true;
    for(int n=0;n<static_cast<int>(rate*seconds*1.6f)&&f.getState().stage==FilterEnv::Stage::Attack;++n){float old=f.getValue();f.tick(rate,false);bounded&=f.getValue()>=old&&f.getValue()<=3;}
    check(bounded&&f.getState().stage==FilterEnv::Stage::Decay,"slow high-rate live AL converges without float stall");
    f.noteOff();f.setLevels(-2.5f,3);
    for(int n=0;n<static_cast<int>(rate*seconds*1.6f)&&!f.isIdle();++n)f.tick(rate,false);
    check(f.isIdle()&&f.getValue()==-2.5f,"slow high-rate live IL converges without float stall");
    Adsr a;a.noteOn(.001f,.001f,.1f,seconds);for(int n=0;n<40000;++n)a.tick(rate);a.setTimes(seconds,seconds,seconds);a.setSustain(.9f);
    for(int n=0;n<static_cast<int>(rate*seconds*1.6f);++n)a.tick(rate,false);
    check(a.getState().stage==Adsr::Stage::Sustain&&a.getValue()==.9f,"slow high-rate held S converges without float stall");
    a.reset();a.noteOn(.001f,seconds,.1f,seconds);for(int n=0;n<40000;++n)a.tick(rate);a.setSustain(.7f);
    for(int n=0;n<static_cast<int>(rate*seconds*1.6f)&&a.getState().stage==Adsr::Stage::Decay;++n)a.tick(rate,false);
    check(a.getState().stage==Adsr::Stage::Sustain&&a.getValue()==.7f,"slow high-rate live decay converges without float stall");
}
}
int main(){for(double rate:{44100.,48000.,96000.,192000.}){primitive(static_cast<float>(rate*2));engine(rate);completedFilterTail(rate);}extremePrecision();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

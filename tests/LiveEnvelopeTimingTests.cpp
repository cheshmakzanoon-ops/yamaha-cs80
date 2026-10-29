// Live timing connectivity under the existing provisional envelope law.
// These checks do not establish calibrated IC timing or level-target dynamics.
#include "core/Engine.h"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok){if(failures<30)std::cerr<<text<<'\n';++failures;}}
void near(float a,float b,const char* text,float tolerance=2e-6f){check(std::abs(a-b)<=tolerance,text);}
template<class S>bool equal(S a,S b){return a.stage==b.stage&&a.value==b.value&&a.elapsed==b.elapsed;}
bool equal(Engine::RankEnvelopeState a,Engine::RankEnvelopeState b){return equal(a.amplitude,b.amplitude)&&equal(a.filter,b.filter);}
bool equal(Engine::RankAudioState a,Engine::RankAudioState b){return a.phase==b.phase&&a.hpf.ic1==b.hpf.ic1&&a.hpf.ic2==b.hpf.ic2&&a.lpf.ic1==b.lpf.ic1&&a.lpf.ic2==b.lpf.ic2;}
std::vector<float> render(Engine& e,int frames,int block=0,const ParamValues* repeated=nullptr){
    std::vector<float> output(static_cast<size_t>(frames)*2);
    if(block<=0)block=std::max(1,frames);
    for(int offset=0;offset<frames;offset+=block){if(repeated)e.setParams(*repeated);e.render(output.data()+offset*2,std::min(block,frames-offset));}
    return output;
}
ParamValues settings(bool isolation=true){
    auto p=defaultValues();p[P_parityMode]=isolation?1.f:0.f;p[P_masterSustain]=0;p[P_keyRelease]=0;
    p[P_sustainMode]=0;p[P_sustainAssign]=0;
    for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=1;p[chParam(ch,CP_fA)]=1;p[chParam(ch,CP_vD)]=2;p[chParam(ch,CP_fD)]=2;p[chParam(ch,CP_vR)]=2;p[chParam(ch,CP_fR)]=2;p[chParam(ch,CP_vS)]=.25f;p[chParam(ch,CP_fIl)]=0;p[chParam(ch,CP_fAl)]=.8f;}
    return p;
}
void init(Engine& e,double rate,const ParamValues& p){e.setParams(p);e.prepare(rate);}
void primitiveTests(float rate){
    for(int phase=0;phase<3;++phase){
        Adsr amp;FilterEnv filter;
        const float attack=phase==0?1.f:.001f;
        amp.noteOn(attack,4,.25f,4);filter.noteOn(-.7f,1.2f,attack,4,4,.1f);
        if(phase==0){for(int n=0;n<static_cast<int>(rate*.01f);++n){amp.tick(rate);filter.tick(rate);}}
        else {
            while(amp.getState().stage==Adsr::Stage::Attack)amp.tick(rate);
            while(filter.getState().stage==FilterEnv::Stage::Attack)filter.tick(rate);
            if(phase==2){amp.noteOff();filter.noteOff();}
            for(int n=0;n<static_cast<int>(rate*.06f);++n){amp.tick(rate);filter.tick(rate);}
        }
        auto a=amp.getState();auto f=filter.getState();
        check(a.stage==(phase==0?Adsr::Stage::Attack:phase==1?Adsr::Stage::Decay:Adsr::Stage::Release),"amplitude stage fixture");
        check(f.stage==(phase==0?FilterEnv::Stage::Attack:phase==1?FilterEnv::Stage::Decay:FilterEnv::Stage::Release),"filter stage fixture");
        amp.setTimes(.5f,1.5f,1.2f);filter.setTimes(.5f,1.5f,1.2f);
        check(equal(a,amp.getState()),"amplitude timing update preserves exact state");check(equal(f,filter.getState()),"filter timing update preserves exact state");
        const float time=phase==0?.5f:phase==1?1.5f:1.2f;
        // Independently express the existing, deliberately provisional, coefficient.
        const float c=static_cast<float>(std::exp(-6.0/(static_cast<double>(time)*rate)));
        float wantedAmp=phase==0?1.f+(a.value-1.f)*c:phase==1?.25f+(a.value-.25f)*c:a.value*c;
        float wantedFilter=phase==0?1.2f+(f.value-1.2f)*c:phase==1?.1f+(f.value-.1f)*c:f.value+(-.7f-f.value)*(1.f-c);
        near(amp.tick(rate),wantedAmp,"next amplitude tick uses new time without fast-window restart");near(filter.tick(rate),wantedFilter,"next filter tick uses new time without fast-window restart");
        check(amp.getState().elapsed>a.elapsed&&filter.getState().elapsed>f.elapsed,"elapsed stage time continues");
    }
    Adsr held;FilterEnv settled;held.noteOn(.001f,.001f,.4f,.5f);settled.noteOn(-.2f,.8f,.001f,.001f,.5f,.1f);
    for(int n=0;n<static_cast<int>(rate*.1f);++n){held.tick(rate);settled.tick(rate);}
    check(held.getState().stage==Adsr::Stage::Sustain&&settled.getState().stage==FilterEnv::Stage::Sustain,"sustain fixture");
    auto a=held.getState();auto f=settled.getState();held.setTimes(2,2,2);settled.setTimes(2,2,2);
    check(equal(a,held.getState())&&equal(f,settled.getState()),"sustain timing refresh changes no state");
    near(held.tick(rate),.4f,"sustain amplitude target unchanged");near(settled.tick(rate),.1f,"filter steady target unchanged");
}
void engineTimingTests(double rate){
    for(bool isolated:{false,true})for(int phase=0;phase<3;++phase){
        Engine edited,control;auto p=settings(isolated);
        if(phase!=0)for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=.001f;p[chParam(ch,CP_fA)]=.001f;}
        init(edited,rate,p);init(control,rate,p);edited.noteOn(60,.8f);control.noteOn(60,.8f);
        render(edited,static_cast<int>(rate*.01));render(control,static_cast<int>(rate*.01));
        if(phase==2){edited.noteOff(60);control.noteOff(60);}
        render(edited,static_cast<int>(rate*.06));render(control,static_cast<int>(rate*.06));
        std::array<Engine::RankEnvelopeState,2> before;std::array<Engine::RankAudioState,2> audio;
        for(int ch=0;ch<2;++ch){before[ch]=edited.getRankEnvelopeState(0,ch);audio[ch]=edited.getRankAudioState(0,ch);const int local=phase==0?CP_vA:phase==1?CP_vD:CP_vR;const int flocal=phase==0?CP_fA:phase==1?CP_fD:CP_fR;p[chParam(ch,local)]=.2f;p[chParam(ch,flocal)]=.2f;}
        edited.setParams(p);
        for(int ch=0;ch<2;++ch){check(equal(before[ch],edited.getRankEnvelopeState(0,ch)),"setParams preserves envelope state boundary");check(equal(audio[ch],edited.getRankAudioState(0,ch)),"setParams preserves oscillator and filter memory");}
        check(edited.getVoiceInfo(0).note==60&&edited.getVoiceInfo(0).active&&edited.getVoiceInfo(0).held==(phase!=2),"timing edits preserve key identity and gate");
        auto out=render(edited,static_cast<int>(rate*.02));auto reference=render(control,static_cast<int>(rate*.02));
        for(int ch=0;ch<2;++ch){auto e=edited.getRankEnvelopeState(0,ch),c=control.getRankEnvelopeState(0,ch);
            if(isolated){check(phase==0?e.amplitude.value>c.amplitude.value:e.amplitude.value<c.amplitude.value,"live amplitude timing affects current stage");check(phase==0?e.filter.value>c.filter.value:e.filter.value<c.filter.value,"live filter timing affects current stage");}
            else check(equal(e,c),"legacy dynamic timing remains latched");}
        if(!isolated)check(out==reference,"legacy timing automation audio remains unchanged");
        for(float sample:out)check(std::isfinite(sample),"live timing render finite");
    }
}
void gateTests(double rate){
    Engine e;auto p=settings();p[P_masterSustain]=.8f;p[P_sustainAssign]=1;p[P_footswitchPlugged]=1;p[P_footswitchPressed]=1;
    for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=.001f;p[chParam(ch,CP_fA)]=.001f;p[chParam(ch,CP_vR)]=1;p[chParam(ch,CP_fR)]=1;}
    init(e,rate,p);e.noteOn(60,.8f);render(e,static_cast<int>(rate*.01));e.noteOff(60);render(e,static_cast<int>(rate*.06));
    for(bool open:{false,false,true,false}){
        p[P_footswitchPressed]=open?1.f:0.f;auto previous=e.getRankEnvelopeState(0,0);e.setParams(p);
        check(equal(previous,e.getRankEnvelopeState(0,0)),"gate edge preserves envelope state and elapsed window");
        const float oldValue=previous.amplitude.value;render(e,1);auto after=e.getRankEnvelopeState(0,0);
        const float seconds=open?8.f:1.f;const float coefficient=static_cast<float>(std::exp(-6.0/(seconds*rate*2)));
        near(after.amplitude.value,oldValue*coefficient*coefficient,"current gate selects floor without resurrecting cancelled floor",3e-6f);
        check(!e.getVoiceInfo(0).held&&!e.getVoiceInfo(0).pedalHeld,"gate edits never restore held flags");
    }
    // The SUSTAIN lever itself continuously changes the existing release floor.
    p[P_footswitchPressed]=1;p[P_masterSustain]=.4f;e.setParams(p);auto before=e.getRankEnvelopeState(0,0);render(e,1);
    const float c=static_cast<float>(std::exp(-6.0/(4.*rate*2)));near(e.getRankEnvelopeState(0,0).amplitude.value,before.amplitude.value*c*c,"live master sustain floor");
    e.allNotesOff(false);p[P_footswitchPressed]=0;e.setParams(p);p[P_footswitchPressed]=1;e.setParams(p);render(e,10);check(e.activeVoices()==0,"gate reopening cannot resurrect inactive notes");
}
void partitionTests(double rate){
    Engine whole,split;auto p=settings();init(whole,rate,p);init(split,rate,p);whole.noteOn(60,.8f);split.noteOn(60,.8f);
    for(int event=0;event<5;++event){
        if(event==1)for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vA)]=.2f;p[chParam(ch,CP_fA)]=.2f;}
        if(event==2){whole.noteOff(60);split.noteOff(60);}
        if(event==3)for(int ch=0;ch<2;++ch){p[chParam(ch,CP_vR)]=.4f;p[chParam(ch,CP_fR)]=.4f;}
        whole.setParams(p);split.setParams(p);
        const int frames=static_cast<int>(rate*.04)+17;auto a=render(whole,frames),b=render(split,frames,31,&p);
        check(a==b,"unchanged repeated setters and block partition are bit-identical");
        for(int ch=0;ch<2;++ch)check(equal(whole.getRankEnvelopeState(0,ch),split.getRankEnvelopeState(0,ch)),"partition preserves exact envelope state");
    }
    // Live target selection preserves the update boundary, then changes release.
    auto before=whole.getRankEnvelopeState(0,0);p[chParam(0,CP_vS)]=.9f;p[chParam(0,CP_fIl)]=.9f;p[chParam(0,CP_fAl)]=.1f;whole.setParams(p);
    check(equal(before,whole.getRankEnvelopeState(0,0)),"live target edits do not invent instantaneous transition");
    auto expected=render(split,128),actual=render(whole,128);check(actual!=expected,"isolation live IL changes the active release trajectory");
    // Isolation transitions retain their documented reset policy.
    p[P_parityMode]=0;whole.setParams(p);check(whole.activeVoices()==0,"leaving isolation clears notes");p[P_parityMode]=1;whole.setParams(p);check(whole.activeVoices()==0,"entering isolation does not resurrect notes");
    for(int ch=0;ch<2;++ch){auto s=whole.getRankEnvelopeState(0,ch);check(s.amplitude.stage==Adsr::Stage::Idle&&s.filter.stage==FilterEnv::Stage::Idle&&s.amplitude.value==0&&s.filter.value==0,"mode transition resets envelope states");}
    check(whole.getRankEnvelopeState(-1,0).amplitude.value==0&&whole.getRankEnvelopeState(0,2).filter.value==0,"invalid envelope diagnostics safe");
}
}
int main(){for(double rate:{44100.,48000.,96000.,192000.}){primitiveTests(static_cast<float>(rate*2));engineTimingTests(rate);gateTests(rate);partitionTests(rate);}std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

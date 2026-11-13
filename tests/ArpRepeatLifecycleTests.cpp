#include "core/Engine.h"
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
namespace {
constexpr int rate=48000,repeatFrame=1000,removeFrame=7000,tolerance=2;
constexpr float firstVelocity=.25f,replacementVelocity=.75f,otherVelocity=.5f;
struct Held {int count=0,index=-1,note=-1,channel=-1;float velocity=0;};
Held held(const cs80::Engine& e){Held h;for(int i=0;i<8;++i){auto v=e.getVoiceInfo(i);if(v.active&&v.held){++h.count;h.index=i;h.note=v.note;h.channel=v.channel;h.velocity=v.velocity;}}return h;}
struct Event{int frame;Held h;};
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",text);}
bool tick(cs80::Engine& e){float out[2]{};e.render(out,1);return std::isfinite(out[0])&&std::isfinite(out[1]);}
void record(std::vector<Event>& events,int frame,Held h,int& last){if(h.note!=last){events.push_back({frame,h});last=h.note;}}
}
int main(){
 auto actual=std::make_unique<cs80::Engine>(),reference=std::make_unique<cs80::Engine>();
 cs80::ParamValues p;p[cs80::P_parityMode]=0;p[cs80::P_sourcePerformance]=0;
 p[cs80::P_kasScanTiming]=0;p[cs80::P_oversampling]=0;
 p[cs80::P_voiceDispersion]=p[cs80::P_drift]=p[cs80::P_analogNoise]=0;
 p[cs80::P_arpOn]=1;p[cs80::P_arpMode]=4;p[cs80::P_arpOctaves]=1;p[cs80::P_arpRate]=16;
 for(auto* e:{actual.get(),reference.get()}){e->prepare(rate);e->setParams(p);e->noteOn(60,firstVelocity,1);e->noteOn(64,otherVelocity,1);}
 check(actual->getInternalSampleRate()==96000&&actual->getOversamplingFactor()==2,"Legacy fixed 2x clock");
 bool finite=true,single=true;std::vector<Event>a,r;int alast=-1,rlast=-1;
 for(int frame=0;frame<removeFrame;++frame){
  if(frame==repeatFrame){auto before=held(*actual);actual->noteOn(60,replacementVelocity,1);auto after=held(*actual);
   check(before.count==1&&before.note==60&&before.channel==1&&before.velocity==firstVelocity,"positive current source before repeat");
   check(after.count==1&&after.index==before.index&&after.note==before.note&&after.channel==before.channel&&after.velocity==before.velocity,"repeat preserves current generated voice and velocity immediately");}
  finite=tick(*actual)&&finite;finite=tick(*reference)&&finite;auto ah=held(*actual),rh=held(*reference);single&=ah.count<=1&&rh.count<=1;
  record(a,frame,ah,alast);record(r,frame,rh,rlast);
 }
 bool timing=a.size()==r.size();if(timing)for(size_t i=0;i<a.size();++i)timing&=(a[i].h.note<0)==(r[i].h.note<0)&&std::abs(a[i].frame-r[i].frame)<=tolerance;
 check(timing,"repeat preserves gate expiry and scheduled onset times versus untouched control");
 std::vector<Event>onsets;for(auto x:a)if(x.h.note>=0)onsets.push_back(x);
 check(onsets.size()==3&&onsets[0].h.note==60&&onsets[1].h.note==60&&onsets[2].h.note==64,"sequence restarts from retained played order without duplicate entry");
 check(onsets.size()==3&&onsets[1].h.velocity==replacementVelocity&&onsets[1].h.channel==1&&onsets[2].h.velocity==otherVelocity&&onsets[2].h.channel==1,"future generated occurrences use replaced and untouched source velocities");
 auto beforeRemoval=held(*actual);actual->noteOff(60,1);auto afterRemoval=held(*actual);
 check(beforeRemoval.count==1&&beforeRemoval.note==64&&afterRemoval.index==beforeRemoval.index&&afterRemoval.note==64,"single source release leaves current other source intact");
 bool onlyOther=true;int otherOnsets=0;int previous=afterRemoval.note;
 for(int i=0;i<9000;++i){finite=tick(*actual)&&finite;auto h=held(*actual);single&=h.count<=1;
  if(h.count)onlyOther&=h.note==64&&h.channel==1&&h.velocity==otherVelocity;
  if(h.note==64&&previous<0)++otherOnsets;previous=h.note;}
 check(onlyOther&&otherOnsets>=2,"one note-off removes repeated source with no duplicate obligation");
 actual->noteOff(64,1);bool stopped=held(*actual).count==0;
 for(int i=0;i<6000;++i){finite=tick(*actual)&&finite;stopped&=held(*actual).count==0;}
 check(stopped,"final source release stops generated pattern");check(finite&&single,"finite audio and at most one generated held voice");
 for(auto x:a)std::printf("EVENT frame=%d note=%d channel=%d velocity=%.9g\n",x.frame,x.h.note,x.h.channel,x.h.velocity);
 std::printf("RESULT checks=%d failures=%d\n",checks,failures);return failures?1:0;
}

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;
namespace {
constexpr int sr=48000;
int frame(double t){return static_cast<int>(std::llround(t*sr));}
double db(double x){return 20*std::log10(std::max(x,1e-15));}
const FactoryPreset& find(const char* name){const auto& bank=curatedLibrary();auto i=std::find_if(bank.begin(),bank.end(),[&](const auto&p){return p.name==name;});if(i==bank.end())throw std::runtime_error(name);return *i;}
struct Event {int at,type,note;float value;}; // 0=on,1=off,2=pressure
struct Result {std::vector<float> audio;int maxVoices=0,finalVoices=0;double targetLead=0,targetOther=0;bool route=true;};
std::vector<Event> sequence(int note){
 std::vector<Event> events;
 auto add=[&](double t,int type,int n,float v){events.push_back({frame(t),type,n,v});};
 auto ramp=[&](double start,double stop,float a,float b){int count=static_cast<int>(std::lround((stop-start)/.005));for(int i=0;i<=count;i++)add(start+i*.005,2,note,a+(b-a)*i/count);};
 add(0,0,note,.3f);ramp(.3,1.3,0,1);ramp(1.8,2.8,1,0);add(2.9,1,note,0);
 add(3.05,0,note,.95f);add(3.15,0,note+4,.8f);add(3.25,0,note+7,.8f);add(3.35,0,note+12,.85f);
 ramp(3.5,4.3,0,1);add(4.45,1,note,0);add(4.5,0,note,.55f);ramp(4.65,5.25,1,0);
 for(int n:{note,note+4,note+7,note+12})add(5.4,1,n,0);
 std::stable_sort(events.begin(),events.end(),[](const auto&a,const auto&b){return a.at<b.at;});return events;
}
Result render(const FactoryPreset& p,int note,bool poly){
 auto e=std::make_unique<Engine>();e->prepare(sr);e->setParams(p.values);
 if(e->getRenderPolicy()!=RenderPolicy::sourcePerformance)throw std::runtime_error("wrong policy");
 auto events=sequence(note);size_t next=0;Result r;r.audio.resize(frame(7)*2);
 for(int at=0;at<frame(7);){
  while(next<events.size()&&events[next].at==at){const auto&x=events[next++];if(x.type==0)e->noteOn(x.note,x.value,1);else if(x.type==1)e->noteOff(x.note,1);else if(poly)e->polyPressure(x.note,x.value,1);else e->channelPressure(1,x.value);}
  if(at==frame(4.1)){
   int checked=0;for(int i=0;i<8;i++){auto v=e->getVoiceInfo(i);if(!v.active||!v.held)continue;++checked;if(v.note==note)r.targetLead=v.pressure;else r.targetOther=std::max(r.targetOther,double(v.pressure));
    r.route=r.route&&((poly&&v.note!=note)?std::abs(v.pressure)<1e-7:v.pressure>.7f);
   }r.route=r.route&&checked==4;
  }
  int count=std::min(128,frame(7)-at);if(next<events.size())count=std::min(count,events[next].at-at);if(at<frame(4.1))count=std::min(count,frame(4.1)-at);
  if(count<=0)throw std::runtime_error("timeline stalled");e->render(r.audio.data()+2*at,count);at+=count;r.maxVoices=std::max(r.maxVoices,e->activeVoices());
 }
 r.finalVoices=e->activeVoices();return r;
}
double rms(const std::vector<float>&a,double begin,double end){double energy=0;for(int i=frame(begin)*2;i<frame(end)*2;i++)energy+=double(a[i])*a[i];return std::sqrt(energy/(2*(frame(end)-frame(begin))));}
void pairedStep(const FactoryPreset& p,int note,std::ofstream&f){
 auto a=std::make_unique<Engine>(),b=std::make_unique<Engine>();for(auto* e:{a.get(),b.get()}){e->prepare(sr);e->setParams(p.values);e->noteOn(note,.9f,1);}
 constexpr int start=28800,total=48000;std::vector<float> changed(total*2),stationary(total*2);double touchBefore=0,touchAfterSet=0,maxTouchDelta=0,previousTouch=0;
 for(int at=0;at<total;){
  if(at==start){touchBefore=a->getVoiceInfo(0).touchA;a->channelPressure(1,1);touchAfterSet=a->getVoiceInfo(0).touchA;previousTouch=touchAfterSet;}
  // Single frames around the boundary expose actual pressure-node continuity.
  int n=(at>=start&&at<start+480)?1:std::min(128,total-at);if(at<start)n=std::min(n,start-at);
  a->render(changed.data()+at*2,n);b->render(stationary.data()+at*2,n);
  if(at>=start&&at<start+480){double t=a->getVoiceInfo(0).touchA;maxTouchDelta=std::max(maxTouchDelta,std::abs(t-previousTouch));previousTouch=t;}at+=n;
 }
 double beforeError=0,boundaryError=0,firstMsError=0,residualDiffMax=0,signalDerivativeMax=0;
 for(int i=0;i<start*2;i++)beforeError=std::max(beforeError,std::abs(double(changed[i])-stationary[i]));
 for(int ch=0;ch<2;ch++)boundaryError=std::max(boundaryError,std::abs(double(changed[start*2+ch])-stationary[start*2+ch]));
 for(int i=start*2;i<(start+48)*2;i++)firstMsError=std::max(firstMsError,std::abs(double(changed[i])-stationary[i]));
 for(int at=start;at<start+480;at++)for(int ch=0;ch<2;ch++){
  auto k=at*2+ch;double r=double(changed[k])-stationary[k],previous=double(changed[k-2])-stationary[k-2];residualDiffMax=std::max(residualDiffMax,std::abs(r-previous));signalDerivativeMax=std::max(signalDerivativeMax,std::abs(double(stationary[k])-stationary[k-2]));
 }
 const double level=rms(stationary,.55,.6);
 f<<p.name<<','<<note<<','<<beforeError<<','<<boundaryError<<','<<firstMsError<<','<<maxTouchDelta<<','<<touchAfterSet-touchBefore<<','<<db(residualDiffMax)<<','<<db(residualDiffMax/level)<<','<<db(signalDerivativeMax)<<','<<db(rms(changed,.85,.95)/rms(stationary,.85,.95))<<'\n';
 if(beforeError!=0||touchAfterSet!=touchBefore||maxTouchDelta>.000209||firstMsError>1e-5)throw std::runtime_error("unexpected boundary discontinuity");
}
}
int main(){try{
 std::ofstream f("build-w04-wind-performance/sequences.csv"),step("build-w04-wind-performance/pressure-step.csv");f<<std::setprecision(12);step<<std::setprecision(12);
 f<<"program,note,pressure_mode,seconds,peak_dbfs,rms_dbfs,tail_last_200ms_dbfs,nonfinite,over_full_scale,max_voices,final_voices,lead_pressure_at4p1,other_max_pressure_at4p1,route_correct\n";
 step<<"program,note,pre_event_max_error,event_sample_error,first_ms_max_error,touch_max_host_delta_10ms,touch_jump_on_message,residual_derivative_10ms_dbfs,residual_derivative_relative_control_rms_db,control_derivative_10ms_dbfs,late_held_gain_db\n";
 int renders=0;for(const char* name:{"Flute","Breathy Flute"})for(int note:{55,67,79}){
  auto& p=find(name);for(bool poly:{false,true}){auto r=render(p,note,poly);double peak=0;int nonfinite=0,over=0;for(float x:r.audio){if(!std::isfinite(x))++nonfinite;peak=std::max(peak,std::abs(double(x)));if(std::abs(x)>1)++over;}
   double tail=rms(r.audio,6.8,7.0);f<<name<<','<<note<<','<<(poly?"poly":"channel")<<",7,"<<db(peak)<<','<<db(rms(r.audio,0,7))<<','<<db(tail)<<','<<nonfinite<<','<<over<<','<<r.maxVoices<<','<<r.finalVoices<<','<<r.targetLead<<','<<r.targetOther<<','<<r.route<<'\n';
   if(nonfinite||over||r.finalVoices!=0||!r.route||tail>1e-5)throw std::runtime_error("performance check failed");++renders;
  }pairedStep(p,note,step);renders+=2;
 }
 std::printf("Passed %d renders, 96 seconds total, factory parameters unchanged.\n",renders);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;
namespace {
constexpr int rate=48000;
constexpr const char* out="build-w06-performance-controls/";
int frame(double t){return static_cast<int>(std::llround(t*rate));}
const FactoryPreset& preset(const std::string& n){for(const auto& p:curatedLibrary())if(p.name==n)return p;throw std::runtime_error(n);}
struct Event {int at,kind;float value;};
std::vector<Event> pressureEvents(bool changing){std::vector<Event> e;
 auto add=[&](double t,int k,float v){e.push_back({frame(t),k,v});};
 auto ramp=[&](double a,double b,float x,float y){for(int i=0;i<=std::lround((b-a)/.005);++i){double t=a+i*.005;add(t,2,changing?x+(y-x)*(t-a)/(b-a):0);}};
 add(0,0,.8f);ramp(.5,1,0,1);ramp(2.0,2.5,1,0);add(3.2,1,0);
 add(4.5,0,.8f);ramp(4.6,5.1,0,1);add(5.7,1,0);add(6.5,2,0);
 std::stable_sort(e.begin(),e.end(),[](const auto&a,const auto&b){return a.at<b.at;});return e;}
std::vector<Event> velocityEvents(bool changing){return {{frame(0),0,.2f},{frame(.6),1,0},{frame(1.1),0,changing?1.f:.2f},{frame(1.7),1,0},{frame(2.2),0,.2f},{frame(2.8),1,0}};}
void render(const std::string& name,const std::string& slug,bool control,bool velocity,std::ofstream& info,std::ofstream& timeline){
 auto e=std::make_unique<Engine>();e->prepare(rate);e->setParams(preset(name).values);
 if(e->getRenderPolicy()!=RenderPolicy::sourcePerformance)throw std::runtime_error("wrong render policy");
 const double seconds=velocity?4:8;auto events=velocity?velocityEvents(!control):pressureEvents(!control);size_t next=0;
 std::string id=slug+(control?"_control":"_performed");
 for(const auto&x:events)timeline<<id<<','<<double(x.at)/rate<<','<<x.kind<<','<<x.value<<'\n';
 std::ofstream trace(std::string(out)+id+".csv");trace<<std::setprecision(12)<<"time,active,held,pressure,touchA,osc1,osc2,lpf1,lpf2,amp1,amp2\n";
 std::vector<float> audio(frame(seconds)*2);int maxVoices=0;int nextTrace=0;
 for(int at=0;at<frame(seconds);){
  while(next<events.size()&&events[next].at==at){auto x=events[next++];if(x.kind==0)e->noteOn(60,x.value,1);else if(x.kind==1)e->noteOff(60,1);else e->channelPressure(1,x.value);}
  if(at==nextTrace){int slot=0;for(int i=0;i<8;++i)if(e->getVoiceInfo(i).held){slot=i;break;}auto v=e->getVoiceInfo(slot);auto a=e->getRankInfo(slot,0),b=e->getRankInfo(slot,1);trace<<double(at)/rate<<','<<e->activeVoices()<<','<<v.held<<','<<v.pressure<<','<<v.touchA<<','<<a.oscillatorHz<<','<<b.oscillatorHz<<','<<a.lpfHz<<','<<b.lpfHz<<','<<a.ampEnvelope<<','<<b.ampEnvelope<<'\n';nextTrace+=480;}
  int n=std::min(128,frame(seconds)-at);if(next<events.size())n=std::min(n,events[next].at-at);n=std::min(n,nextTrace-at);if(n<=0)throw std::runtime_error("timeline");
  e->render(audio.data()+at*2,n);at+=n;maxVoices=std::max(maxVoices,e->activeVoices());
 }
 std::ofstream raw(std::string(out)+id+".f32",std::ios::binary);raw.write(reinterpret_cast<const char*>(audio.data()),audio.size()*sizeof(float));
 double peak=0;int nonfinite=0,over=0;for(float x:audio){if(!std::isfinite(x))++nonfinite;peak=std::max(peak,std::abs(double(x)));if(std::abs(x)>1)++over;}
 info<<id<<','<<name<<','<<seconds<<','<<peak<<','<<nonfinite<<','<<over<<','<<maxVoices<<','<<e->activeVoices()<<'\n';
 if(nonfinite||over||e->activeVoices()!=0)throw std::runtime_error("integrity/release: "+id);
}
}
int main(){try{
 std::ofstream info(std::string(out)+"captures.csv"),timeline(std::string(out)+"events.csv");info<<std::setprecision(12)<<"capture,program,seconds,peak,nonfinite,over_full_scale,max_voices,final_voices\n";timeline<<std::setprecision(12)<<"capture,time,event_kind,value\n";
 for(auto c:{std::pair{"Pressure Lead","pressure_lead"},std::pair{"Wah Lead","wah_lead"},std::pair{"Aftertouch Vibrato Lead","vibrato_lead"}})for(bool control:{false,true})render(c.first,c.second,control,false,info,timeline);
 for(auto c:{std::pair{"Velocity Keys","velocity_keys"},std::pair{"Velocity Lead","velocity_lead"}})for(bool control:{false,true})render(c.first,c.second,control,true,info,timeline);
 std::cout<<"Rendered 10 factory captures, 64 seconds, finite and below full scale; every sequence retired all voices.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

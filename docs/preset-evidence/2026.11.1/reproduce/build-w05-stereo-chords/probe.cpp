#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;
struct Event { int frame; int type; int note; float value; };
int main() {
 constexpr int rate=48000, frames=rate*12;
 std::vector<Event> events;
 const std::array<int,4> first {48,55,60,64}, second {53,57,62,69};
 for(int i=0;i<4;++i) {
  events.push_back({int(std::lround((.035*i)*rate)),0,first[i],.8f+.05f*(i%2)});
  events.push_back({int(std::lround((3.6+.035*i)*rate)),1,first[i],0.f});
  events.push_back({int(std::lround((3.75+.035*i)*rate)),0,second[i],1.f});
  events.push_back({int(std::lround((5.3+.035*i)*rate)),1,second[i],0.f});
 }
 for(int tick=0;tick<=600;++tick) {
  const double t=tick*.01;
  float pressure=0.f;
  if(t>=1.0 && t<2.0)pressure=float(t-1.0);
  else if(t>=2.0 && t<4.7)pressure=1.f;
  else if(t>=4.7 && t<5.2)pressure=float((5.2-t)/.5);
  events.push_back({tick*480,2,0,pressure});
 }
 std::stable_sort(events.begin(),events.end(),[](const Event&a,const Event&b){return a.frame<b.frame;});
 std::ofstream sequence("build-w05-stereo-chords/events.csv");
 sequence<<"seconds,type,note,value\n"<<std::setprecision(9);
 for(const auto&ev:events)sequence<<double(ev.frame)/rate<<','<<ev.type<<','<<ev.note<<','<<ev.value<<'\n';
 std::ofstream report("build-w05-stereo-chords/capture.csv");
 report<<"name,variant,peak_dbfs,rms_dbfs,final_half_second_dbfs,nonfinite,over_full_scale,max_active_voices,final_active_voices,retired_seconds\n"<<std::setprecision(12);
 const std::array<const char*,5> names{"Noir Swell Brass","Brass Ensemble 16","Neo-Noir Pad","Arctic Pad","Synthwave Chords"};
 auto db=[](double x){return 20*std::log10(std::max(1e-15,x));};
 for(const auto*name:names)for(bool centered:{false,true}) {
  const auto& bank=curatedLibrary();
  const auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==name;});
  if(it==bank.end()){std::cerr<<"Missing program";return 2;}
  auto p=it->values;
  if(centered){p[chParam(0,CP_pan)]=0.f;p[chParam(1,CP_pan)]=0.f;}
  auto engine=std::make_unique<Engine>();engine->prepare(rate);engine->setParams(p);
  if(engine->getRenderPolicy()!=RenderPolicy::sourcePerformance)return 3;
  std::vector<float> audio(frames*2);
  size_t event=0;int maxVoices=0;double retired=-1;
  for(int at=0;at<frames;) {
   while(event<events.size() && events[event].frame<=at) {
    const auto&ev=events[event++];
    if(ev.type==0)engine->noteOn(ev.note,ev.value,1);
    else if(ev.type==1)engine->noteOff(ev.note,1);
    else engine->channelPressure(1,ev.value);
   }
   int count=std::min(128,frames-at);
   if(event<events.size())count=std::min(count,events[event].frame-at);
   if(count<=0)return 4;
   engine->render(audio.data()+at*2,count);at+=count;
   maxVoices=std::max(maxVoices,engine->activeVoices());
   if(at>6*rate && engine->activeVoices()==0 && retired<0)retired=double(at)/rate;
  }
  double peak=0,energy=0,tail=0;int nonfinite=0,over=0;
  for(int i=0;i<frames*2;++i){const double x=audio[i];if(!std::isfinite(x))++nonfinite;if(std::abs(x)>1)++over;peak=std::max(peak,std::abs(x));energy+=x*x;if(i>=(frames-rate/2)*2)tail+=x*x;}
  std::string slug=name;std::replace(slug.begin(),slug.end(),' ','-');
  const std::string variant=centered?"centered-control":"factory";
  std::ofstream raw("build-w05-stereo-chords/"+slug+"-"+variant+".f32",std::ios::binary);
  raw.write(reinterpret_cast<const char*>(audio.data()),audio.size()*sizeof(float));
  report<<name<<','<<variant<<','<<db(peak)<<','<<db(std::sqrt(energy/audio.size()))<<','<<db(std::sqrt(tail/rate))<<','<<nonfinite<<','<<over<<','<<maxVoices<<','<<engine->activeVoices()<<','<<retired<<'\n';report.flush();
  std::cout<<name<<' '<<variant<<" peak "<<db(peak)<<" voices "<<maxVoices<<" final "<<engine->activeVoices()<<std::endl;
 }
}

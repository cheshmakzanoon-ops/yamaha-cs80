// A18: whole authored programs, fixed 48 kHz; only quality/MIDI gestures vary.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
using namespace cs80;
int main(int argc,char**argv){
 if(argc!=2&&argc!=3)return 2;const bool chordsOnly=argc==3;std::filesystem::create_directories(argv[1]);
 const auto& bank=programLibrary();
 for(const char*name:{"Solo Violin","Solo Cello","Cellos","PWM Strings"}){
  auto it=std::find_if(bank.begin(),bank.end(),[&](const auto&p){return p.name==name;});if(it==bank.end())return 3;
  for(int q=0;q<2;++q)for(int scene=0;scene<7;++scene){
   if(chordsOnly&&scene!=6)continue;
   if(scene==6&&std::string(name).find("Solo")==0)continue;
   auto p=it->values;p[P_oversampling]=q?2.f:1.f;
   auto e=std::make_unique<Engine>();e->setParams(p);e->prepare(48000);
   int note=scene==1?48:scene==2?72:60;const char*label[]={"held","low","high","pressure","bend","earlyoff","chord"};
   if(scene==6){for(int n:{48,55,60,67})e->noteOn(n,.85f,1);}else e->noteOn(note,.85f,1);
   constexpr int rate=48000,total=rate*3;
   const int off=scene==5?rate/5:rate*2;
   std::vector<float>a(total*2);
   const int events[]={rate/5,rate*3/5,rate*7/5,rate*2,total};
   for(int at=0;at<total;){
    if(at==rate*3/5){if(scene==3||scene==6)e->channelPressure(1,.8f);if(scene==4)e->pitchBend(1,1.f);}
    if(at==rate*7/5){if(scene==3||scene==6)e->channelPressure(1,0.f);if(scene==4)e->pitchBend(1,0.f);}
    if(at==off){if(scene==6){for(int n:{48,55,60,67})e->noteOff(n,1);}else e->noteOff(note,1);}
    int boundary=total;for(int event:events)if(event>at){boundary=event;break;}
    int n=std::min(113,boundary-at);e->render(a.data()+2*at,n);at+=n;
   }
   std::string slug=name;std::replace(slug.begin(),slug.end(),' ','_');
   const char*quality=q?"hq":"native";auto path=std::filesystem::path(argv[1])/(slug+"_"+quality+"_"+label[scene]+".f32");
   auto*f=fopen(path.c_str(),"wb");if(!f)return 4;fwrite(a.data(),sizeof(float),a.size(),f);fclose(f);
   printf("%s,%s,%s,%d,%d,%.9g,%.9g\n",name,quality,label[scene],note,e->getOversamplingFactor(),p[P_pitchBendRange],p[P_body1Size]);fflush(stdout);
  }
 }
}

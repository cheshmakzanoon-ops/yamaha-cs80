// A16: unchanged factory recipes; only capture quality/nonrealtime are selected.
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
 if(argc!=3)return 2; const std::string suite=argv[2];std::filesystem::create_directories(argv[1]);
 const auto& bank=programLibrary();
 for(const char* name:{"French Horn","Muted Trumpet","Trombone Glide","Pressure Swell Brass","Synth Horns","Delay Brass"}){
  auto it=std::find_if(bank.begin(),bank.end(),[&](const auto&p){return p.name==name;});if(it==bank.end())return 3;
  if(suite=="dry"&&std::string(name)!="Delay Brass")continue;
  for(int quality=0;quality<3;++quality)for(int gesture=0;gesture<3;++gesture){
   if((suite=="baseline"||suite=="dry")&&gesture!=0)continue;
   if(gesture==1&&quality==2)continue;
   if(gesture==2&&(quality!=0||std::string(name)=="Pressure Swell Brass"||std::string(name)=="Synth Horns"||std::string(name)=="Delay Brass"))continue;
   if(suite=="frozen"&&gesture!=2)continue;
   auto p=it->values;if(suite=="dry"){p[P_fx1Mix]=0;p[P_fx2Mix]=0;}p[P_oversampling]=quality==1?2.f:1.f;
   auto e=std::make_unique<Engine>();e->setParams(p);e->setNonRealtime(quality==2);e->prepare(48000);e->noteOn(60,.8f,1);e->channelPressure(1,gesture==1?.8f:0.f);
   constexpr int rate=48000,held=3*rate,total=33*rate/10;std::vector<float>a(total*2);
   for(int at=0;at<total;){if(at==rate&&gesture==2)e->pitchBend(1,1.f);if(at==held)e->noteOff(60,1);
    int boundary=at<rate?rate:at<held?held:total;int n=std::min(113,boundary-at);e->render(a.data()+2*at,n);at+=n;}
   std::string slug=name;std::replace(slug.begin(),slug.end(),' ','_');
   const char*q=quality==0?"standard":quality==1?"hq":"offline";const char*g=gesture==0?"held":gesture==1?"pressure":"bend";
   auto path=std::filesystem::path(argv[1])/(slug+"_"+q+"_"+g+".f32");auto*f=fopen(path.c_str(),"wb");if(!f)return 4;fwrite(a.data(),sizeof(float),a.size(),f);fclose(f);
   printf("%s,%s,%s,%d,%g\n",name,q,g,e->getOversamplingFactor(),p[P_pitchBendRange]);fflush(stdout);
  }
 }
}

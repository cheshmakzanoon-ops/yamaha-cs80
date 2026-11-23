#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
namespace cs80::original_circuit { enum class Finish { amber, velvet, distant, neon, hollow, golden, frozen, electric, midnight, slow, bright, dark, wide, soft }; void finish(FactoryPreset&, Finish); }
using namespace cs80;
static std::vector<float> render(ParamValues p,int note,int seconds=3){auto e=std::make_unique<Engine>();e->prepare(48000);e->setParams(p);e->noteOn(note,.85f,1);std::vector<float>a(2*48000*seconds);for(int i=0;i<48000*seconds;i+=128)e->render(a.data()+2*i,std::min(128,48000*seconds-i));return a;}
static double rms(const std::vector<float>& a,int start,int end){double s=0;for(int i=start*2;i<end*2;++i)s+=double(a[i])*a[i];return std::sqrt(s/(2*(end-start)));}
static double db(double x){return 20*std::log10(std::max(1.e-12,x));}
int main(){
 const char* dir="/workspace/scratch/a9592b052b11/a16/";
 for(const char* name:{"Stage Keys","Electric Piano I","Solo Violin","Covered Horn","Warm Pad","Picked Bass"}){
  auto it=std::find_if(curatedLibrary().begin(),curatedLibrary().end(),[&](const auto&p){return p.name==name;});if(it==curatedLibrary().end())return 2;
  for(const auto style:{original_circuit::Finish::dark,original_circuit::Finish::bright}){
   auto p=*it;original_circuit::finish(p,style);auto a=render(p.values,60,3);std::string file=std::string(dir)+name+(style==original_circuit::Finish::dark?"-dark.f32":"-bright.f32");std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(a.data()),static_cast<std::streamsize>(a.size()*sizeof(float)));
  }
  auto slow=*it;original_circuit::finish(slow,original_circuit::Finish::slow);if(it->category=="Keys"||it->category=="Bass")for(int ch=0;ch<2;++ch)if(slow.values[chParam(ch,CP_vA)]!=it->values[chParam(ch,CP_vA)])return 3;
 }
 std::ofstream out(std::string(dir)+"tone-rank-dominance.csv");out<<"name,main_rank,main_body_db,other_body_db,difference_db\n";
 const auto& bank=originalLibrary();for(int i=0;i<22;++i){auto p=bank[i].values;float mix=p[P_mix];p[P_mix]=0;auto a=render(p,60,2);p[P_mix]=1;auto b=render(p,60,2);double x=rms(a,24000,96000)*(1-mix),y=rms(b,24000,96000)*mix;if(i%2)std::swap(x,y);out<<bank[i].name<<','<<(i%2?"II":"I")<<','<<db(x)<<','<<db(y)<<','<<db(x/y)<<'\n';}
 return 0;
}

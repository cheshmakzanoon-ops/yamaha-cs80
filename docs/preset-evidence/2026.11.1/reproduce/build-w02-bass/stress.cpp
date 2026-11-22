#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace cs80;
void candidate(ParamValues& p, bool upright) {
 p[chParam(1,CP_vcfLevel)]=.95f;
 p[chParam(1,CP_hpfCutoff)]=upright?300.f:450.f;
 p[P_mix]=upright?.19f:.21f;
}
int main(){
 std::puts("name,version,peak_dbfs,rms_dbfs,tail_dbfs,nonfinite");
 for(const char* name:{"Sine Bass","Upright Pluck"}) for(bool changed:{false,true}) {
  const auto& bank=curatedLibrary();auto it=std::find_if(bank.begin(),bank.end(),[&](const auto&p){return p.name==name;});
  auto p=it->values;if(changed)candidate(p,std::string(name)=="Upright Pluck");
  auto e=std::make_unique<Engine>(); e->prepare(48000);e->setParams(p);
  const int notes[]={36,43,48,55,60,64,67,72};
  for(int n:notes)e->noteOn(n,1.f,1);e->channelPressure(1,1.f);
  std::vector<float>a(240000);int at=0;while(at<120000){if(at==96000)for(int n:notes)e->noteOff(n,1);int k=std::min(128,120000-at);if(at<96000)k=std::min(k,96000-at);e->render(a.data()+at*2,k);at+=k;}
  double peak=0,power=0,tail=0;int nonfinite=0;for(size_t i=0;i<a.size();i++){if(!std::isfinite(a[i]))nonfinite++;peak=std::max(peak,std::abs((double)a[i]));power+=(double)a[i]*a[i];if(i>=230400)tail+=(double)a[i]*a[i];}
  auto db=[](double x){return 20*std::log10(std::max(1e-15,x));};
  std::printf("%s,%s,%.5f,%.5f,%.5f,%d\n",name,changed?"candidate":"baseline",db(peak),db(std::sqrt(power/a.size())),db(std::sqrt(tail/9600)),nonfinite);
 }
}

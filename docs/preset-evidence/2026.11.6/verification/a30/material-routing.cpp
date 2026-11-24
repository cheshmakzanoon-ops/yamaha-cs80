#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace cs80;
int checks=0,failures=0;
void check(bool b,const char* message){++checks;if(!b){++failures;std::printf("FAIL %s\n",message);}}
std::vector<float> render(ControlFrame f){
 auto e=std::make_unique<Engine>();e->prepare(48000);check(e->setControlFrame(f),"accepted frame");
 e->noteOn(60,.85f);std::vector<float> a(8192);e->render(a.data(),4096);return a;
}
int main(){
 for(int rank=0;rank<2;++rank){
  for(int selector=0;selector<14;++selector){
   ControlFrame f;auto& p=f.effectiveValues;p[P_sourcePerformance]=1;p[P_parityMode]=0;
   p[P_analogNoise]=p[P_drift]=p[P_voiceDispersion]=p[P_chorusOn]=0;p[P_mix]=float(rank);
   p[P_fx1Mix]=p[P_fx2Mix]=p[P_fx3Mix]=0;
   p[bodyParam(rank,0)]=body::bell;p[bodyParam(rank,1)]=1;p[bodyMaterialParam(rank)]=body::tubular;
   f.hardwareSources[rank]=selector;auto on=render(f);p[bodyParam(rank,0)]=0;p[bodyParam(rank,1)]=0;auto off=render(f);
   double energy=0,delta=0;bool finite=true;
   for(std::size_t i=0;i<on.size();++i){finite=finite&&std::isfinite(on[i])&&std::isfinite(off[i]);energy+=double(on[i])*on[i]+double(off[i])*off[i];delta+=std::pow(double(on[i])-off[i],2);}
   check(finite&&energy>1.e-8,"audible finite fixture");
   check(selector==13?delta>1.e-7:on==off,"only PANEL admits material bodies");
   std::printf("rank %d selector %d delta %.10g energy %.10g\n",rank,selector,delta,energy);
  }
  for(int selector=0;selector<11;++selector){
   ParamValues p;p[P_volume]=.42f;p[P_detune]=2.5f;
   for(int r=0;r<2;++r){p[bodyParam(r,0)]=body::bell;p[bodyParam(r,1)]=.8f;p[bodyParam(r,2)]=.7f;p[bodyParam(r,3)]=1.4f;p[bodySizeParam(r)]=1.5f;p[bodyVariantParam(r)]=3;p[bodyMaterialParam(r)]=body::gong;}
   auto before=p;applyHardwareTone(p,rank,selector);
   const auto indices=[&](int r){return std::array<int,7>{bodyParam(r,0),bodyParam(r,1),bodyParam(r,2),bodyParam(r,3),bodySizeParam(r),bodyVariantParam(r),bodyMaterialParam(r)};};
   for(int i:indices(rank))check(p[i]==paramDef(i).def,"tab resets all seven rank body controls");
   for(int i:indices(1-rank))check(p[i]==before[i],"tab preserves other rank body controls");
   check(p[P_volume]==before[P_volume]&&p[P_detune]==before[P_detune],"tab preserves global volume and detune");
  }
 }
 std::printf("A30 material routing/reset: %d checks, %d failures\n",checks,failures);return failures?1:0;
}

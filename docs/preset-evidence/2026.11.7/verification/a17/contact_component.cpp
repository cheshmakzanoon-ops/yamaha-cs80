// Causal diagnostic only: expose private state to silence modes after noteOn.
// Keeps the real authored Agogo contact envelope, RNG, onset and band filter.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#define private public
#include "core/InstrumentBody.h"
#undef private
int main(){
 for(int rank=0;rank<2;++rank)for(double rate:{96000.,384000.}){
  double energy=0;long count=0;
  for(unsigned seed=1;seed<=128;++seed){
   cs80::body::InstrumentBody b;b.prepare(rate,48000.);
   b.noteOn(cs80::body::bell,72,.85f,rank?1.f:.91f,rank?.25f:.70f,seed*0x9e3779b9u,1,0,cs80::body::agogo);
   b.count=0;
   for(int i=0;i<int(.02*rate);++i){double x=b.tick(523.25113f,rank?1.f:.91f);energy+=x*x;++count;}
  }
  std::printf("%d,%.0f,%.12g\n",rank+1,rate,std::sqrt(energy/count));
 }
}

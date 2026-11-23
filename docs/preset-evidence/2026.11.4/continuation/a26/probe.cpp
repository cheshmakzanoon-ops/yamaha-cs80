#include "core/InstrumentBody.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
using namespace cs80::body;
int main(){
 int errors=0,cases=0;std::uint64_t samples=0;
 auto hz=[](double n){return float(440.0*std::exp2((n-69.0)/12.0));};
 auto same=[&](float a,float b){++samples;if(std::memcmp(&a,&b,sizeof(float))||!std::isfinite(a)||!std::isfinite(b))++errors;};
 for(double rate:{44100.,48000.,96000.}){
  InstrumentBody reused;reused.prepare(rate*2,rate);
  for(int target=off;target<=celesta;++target){
   reused.noteOn(bowed,48.f,.83f,.62f,1.f,0x12ad3u,1.8f);
   for(int i=0;i<4137;++i)reused.tick(hz(48.+18.*i/4137.),.2f+.6f*i/4137.,.8f);
   InstrumentBody copy=reused;
   for(int i=0;i<4096;++i){float pitch=hz(66.+.3*std::sin(i*.01));same(reused.tick(pitch,.7f,.4f),copy.tick(pitch,.7f,.4f));}
   ++cases;
   InstrumentBody fresh;fresh.prepare(rate*2,rate);
   reused.noteOn(target,61.37f,.44f,.73f,1.3f,0x112abdu,.75f);
   fresh.noteOn(target,61.37f,.44f,.73f,1.3f,0x112abdu,.75f);
   for(int i=0;i<4096;++i){float pitch=hz(61.37+.35*std::sin(i*.008));same(reused.tick(pitch,.67f,.6f),fresh.tick(pitch,.67f,.6f));}
   ++cases;
   reused.reset();same(reused.tick(hz(60.),.6f,.4f),0.f);if(reused.enabled())++errors;
   reused.noteOn(bowed,55.25f,.81f,.48f,.6f,0xa54du,2.2f);
   fresh.noteOn(bowed,55.25f,.81f,.48f,.6f,0xa54du,2.2f);
   for(int i=0;i<4096;++i){float pitch=hz(55.25-12.*i/4096.);same(reused.tick(pitch,.4f,.7f),fresh.tick(pitch,.4f,.7f));}
   ++cases;
   reused.prepare(rate*2,rate);
   fresh.prepare(rate*2,rate);
   reused.noteOn(bowed,72.f,.9f,.8f,2.f,0x679afdu,.5f);
   fresh.noteOn(bowed,72.f,.9f,.8f,2.f,0x679afdu,.5f);
   for(int i=0;i<4096;++i){float pitch=hz(72.-24.*i/4096.);same(reused.tick(pitch,.5f,.5f),fresh.tick(pitch,.5f,.5f));}
   ++cases;
  }
 }
 std::printf("lifecycle_cases=%d compared_float_samples=%llu differences_or_nonfinite=%d\n",cases,(unsigned long long)samples,errors);
 return errors?1:0;
}

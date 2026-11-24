#include "core/InstrumentBody.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace cs80::body;
namespace {
int checks=0, failures=0, edgeCases=0, fallbackCases=0, cutoffCases=0, malformedCases=0;
long long samples=0;
double peak=0.;
void check(bool ok,const char* label) { ++checks; if(!ok){++failures; std::printf("FAIL: %s\n",label);} }
int modelFor(int material) { return material<=agogo?bell:material==musicBox?celesta:material==kalimba?tine:material==harp?nylon:steel; }
float hz(int note) { return float(440.*std::exp2((note-69.)/12.)); }
bool valid(float x) { ++samples; peak=std::max(peak,std::abs(double(x))); return std::isfinite(x)&&std::abs(x)<.8341f; }
}
int main() {
 for(double rate:{44100.,48000.,96000.,192000.})
  for(int material=1;material<=maxMaterial;++material)
   for(int note:{0,24,60,96,127}) for(float velocity:{.01f,1.f})
    for(float size:{.5f,2.5f}) for(float decay:{.25f,4.f}) {
     InstrumentBody b;b.prepare(2*rate,rate);
     b.noteOn(modelFor(material),float(note),velocity,1,decay,0x87162u,size,standard,material);
     bool finiteAndBounded=true;
     for(int i=0;i<4096;++i) {
      const int phase=(i/16)%4;
      const float frequency=phase==0?hz(note):phase==1?.5f*hz(note):phase==2?2.f*hz(note):float(2*rate);
      finiteAndBounded=valid(b.tick(frequency,((i/16)&1)?0.f:1.f))&&finiteAndBounded;
     }
     ++edgeCases; check(finiteAndBounded,"extreme register/rate/size/decay/pitch/brightness stays bounded");
     b.reset();check(!b.enabled()&&b.tick(hz(note),1)==0,"reset disables and silences material");
    }
 for(int model=off;model<=maxModel;++model) for(int material=-1;material<=maxMaterial+1;++material) {
  if(material==0 || compatibleMaterial(model,material)!=0)continue;
  InstrumentBody a,b;a.prepare(88200,44100);b.prepare(88200,44100);
  a.noteOn(model,60,.85f,.5f,1,12345,1,standard,material);
  b.noteOn(model,60,.85f,.5f,1,12345,1,standard,defaultMaterial);
  bool identical=true;
  for(int i=0;i<1024;++i) {const float f=(i&16)?220.f:440.f;const float x=a.tick(f,.6f),y=b.tick(f,.6f);identical=identical&&std::isfinite(x)&&x==y;samples+=2;}
  ++fallbackCases;check(identical,"incompatible/invalid selector equals neutral material sample-for-sample");
 }
 for(double rate:{44100.,48000.,96000.,192000.}) for(int material=gong;material<=maxMaterial;++material) {
  // These eight materials have no sub-fundamental modes. Tube is intentionally
  // excluded: its real lower modes can remain below Nyquist at this pitch.
  InstrumentBody b;b.prepare(2*rate,rate);b.noteOn(modelFor(material),60,1,1,4,123,1,standard,material);
  double tailPeak=0.;bool bounded=true;
  for(int i=0;i<int(2*rate*.8);++i) {const float x=b.tick(float(rate),1);bounded=valid(x)&&bounded;if(i>int(2*rate*.75))tailPeak=std::max(tailPeak,std::abs(double(x)));}
  check(bounded,"cutoff transition and long tail remain finite and bounded");
  ++cutoffCases;check(tailPeak<1.e-15,"above-host-Nyquist modal output vanishes after contact decays");
 }
 for(int material=1;material<=maxMaterial;++material) for(float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
  InstrumentBody b;b.prepare(std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity());
  b.noteOn(modelFor(material),invalid,invalid,invalid,invalid,0,invalid,standard,material);
  bool bounded=true;
  for(int i=0;i<1024;++i)bounded=valid(b.tick(261.625565f,invalid,invalid))&&bounded;
  check(bounded,"nonfinite onset and live controls use finite bounded defaults");
  auto reference=b;
  check(b.tick(invalid,1)==0 && b.tick(-1,1)==0 && b.tick(0,1)==0,"nonpositive/nonfinite frequencies return silence");
  check(b.tick(440,1)==reference.tick(440,1),"rejected frequency leaves the next accepted sample unchanged");
  ++malformedCases;
 }
 std::printf("Material extremes: %d checks, %d failures; edge=%d fallback=%d cutoff=%d malformed=%d; %lld samples; peak %.9f\n",checks,failures,edgeCases,fallbackCases,cutoffCases,malformedCases,samples,peak);
 return failures?1:0;
}

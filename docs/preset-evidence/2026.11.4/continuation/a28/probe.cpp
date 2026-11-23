#include "core/InstrumentBody.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <cstring>
using cs80::body::InstrumentBody;
int main(){
 constexpr float nan=std::numeric_limits<float>::quiet_NaN();
 constexpr float inf=std::numeric_limits<float>::infinity();
 std::array<std::array<double,2>,7> rates{{{8000,8000},{88200,44100},{96000,48000},{192000,96000},{1536000,96000},{-1,inf},{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::quiet_NaN()}}};
 InstrumentBody reused;
 long long samples=0,checks=0; int errors=0; double peak=0;
 for(auto rate:rates)for(int model=0;model<=cs80::body::maxModel;++model)for(float note:{0.f,60.f,127.f,nan})for(float size:{0.5f,2.5f,nan}){
   // Contaminate every cache and phase before re-preparing the reused object.
   reused.prepare(192000,96000);reused.noteOn(cs80::body::bowed,36,.99f,.9f,4,7,2.5f);
   for(int i=0;i<161;++i)reused.tick(4000.f,.7f,.9f);
   InstrumentBody fresh;
   fresh.prepare(rate[0],rate[1]);reused.prepare(rate[0],rate[1]);
   fresh.noteOn(model,note,.8f,.65f,1,0xabc123u,size);reused.noteOn(model,note,.8f,.65f,1,0xabc123u,size);
   for(int i=0;i<4096;++i){
     const float f=std::array<float,7>{.01f,261.625565f,1000.f,15000.f,1e30f,120.f,42.f}[i/19%7];
     // Invalid pitches must return zero without consuming phase or time.
     if(i%137==0){for(float bad:{nan,inf,-inf,-1.f,0.f}){++checks;if(reused.tick(bad,nan,inf)!=0)++errors;}}
     const float c=i%29==0?nan:.85f;const float pressure=i%31==0?inf:.72f;
     float a=fresh.tick(f,c,pressure),b=reused.tick(f,c,pressure);
     ++samples;++checks;if(std::memcmp(&a,&b,sizeof(float))!=0||!std::isfinite(b)||std::abs(b)>.8f)++errors;
     peak=std::max(peak,double(std::abs(b)));
     if(i==2047){InstrumentBody copied=reused;for(int j=0;j<23;++j){float x=copied.tick(523.25113f,.7f,.3f);float y=reused.tick(523.25113f,.7f,.3f);float z=fresh.tick(523.25113f,.7f,.3f);++checks;if(x!=y||y!=z)++errors;}}
   }
   reused.reset();++checks;if(reused.enabled()||reused.tick(440,.5f)!=0)++errors;
 }
 std::printf("samples=%lld checks=%lld errors=%d peak=%.9f\n",samples,checks,errors,peak);
 return errors?1:0;
}

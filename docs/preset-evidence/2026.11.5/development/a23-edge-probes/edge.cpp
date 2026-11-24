#include "core/BodyArticulation.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
static bool armed=false;
static unsigned long allocations=0;
void* operator new(std::size_t n){if(armed)++allocations; if(auto*p=std::malloc(n))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,std::size_t)noexcept{std::free(p);}
void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
int main(){
 using namespace cs80::body;
 const float nan=std::numeric_limits<float>::quiet_NaN(), inf=std::numeric_limits<float>::infinity();
 const float inputs[]={0,1,-1,127,nan,inf,-inf,FLT_MAX,-FLT_MAX};
 int failures=0, profiles=0, bodies=0; unsigned long samples=0; double peak=0;
 armed=true;
 for(int model=-1;model<=maxModel+1;++model)for(float note:inputs)for(float velocity:inputs)for(float mix:inputs){
  const auto a=captureArticulation(model,note,velocity,mix);++profiles;
  if(!std::isfinite(a.attack)||!std::isfinite(a.decay)||!std::isfinite(a.release)||a.attack<.79f||a.attack>1.36f||a.decay<.649f||a.decay>1.551f||a.release<.719f||a.release>1.401f)++failures;
  if((model<=off||model>maxModel||model==organ||!std::isfinite(mix)||mix<=0)&&(a.attack!=1||a.decay!=1||a.release!=1))++failures;
 }
 for(double rate:{8000.,44100.,48000.,96000.,192000.})for(int variant:{xylophone,steelpan,choirAh,choirOo})for(float note:inputs)for(float control:inputs){
  InstrumentBody b;b.prepare(2*rate,rate);b.noteOn(variant==xylophone?marimba:variant==steelpan?vibes:organ,note,control,control,control,17,control,variant);++bodies;
  const float pitches[]={0,-1,nan,inf,std::numeric_limits<float>::denorm_min(),FLT_MAX,8.175799f,12543.854f,(float)(rate*.4649),(float)(rate*.466)};
  for(int i=0;i<1024;++i){float pitch=pitches[(i/17)%10];const float x=b.tick(pitch,control,control);++samples;if(!std::isfinite(x)||std::abs(x)>.80f)++failures;peak=std::max(peak,std::abs((double)x));if((!std::isfinite(pitch)||pitch<=0)&&x!=0)++failures;}
 }
 for(int model=0;model<=maxModel;++model)for(int variant:{-2147483647,-1,0,1,2,3,4,5,2147483647}){
  const int legal=compatibleVariant(model,variant);
  InstrumentBody a,b;a.prepare(96000,48000);b.prepare(96000,48000);a.noteOn(model,60,.6f,.7f,1,419,1,variant);b.noteOn(model,60,.6f,.7f,1,419,1,legal);
  for(int i=0;i<2048;++i){float f=i<1024?261.625565f:1046.50226f;if(a.tick(f,.8f,.7f)!=b.tick(f,.8f,.7f))++failures;}
 }
 armed=false;
 std::printf("edge probe: profiles=%d bodies=%d samples=%lu failures=%d allocations=%lu peak=%.9f sizeofBody=%zu sizeofArticulation=%zu\n",profiles,bodies,samples,failures,allocations,peak,sizeof(InstrumentBody),sizeof(Articulation));
 return failures||allocations?1:0;
}

#include "BaselineInstrumentBody.h"
#include "PrototypeInstrumentBody.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

constexpr double pi=3.1415926535897932384626433832795;
double hz(double n){return 440*std::exp2((n-69)/12);}
double db(double a){return 20*std::log10(std::max(a,1e-15));}
struct Capture {std::vector<float> a; double peak=0; bool finite=true;};
template<class Body> Capture render(int model,int from,int to,float size=1.0f,double host=48000.0){
 const double rate=host*2; Body b; b.prepare(rate,host); b.noteOn(model,float(from),.8f,.65f,1.f,0x7812a3u,size);
 Capture x; x.a.resize(size_t(1.6*rate));
 for(size_t i=0;i<x.a.size();++i){double t=i/rate; double blend=std::clamp((t-.25)/.35,0.,1.); float freq=float(hz(from+(to-from)*blend));
   x.a[i]=b.tick(freq,.65f);x.peak=std::max(x.peak,double(std::abs(x.a[i])));x.finite=x.finite&&std::isfinite(x.a[i]);}
 return x;
}
double amp(const Capture& x,double f,double rate=96000.){
 int first=int(.9*rate),last=int(1.5*rate);double sumR=0,sumI=0,sumW=0;
 double ca=std::cos(2*pi*f/rate),sa=std::sin(2*pi*f/rate),r=1,im=0;
 for(int i=first;i<last;++i){double w=.5-.5*std::cos(2*pi*(i-first)/(last-first-1));sumW+=w;sumR+=w*x.a[i]*r;sumI+=w*x.a[i]*im;double nr=r*ca-im*sa;im=im*ca+r*sa;r=nr;}
 return 2*std::hypot(sumR,sumI)/sumW;
}
volatile double sink=0;
double cpu(){timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+1e-9*t.tv_nsec;}
template<class Body> double bench(int model,bool moving){
 std::array<Body,8>b;
 for(int i=0;i<8;++i){b[i].prepare(96000,48000);b[i].noteOn(model,float(48+i),.8f,.65f,1.f,0x7812a3u+i,1.f);}
 std::vector<std::array<float,8>>freq(96000);for(size_t k=0;k<freq.size();++k){double bend=moving?.4*std::sin(2*pi*5*k/96000.):0;for(int i=0;i<8;++i)freq[k][i]=float(hz(48+i+bend));}
 double start=cpu(),sum=0;for(const auto& row:freq)for(int i=0;i<8;++i)sum+=b[i].tick(row[i],.65f);sink=sum;return cpu()-start;
}
int main(){
 using Baseline=cs80::body::InstrumentBody;using Prototype=cs80prototype::body::InstrumentBody;
 std::puts("kind,model,from,to,size,partial,baseline_error_db,prototype_error_db,baseline_peak,prototype_peak");
 for(int model=cs80::body::bowed;model<=cs80::body::brass;++model)for(auto pair:{std::array<int,2>{36,48},std::array<int,2>{48,36},std::array<int,2>{72,84}}){float size=(model==cs80::body::bowed&&pair[0]<60)?1.8f:1.f;
 auto old=render<Baseline>(model,pair[0],pair[1],size),now=render<Prototype>(model,pair[0],pair[1],size),direct=render<Baseline>(model,pair[1],pair[1],size);
 double f=hz(pair[1]),o0=amp(old,f),n0=amp(now,f),d0=amp(direct,f);
 for(int h:{2,3,4,5,8,12}){if(f*h>19000)continue;double o=amp(old,f*h),n=amp(now,f*h),d=amp(direct,f*h);if(d/d0<.003)continue;
 std::printf("spectral,%d,%d,%d,%.2f,%d,%+.6f,%+.6f,%.6f,%.6f\n",model,pair[0],pair[1],size,h,db(o/o0)-db(d/d0),db(n/n0)-db(d/d0),old.peak,now.peak);}}
 size_t compared=0,different=0;double maxdiff=0;
 for(int model=0;model<=19;++model)for(int note:{24,60,96})for(double rate:{44100.,48000.,96000.}){
   auto old=render<Baseline>(model,note,note,1.f,rate),now=render<Prototype>(model,note,note,1.f,rate);for(size_t k=0;k<old.a.size();++k){++compared;if(old.a[k]!=now.a[k])++different;maxdiff=std::max(maxdiff,double(std::abs(old.a[k]-now.a[k])));}}
 std::printf("PARITY samples=%zu different=%zu maxdiff=%.9g\n",compared,different,maxdiff);
 for(int model:{0,1,2,9,10,11,12,13,14,15})for(bool moving:{false,true})for(int trial=0;trial<5;++trial){double o,n;if(trial%2){n=bench<Prototype>(model,moving);o=bench<Baseline>(model,moving);}else{o=bench<Baseline>(model,moving);n=bench<Prototype>(model,moving);}std::printf("CPU model=%d moving=%d trial=%d baseline=%.6f prototype=%.6f ratio=%.4f\n",model,int(moving),trial,o,n,n/o);}
}

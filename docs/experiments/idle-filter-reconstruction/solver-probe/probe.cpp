#include "core/Ig00156Filter.h"
#include "instrumented.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <array>
template<class M> std::array<double,12> named(const M&m){return {m.z.input,m.z.feedback,m.z.bp,m.z.lp,m.v.input,m.v.feedback,m.v.bp,m.v.lp,m.slope1,m.slope2,m.halfIntegrators,m.halfCouplings};}
bool equal(double a,double b){return std::bit_cast<uint64_t>(a)==std::bit_cast<uint64_t>(b);}
int main(){uint64_t samples=0,mismatch=0,nonfinite=0,caps[3]{},saved[3]{},hist[3][5]{};unsigned caseid=0;uint32_t rng=803021;
for(int clock=0;clock<3;++clock){float rate=clock==0?3000.f:clock==1?88200.f:96000.f;bool idle=clock==0;
for(int lp=0;lp<2;++lp)for(float fc:{0.f,18.f,200.f,2000.f,20000.f,.45f*rate})for(float q:{.35f,1.f,5.f,9.f})for(int pattern=0;pattern<3;++pattern){
 cs80::Ig00156Stage a(lp?cs80::kIg00156LowPassNetwork:cs80::kIg00156HighPassNetwork);diagnostic::Ig00156Stage b(lp?diagnostic::kIg00156LowPassNetwork:diagnostic::kIg00156HighPassNetwork);a.set(fc,q,rate,idle);b.set(fc,q,rate,idle);++caseid;
 for(int n=0;n<4096;++n){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;float in=pattern==0?float((double(rng)/4294967295.)*16.-8.):pattern==1?(n%2?8.f:-8.f):(n<2048?float(2.5*std::sin(n*.17)):0.f);auto pre=a.getMemory();auto x=a.process(in);auto y=b.process(in);++samples;
 bool same=equal(x.lp,y.lp)&&equal(x.hp,y.hp)&&equal(x.bp,y.bp);auto am=named(a.getMemory()),bm=named(b.getMemory());for(unsigned i=0;i<am.size();++i){same &=equal(am[i],bm[i]);if(!std::isfinite(am[i])||!std::isfinite(bm[i]))++nonfinite;}if(!same)++mismatch;
 if(!std::isfinite(x.lp)||!std::isfinite(x.hp)||!std::isfinite(x.bp)||!std::isfinite(y.lp)||!std::isfinite(y.hp)||!std::isfinite(y.bp))++nonfinite;
 ++hist[clock][diagnostic::terminal];if(diagnostic::terminal==4&&!(std::abs(diagnostic::residual)<1e-7)){++caps[clock];if(saved[clock]<256){++saved[clock];std::printf("CAP,%u,%d,%d,%d,%d,%a,%a,%a,%a",caseid,clock,lp,pattern,n,double(rate),double(fc),double(q),double(in));for(double z:named(pre))std::printf(",%a",z);for(double z:diagnostic::coefficients)std::printf(",%a",z);for(auto&t:diagnostic::trace)for(double z:t)std::printf(",%a",z);std::puts("");}}
 }
}}
std::printf("SUMMARY,cases=%u,samples=%llu,mismatches=%llu,nonfinite=%llu\n",caseid,(unsigned long long)samples,(unsigned long long)mismatch,(unsigned long long)nonfinite);
for(int c=0;c<3;++c){std::printf("CLOCK,%d,capped=%llu,captured=%llu,omitted=%llu,hist",c,(unsigned long long)caps[c],(unsigned long long)saved[c],(unsigned long long)(caps[c]-saved[c]));for(auto h:hist[c])std::printf(",%llu",(unsigned long long)h);std::puts("");}return mismatch||nonfinite?1:0;}

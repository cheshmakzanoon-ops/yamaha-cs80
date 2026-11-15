#include "core/Ig00156RootFallback.h"
#include "old-helper.h"
#include <random>
#include <cstdio>
#include <bit>
#include <array>
#include <limits>
using namespace cs80;using ig00156_detail::RootPoint;
auto fields(const RootPoint&p){return std::array<double,5>{p.bp,p.hp,p.t1,p.t2,p.residual};}
int main(){std::mt19937_64 rng(803021);unsigned accepted=0,rejected=0,bad=0,oldOnly=0,newOnly=0,both=0,neither=0;for(unsigned i=0;i<100000;++i){auto u=[&](){return double(rng()>>11)*0x1p-53;};double z=(u()-.5)*200,c0=(u()-.5)*200,c1=u()*10,g=u()*100,ff=u()*2,L=std::exp2((u()-.5)*40);
// Include outside physical monotonic prerequisites and extreme guard inputs.
if(i%17==0)c1=-u()*10;if(i%19==0)L=std::numeric_limits<double>::denorm_min();if(i%23==0)g=std::numeric_limits<double>::max();if(i%29==0)z=INFINITY;if(i%31==0)ff=-1;
RootPoint p{11,22,33,44,55};auto before=fields(p);bool ok=ig00156_detail::bracketedRoot(z,c0,c1,g,ff,L,p);cs80::old_root::RootPoint old{11,22,33,44,55};bool oldOk=cs80::old_root::bracketedRoot(z,c0,c1,g,ff,L,old);
if(oldOk&&ok)++both;else if(!oldOk&&!ok)++neither;else{if(oldOk)++oldOnly;else ++newOnly;printf("DIFFERENCE,%u,%s,%a,%a,%a,%a,%a,%a\n",i,oldOk?"old_only":"new_only",z,c0,c1,g,ff,L);}
if(!ok){++rejected;auto after=fields(p);for(unsigned j=0;j<5;++j)if(std::bit_cast<unsigned long long>(before[j])!=std::bit_cast<unsigned long long>(after[j]))++bad;continue;}++accepted;
for(double x:fields(p))if(!std::isfinite(x))++bad;
double inv=1/L,t2=fastTanh(p.bp*inv),hp=c0+c1*p.bp+g*L*t2+ff*p.bp,t1=fastTanh(hp*inv),res=p.bp-z+g*L*t1+ff*hp;
if(!std::isfinite(res)||!(std::abs(res)<1e-7)||std::abs(hp-p.hp)>1e-12||std::abs(t1-p.t1)>1e-12||std::abs(t2-p.t2)>1e-12||std::abs(res-p.residual)>1e-12)++bad;
}printf("relative old_only=%u new_only=%u both=%u neither=%u\n",oldOnly,newOnly,both,neither);printf("cases=100000 accepted=%u rejected=%u failures=%u\n",accepted,rejected,bad);return bad?1:0;}

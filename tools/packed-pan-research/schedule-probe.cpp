#include "core/PackedPanResearch.h"
#include <immintrin.h>
#include <array>
#include <cfenv>
#include <cerrno>
#include <cstdio>
#include <cstdint>
using Gains=std::array<float,4>;
int main(){const unsigned saved=_mm_getcsr();unsigned rejected=0;
for(unsigned mask=0;mask<63;++mask){unsigned cfg=((saved&~0x1fbfu)|(mask<<7));_mm_setcsr(cfg);errno=17;Gains g{3,4,5,6};const bool ok=cs80::research::tryPanPair(.1f,-.7f,g[0],g[1],g[2],g[3]);unsigned now=_mm_getcsr();int e=errno;_mm_setcsr(saved);if(ok||now!=cfg||e!=17||g!=Gains{3,4,5,6})return 1;++rejected;}
for(uint32_t bad:{0x7f800000u,0xff800000u,0x7fc00001u,0x7f800001u})for(bool second:{false,true}){Gains g{3,4,5,6};_mm_setcsr(saved&~0x3fu);errno=17;float a=std::bit_cast<float>(bad);bool ok=cs80::research::tryPanPair(second?.3f:a,second?a:.3f,g[0],g[1],g[2],g[3]);unsigned now=_mm_getcsr();int e=errno;_mm_setcsr(saved);if(ok||now!=(saved&~0x3fu)||e!=17||g!=Gains{3,4,5,6})return 2;++rejected;}
unsigned cases=0,packed=0;
for(int round:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u})for(int count=0;count<=16;++count)for(int frame=0;frame<16;++frame){std::fesetround(round);_mm_setcsr((_mm_getcsr()&~0x8040u)|denorm);std::array<float,16> pans{};for(int i=0;i<count;++i)pans[i]=float(i-8)/8;if(frame==1&&count>1)pans[1]=std::bit_cast<float>(0x7fc00001u);if(frame==2&&count>0)pans[0]=std::bit_cast<float>(0x7f800001u);std::array<cs80::Ig00151Vca,16>a{},b{};for(auto&v:a)v.prepare(96000);for(auto&v:b)v.prepare(96000);std::array<float,32>x{},y{};
auto run=[&](bool candidate,auto&v,auto&o){bool ready=false;float nl=0,nr=0;for(int i=0;i<count;++i){float gain=v[i].process(.3f,.7f,.5f,.8f);float l,r;if(candidate&&ready){l=nl;r=nr;ready=false;}else if(candidate&&i+1<count&&cs80::research::tryPanPair(pans[i],pans[i+1],l,r,nl,nr)){ready=true;++packed;}else cs80::constantPowerPan(pans[i],l,r);o[2*i]=gain*l;o[2*i+1]=gain*r;}};
std::feclearexcept(FE_ALL_EXCEPT);errno=17;run(false,a,x);unsigned xf=_mm_getcsr();int xe=errno;
std::feclearexcept(FE_ALL_EXCEPT);errno=17;run(true,b,y);unsigned yf=_mm_getcsr();int ye=errno;
if(std::bit_cast<std::array<uint32_t,32>>(x)!=std::bit_cast<std::array<uint32_t,32>>(y)||xf!=yf||xe!=ye){std::printf("FAIL count%d frame%d round%d denorm%u flags%u/%u\n",count,frame,round,denorm,xf,yf);return 3;}++cases;
}
std::fesetround(FE_TONEAREST);_mm_setcsr(saved);std::printf("PASS rejected%u untouched, scheduling%u cases packed%u with VCA interleave\n",rejected,cases,packed);}

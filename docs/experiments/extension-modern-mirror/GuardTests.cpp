#include <bits/stdc++.h>
#include <xmmintrin.h>
#define private public
#include "core/Engine.h"
#undef private
using namespace cs80;
static unsigned short cwget(){unsigned short x;asm volatile("fnstcw %0":"=m"(x));return x;}
static unsigned short swget(){unsigned short x;asm volatile("fnstsw %0":"=am"(x));return x;}
static void set(unsigned csr,unsigned short cw){asm volatile("fnclex; fldcw %0"::"m"(cw));_mm_setcsr(csr);errno=ERANGE;}
int main(int argc,char**){const bool none=argc>1;auto s=std::make_unique<Engine>(),d=std::make_unique<Engine>();std::array<unsigned,8> csrs={0x1f80,0x9fc0,0x1fc0,0x9f80,0x3f80,0x5f80,0x7f80,0x1f00};std::array<unsigned short,6> cws={0x037f,0x077f,0x0b7f,0x0f7f,0x027f,0x037e};unsigned accepts=0,rejects=0;for(auto cw:cws)for(auto csr:csrs){auto before=d->modern;set(csr,cw);const auto sw=swget();const bool ok=d->tryMirrorModernConfigFrom(*s);if(ok)++accepts;else++rejects;if((ok&&!(!none&&cw==0x037f&&(csr==0x1f80||csr==0x9fc0)))||(!ok&&(std::memcmp(&before,&d->modern,sizeof before)||_mm_getcsr()!=csr||cwget()!=cw||swget()!=sw||errno!=ERANGE))){set(0x1f80,0x037f);std::cerr<<"FAIL matrix\n";return 1;}}
// Both pending exception cases must reject without consuming the pending state.
set(0x1f80,0x037f);auto before=d->modern;asm volatile("fldz; fldz; fdivp; fstp %%st(0)":::"st");const auto sw=swget();const unsigned short unmask=0x037e;asm volatile("fldcw %0"::"m"(unmask));const auto pendingSw=swget();errno=ERANGE;const bool ok=d->tryMirrorModernConfigFrom(*s);const auto afterSw=swget();const auto afterCw=cwget();asm volatile("fnclex");set(0x1f80,0x037f);if(!(sw&1)||ok||pendingSw!=afterSw||afterCw!=unmask||std::memcmp(&before,&d->modern,sizeof before)){std::cerr<<"FAIL x87 pending\n";return 1;}
set(0x1f01,0x037f);const bool ok2=d->tryMirrorModernConfigFrom(*s);const auto afterCsr=_mm_getcsr();set(0x1f80,0x037f);if(ok2||afterCsr!=0x1f01||std::memcmp(&before,&d->modern,sizeof before)){std::cerr<<"FAIL SSE pending\n";return 1;}
std::cout<<"PASS controls48 accepted="<<accepts<<" rejected="<<rejects<<" +pending x87/SSE\n";return none&&accepts?1:0;}

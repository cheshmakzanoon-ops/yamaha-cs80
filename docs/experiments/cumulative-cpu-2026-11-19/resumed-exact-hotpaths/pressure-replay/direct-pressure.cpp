#include "Engine.cpp"
#include <csetjmp>
#include <csignal>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <bit>
#include <ucontext.h>
using namespace cs80;
static sigjmp_buf jump;
static int traps=0;static volatile sig_atomic_t hit,sc,scsr,sstatus,scw,serr;
void handler(int sig,siginfo_t* i,void* opaque) {
 serr=errno;
 auto uc=static_cast<ucontext_t*>(opaque);
 hit=sig;sc=i->si_code;scsr=uc->uc_mcontext.fpregs->mxcsr;sstatus=uc->uc_mcontext.fpregs->swd;scw=uc->uc_mcontext.fpregs->cwd;
 siglongjmp(jump,1);
}
void put(uint64_t x){if(std::fwrite(&x,sizeof x,1,stdout)!=1)std::abort();}
static uint64_t calls=0;
void probe(const std::array<double,8>& a,double guess,unsigned csr,unsigned short cw) {
 std::fesetenv(FE_DFL_ENV); _mm_setcsr(0x1f80); unsigned short stdcw=0x37f;
 asm volatile("fldcw %0"::"m"(stdcw):"memory");
 hit=sc=scsr=sstatus=scw=0;
 int err=123;uint64_t bits=0; unsigned char trapped=0;
 if(!sigsetjmp(jump,1)) {
    errno=123;
    asm volatile("fldcw %0"::"m"(cw):"memory");
    _mm_setcsr(csr);
    const double result=samePressureBus(a,guess);
    err=errno;bits=std::bit_cast<uint64_t>(result);scsr=_mm_getcsr();
    unsigned short swraw,cwraw;asm volatile("fnstsw %0; fnstcw %1":"=m"(swraw),"=m"(cwraw)::"memory");sstatus=swraw;scw=cwraw;
 } else {++traps;trapped=1;err=serr;}
 std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x1f80);
 put(calls++);put(bits);put(trapped);put(hit);put(sc);put(scsr);put(sstatus&0x3f);put(scw);put(err);
}
int main() {
 struct sigaction sa{};sa.sa_sigaction=handler;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);if(sigaction(SIGFPE,&sa,nullptr))return 2;
 for(auto [aa,bb]:std::array<std::array<double,2>,8>{{{0.,.3},{.9,0.},{.4,.45},{1.,.1},{1e-12,.2},{.1,.1000000000001},{.03,.14},{.8,.8}}}) {
  for(unsigned mask=0;mask<256;++mask) {
   std::array<double,8>a{};for(unsigned j=0;j<8;++j)a[j]=(mask>>j)&1?aa:bb;
   for(double g:{0.,.10,.896,3.0,5.0})for(unsigned rc:{0u,0x2000u,0x4000u,0x6000u})for(unsigned fz:{0u,0x40u,0x8000u,0x8040u})probe(a,g,0x1f80u|rc|fz|((mask&1)?0x13u:0u),0x37f);
  }
 }
 constexpr uint64_t special[]{0,0x8000000000000000ull,1,0x8000000000000001ull,0x7ff0000000000001ull,0x7ff0000000000000ull,0x7ff8000001234567ull,0xfff0000000000000ull};
 for(double x:{.02, .93,1.0}) {
   for(unsigned i=0;i<8;++i)for(uint64_t w:special){
     std::array<double,8>a={x,x,.05,.05,x,.05,x,.05}; a[i]=std::bit_cast<double>(w);
     for(unsigned mx:{0x1f80u,0x1facu,0x9fc0u,0x1f00u,0x1e80u,0x1d80u,0x1b80u,0x1780u,0xf80u})
      for(auto cw:{0x37fu,0x33fu,0x77fu,0x37eu})probe(a,.35,mx,static_cast<unsigned short>(cw));
   }
 }
 std::fprintf(stderr,"calls %llu trapped %u\n",(unsigned long long)calls,traps);
}

// Linux x86-64 only: compare actual SSE exceptions between separately built arms.
// No NaN delay position: the historical float-to-index cast has no such contract.
#include "core/Dsp.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <csetjmp>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <immintrin.h>
#include <ucontext.h>
using cs80::DelayLine;
static sigjmp_buf jump;
static volatile sig_atomic_t fault, code, csr, sw, cw, err;
static std::uint64_t records=0, traps=0;

static void caught(int sig, siginfo_t* info, void* context) {
    fault=sig; code=info->si_code; err=errno;
    const auto fp=static_cast<ucontext_t*>(context)->uc_mcontext.fpregs;
    csr=static_cast<sig_atomic_t>(fp->mxcsr); sw=fp->swd; cw=fp->cwd;
    siglongjmp(jump,1);
}
__attribute__((noinline,noclone)) static float probeRead(const DelayLine& line,float d) {
    return line.read(d);
}
static void run(const DelayLine& line, float d,unsigned request) {
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x1f80);
    fault=0; code=0; csr=0; sw=0; cw=0; err=0;
    std::uint32_t bits=0;
    if(!sigsetjmp(jump,1)){
        errno=123; _mm_setcsr(request);
        bits=std::bit_cast<std::uint32_t>(probeRead(line,d));
        csr=static_cast<sig_atomic_t>(_mm_getcsr());err=errno;
        unsigned short cw0, sw0;
        asm volatile("fnstsw %0; fnstcw %1":"=m"(sw0),"=m"(cw0)::"memory");
        sw=sw0;cw=cw0;
    }else{
        ++traps;bits=0;
    }
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x1f80);
    const std::array<std::uint32_t,8> record={static_cast<unsigned>(records++),bits,static_cast<unsigned>(fault),
        static_cast<unsigned>(code),static_cast<unsigned>(csr),static_cast<unsigned>(sw)&63u,
        static_cast<unsigned>(cw),static_cast<unsigned>(err)};
    if(std::fwrite(record.data(),sizeof record,1,stdout)!=1)std::abort();
}
int main(){
    struct sigaction sa{};sa.sa_sigaction=caught;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);
    if(sigaction(SIGFPE,&sa,nullptr))return 2;
    constexpr unsigned inputs[]={0u,0x80000000u,0x1u,0x80000001u,0x7fffffu,0x800000u,
        0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc04567u,0x7f804567u,
        0x3eaaaaabu,0xbee147aeu};
    constexpr unsigned controls[]={0x1f80,0x1fac,0x9fc0,0x9f80,0x1fc0,0x3f80,0x5f80,0x7f80,
        0x1f00,0x1e80,0x1d80,0x1b80,0x1780,0xf80};
    for(int cap:{4,5,17})for(unsigned left:inputs)for(unsigned right:inputs){
        DelayLine line;line.prepare((float)(cap-2),1.0f);
        for(int j=0;j<cap;++j)line.write(std::bit_cast<float>(j&1?right:left));
        const auto delays=std::array<float,9>{1.0f,std::nextafter(1.0f,2.0f),1.25f,1.5f,1.75f,2.0f,3.75f,-INFINITY,INFINITY};
        for(int head=0;head<3;++head){
            for(float d:delays)for(unsigned mask:controls)run(line,d,mask);
            line.write(std::bit_cast<float>(head&1?left:right));
        }
    }
    std::fprintf(stderr,"%llu records; %llu SIGFPE\n",(unsigned long long)records,(unsigned long long)traps);
}

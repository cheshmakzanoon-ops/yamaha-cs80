#include "core/Ig00156Batch.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <signal.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#include <xmmintrin.h>

using cs80::Ig00156Stage;
static_assert(sizeof(Ig00156Stage)==296);
namespace {
struct Owned {
    alignas(Ig00156Stage) std::array<unsigned char,sizeof(Ig00156Stage)> storage{};
    Ig00156Stage* p;
    explicit Owned(const cs80::Ig00156Network& net=cs80::kIg00156LowPassNetwork)
        :p(std::construct_at(reinterpret_cast<Ig00156Stage*>(storage.data()),net)){}
};
struct Record {
    std::array<unsigned char,sizeof(Ig00156Stage)> stage{};
    std::uint32_t csr=0;
    std::uint16_t cw=0,sw=0;
    std::int32_t signal=0,code=0,error=0,hit=-1,token=-1,completion=0;
};
static_assert(sizeof(Record)<4096);
struct Case {const char* name;float oldCutoff,cutoff,q,rate;unsigned csr;unsigned network;bool restore;bool expectedTrap;};
Record record;
Ig00156Stage* currentStage;
int outputFd;
void environment(unsigned csr) {
    unsigned short cw=0x037fU;
    asm volatile("fnclex\n\tfldcw %0"::"m"(cw));_mm_setcsr(csr);errno=29;
}
__attribute__((noinline,noclone)) void stockSet(Ig00156Stage* s,float cutoff,float q,float rate) {
    s->set(cutoff,q,rate,false);
}
void copyState() {
    // The same zeroed placement storage is used in each independent child.
    // Includes every double, raw key, flag and unchanged padding byte.
    const volatile auto* p=reinterpret_cast<const volatile unsigned char*>(currentStage);
    for(std::size_t i=0;i<record.stage.size();i++)record.stage[i]=p[i];
}
[[noreturn]] void deliver() {
    const auto n=write(outputFd,&record,sizeof record);
    _exit(n==sizeof record?0:92);
}
void handler(int signal,siginfo_t* info,void* context) {
    record.signal=signal;record.code=info->si_code;record.error=errno;
    const auto* fp=static_cast<ucontext_t*>(context)->uc_mcontext.fpregs;
    record.csr=fp->mxcsr;record.cw=fp->cwd;record.sw=fp->swd;
    copyState();deliver();
}
void child(const Case& c,bool candidate,int fd) {
    outputFd=fd;alarm(3);struct sigaction sa{};sa.sa_sigaction=handler;sa.sa_flags=SA_SIGINFO;
    sigemptyset(&sa.sa_mask);if(sigaction(SIGFPE,&sa,nullptr))_exit(93);
    environment(0x1f80U);
    auto net=cs80::kIg00156LowPassNetwork;
    if(c.network==1)net=cs80::kIg00156HighPassNetwork;
    if(c.network==2)net.feedbackFarads=1e-320;
    if(c.network==3)net.inputFarads=1e308;
    Owned target(net),donor;
    currentStage=target.p;
    stockSet(target.p,c.oldCutoff,2.f,96000.f);
    if(c.restore)target.p->restoreMemory(target.p->getMemory());
    // A valid donor is available, so an unmasked fallback is not just an empty-cache miss.
    stockSet(donor.p,1000.f,.71f,96000.f);
    cs80::Ig00156LpCoefficientMemo memo;
    cs80::Ig00156LpStockRefresh token;
    environment(0x1fa0U);
    if(cs80::tryReuseIg00156LpCoefficients(*donor.p,200000.f,.71f,96000.f,96000.f,memo,token))_exit(94);
    stockSet(donor.p,200000.f,.71f,96000.f);
    cs80::captureIg00156LpStockRefresh(*donor.p,token,memo);
    if(!memo.valid)_exit(95);
    record={};environment(c.csr);
    if(candidate) {
        record.hit=cs80::tryReuseIg00156LpCoefficients(*target.p,c.cutoff,c.q,c.rate,96000.f,memo,token);
        record.token=token.stage!=nullptr;
    }
    stockSet(target.p,c.cutoff,c.q,c.rate);
    if(candidate)cs80::captureIg00156LpStockRefresh(*target.p,token,memo);
    record.completion=1;record.csr=_mm_getcsr();
    asm volatile("fnstcw %0":"=m"(record.cw));asm volatile("fnstsw %0":"=m"(record.sw));
    record.error=errno;copyState();deliver();
}
bool run(const Case& c,bool candidate,Record& out) {
    int pipeFd[2];if(pipe(pipeFd))return false;
    pid_t pid=fork();if(pid<0)return false;
    if(pid==0){close(pipeFd[0]);child(c,candidate,pipeFd[1]);}
    close(pipeFd[1]);std::size_t got=0;
    while(got<sizeof out){ssize_t n=read(pipeFd[0],reinterpret_cast<char*>(&out)+got,sizeof out-got);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)break;
        got+=static_cast<std::size_t>(n);
    }
    close(pipeFd[0]);int status=0;while(waitpid(pid,&status,0)<0&&errno==EINTR){}
    return got==sizeof out&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
bool same(const Record& a,const Record& b) {
    // Route bookkeeping intentionally differs; every observable field must agree.
    return a.stage==b.stage&&a.csr==b.csr&&a.cw==b.cw&&a.sw==b.sw
        &&a.signal==b.signal&&a.code==b.code&&a.error==b.error&&a.completion==b.completion;
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    FILE* binary=std::fopen(argv[1],"wb");if(!binary)return 3;
    const float snan=std::bit_cast<float>(0x7f800123U),tiny=std::bit_cast<float>(1U);
    const std::array<Case,17> cases{{
        {"precision new high",100000.f,300000.f,1.7f,96000.f,0x0f80U,0,false,true},
        {"precision Q-only",100000.f,100000.f,1.7f,96000.f,0x0f80U,0,false,true},
        {"precision raw return",100000.f,100000.f,2.f,96000.f,0x0f80U,0,false,false},
        {"precision after restore",100000.f,300000.f,1.7f,96000.f,0x0f80U,0,true,true},
        {"invalid ordinary",100000.f,300000.f,1.7f,96000.f,0x1f00U,0,false,false},
        {"denormal ordinary",100000.f,300000.f,1.7f,96000.f,0x1e80U,0,false,false},
        {"divide zero ordinary",100000.f,300000.f,1.7f,96000.f,0x1d80U,0,false,false},
        {"overflow ordinary",100000.f,300000.f,1.7f,96000.f,0x1b80U,0,false,false},
        {"underflow ordinary",100000.f,300000.f,1.7f,96000.f,0x1780U,0,false,false},
        {"invalid signaling raw",100000.f,snan,1.7f,96000.f,0x1f00U,0,false,true},
        {"invalid signaling Q",100000.f,300000.f,snan,96000.f,0x1f00U,0,false,true},
        {"divide by zero rate",100000.f,300000.f,1.7f,0.f,0x1d80U,0,false,true},
        {"denormal Q",100000.f,300000.f,tiny,96000.f,0x1e80U,0,false,true},
        {"overflow custom network",100000.f,300000.f,1.7f,96000.f,0x1b80U,2,false,true},
        {"underflow custom network",100000.f,300000.f,1.7f,96000.f,0x1780U,3,false,true},
        {"precision HP network",100000.f,300000.f,1.7f,96000.f,0x0f80U,1,false,true},
        {"masked valid hit",100000.f,300000.f,1.7f,96000.f,0x1fa0U,0,false,false}
    }};
    unsigned failures=0,traps=0;std::puts("case,csr,signal,code,trap_csr,AA_equal,AB_equal,hit,token,expected_route");
    for(const auto& c:cases) {
        Record a{},aa{},b{};const bool oka=run(c,false,a),okaa=run(c,false,aa),okb=run(c,true,b);
        const bool sameAA=oka&&okaa&&same(a,aa),sameAB=oka&&okb&&same(a,b);
        const bool expected=(a.signal!=0)==c.expectedTrap;
        const bool route=b.hit==(c.csr==0x1fa0U?1:0)&&b.token==0;
        if(!sameAA||!sameAB||!expected||!route)++failures;
        traps+=a.signal!=0;
        std::printf("%s,%04x,%d,%d,%04x,%d,%d,%d,%d,%d\n",c.name,c.csr,a.signal,a.code,a.csr,sameAA,sameAB,b.hit,b.token,expected&&route);
        std::fwrite(&a,sizeof a,1,binary);std::fwrite(&aa,sizeof aa,1,binary);std::fwrite(&b,sizeof b,1,binary);
    }
    std::fclose(binary);std::printf("cases=%zu AA=%zu AB=%zu actual_SIGFPE=%u failures=%u record_bytes=%zu\n",cases.size(),cases.size(),cases.size(),traps,failures,sizeof(Record));
    return failures?1:0;
}

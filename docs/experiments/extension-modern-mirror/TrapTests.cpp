#include <bits/stdc++.h>
#include <xmmintrin.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ucontext.h>
#define private public
#include "core/Engine.h"
#undef private
extern "C" void stock_set(cs80::Engine*,const cs80::modern::ModernConfig*);
extern "C" void stock_mirror(cs80::Engine*,const cs80::Engine*);
using namespace cs80;using namespace cs80::modern;
struct Result { int trapped,code,err,accepted; unsigned csr; unsigned short cw,sw; unsigned long pc; unsigned char state[sizeof(ModernLayer)]; };
static Engine* active;static int output;static int accept=-1;
static void handler(int, siginfo_t* si,void* raw){auto* u=(ucontext_t*)raw;Result r{};r.trapped=1;r.code=si->si_code;r.err=errno;r.accepted=accept;r.csr=u->uc_mcontext.fpregs->mxcsr;r.cw=u->uc_mcontext.fpregs->cwd;r.sw=u->uc_mcontext.fpregs->swd;r.pc=u->uc_mcontext.gregs[REG_RIP];std::memcpy(r.state,&active->modern,sizeof r.state);ssize_t n=write(output,&r,sizeof r);(void)n;_exit(0);}
static Result run(bool candidate,int seed){int fd[2];if(pipe(fd))std::abort();auto child=fork();if(!child){close(fd[0]);output=fd[1];auto s=std::make_unique<Engine>();auto d=std::make_unique<Engine>();active=d.get();unsigned short cw=0x037f;asm volatile("fnclex; fldcw %0"::"m"(cw));_mm_setcsr(0x1f80);ModernConfig c;ModernControls control;control.enabled=true;control.amount[0]=std::bit_cast<float>(uint32_t(0x7f800001));d->modern.setControls(control);if(seed==0){c.slots[0]={ModSource::constant,ModSource::none,ModDestination::pitch,ModTarget::both};}else{c.msegs[0].points[1].curve=std::bit_cast<float>(uint32_t(1));}stock_set(s.get(),&c);struct sigaction sa{};sa.sa_flags=SA_SIGINFO;sa.sa_sigaction=handler;sigemptyset(&sa.sa_mask);sigaction(SIGFPE,&sa,nullptr);errno=EDOM;_mm_setcsr(seed==0?0x1f00:0x1e80);if(candidate){accept=d->tryMirrorModernConfigFrom(*s);if(accept)_exit(7);}else accept=0;stock_mirror(d.get(),s.get());_exit(8);}close(fd[1]);Result r{};size_t count=0;while(count<sizeof r){auto n=read(fd[0],(char*)&r+count,sizeof r-count);if(n<=0)break;count+=n;}close(fd[0]);int status;waitpid(child,&status,0);if(count!=sizeof r||!WIFEXITED(status)||WEXITSTATUS(status)!=0){std::cerr<<"Missing actual trap seed="<<seed<<" cand="<<candidate<<" status="<<status<<" count="<<count<<"\n";std::exit(1);}return r;}
int main(){for(int seed=0;seed<2;++seed){const auto a=run(false,seed),aa=run(false,seed),b=run(true,seed);if(std::memcmp(&a,&aa,sizeof a)||std::memcmp(&a,&b,sizeof a)){std::cerr<<"FAIL trap differential "<<seed<<" pc="<<std::hex<<a.pc<<"/"<<b.pc<<"\n";return 1;}std::cout<<"PASS actual SIGFPE seed="<<seed<<" code="<<a.code<<" csr="<<std::hex<<a.csr<<std::dec<<" all3992statebytes + original PC + FP + errno; A/A passed\n";} }

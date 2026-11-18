#include <bits/stdc++.h>
#include <xmmintrin.h>
#include <sys/wait.h>
#include <unistd.h>
#include <setjmp.h>
#include <ucontext.h>
#define private public
#include "core/Engine.h"
#undef private
#include "core/FactoryPresets.h"
using namespace cs80;namespace s=source_factory;using U=Engine::SharedReadoutUpdate;
struct Case{bool isolated=false,sourcePerformance=true,noise=false,moving=false;int performanceProfile=0;};
#include "frame.inc"
void setup(unsigned csr=0x1fbfu){unsigned short cw=0x37f;asm volatile("fnclex\n\tfldcw %0"::"m"(cw):"memory");_mm_setcsr(csr);}
std::vector<unsigned char> canonical(s::SharedReadout x){__builtin_clear_padding(&x);const auto*p=reinterpret_cast<unsigned char*>(&x);return {p,p+sizeof(x)};}
void require(bool b,const char*s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
void fill(Engine&e,const s::BothRankDrives&d){setup();require(e.updateSourceSharedReadout(d)==U::solved,"fill solved");setup();require(e.updateSourceSharedReadout(d)==U::retained,"fill retained");}
struct TrapFp { unsigned csr; unsigned short cw,sw; };
int trapPipe=-1;const s::SharedReadout* trapOutput=nullptr;sigjmp_buf trapReturn;
void exactWrite(int fd,const void*p,std::size_t n){const auto*b=static_cast<const unsigned char*>(p);while(n){const auto k=write(fd,b,n);if(k<=0)_exit(99);b+=k;n-=std::size_t(k);}}
void trapHandler(int, siginfo_t*, void* raw){
 const auto* uc=static_cast<ucontext_t*>(raw);const auto*f=uc->uc_mcontext.fpregs;if(!f)_exit(98);
 const TrapFp fp{f->mxcsr,f->cwd,f->swd};exactWrite(trapPipe,trapOutput,sizeof(*trapOutput));exactWrite(trapPipe,&fp,sizeof(fp));siglongjmp(trapReturn,1);
}
TrapFp currentFp(){TrapFp f{};asm volatile("stmxcsr %0":"=m"(f.csr)::"memory");asm volatile("fnstcw %0":"=m"(f.cw));asm volatile("fnstsw %0":"=am"(f.sw));return f;}
int main(){setup();Engine e;auto frame=makeFrame(Case{},0);e.prepare(48000);require(e.setControlFrame(frame),"initial frame");s::BothRankDrives d{};for(auto&r:d)for(auto&x:r)x={.25,.75,.25};fill(e,d);
 auto old=canonical(e.sourceShared);e.reset();require(!e.sourceSharedMemo.valid,"reset invalidation");require(canonical(e.sourceShared)==old,"reset visibility");fill(e,d);
 require(e.setControlFrame(frame),"identical frame");require(!e.sourceSharedMemo.valid,"identical frame invalidation");require(canonical(e.sourceShared)==canonical(s::SharedReadout{}),"identical frame clearing");fill(e,d);
 auto wrong=frame;wrong.modelVersion++;old=canonical(e.sourceShared);require(!e.setControlFrame(wrong),"reject frame");require(canonical(e.sourceShared)==old,"reject retention");setup();require(e.updateSourceSharedReadout(d)==U::retained,"reject does not alter dependencies");
 {Engine copied=e;setup();require(copied.updateSourceSharedReadout(d)==U::retained,"copy cache");require(canonical(copied.sourceShared)==canonical(e.sourceShared),"copy readout");}
 e.prepare(44100);require(!e.sourceSharedMemo.valid,"prepare invalidation");fill(e,d);
 e.setParams(frame.effectiveValues);require(!e.sourceSharedMemo.valid,"setParams invalidation");require(canonical(e.sourceShared)==canonical(s::SharedReadout{}),"setParams clearing");
 require(e.setControlFrame(frame),"restore source");fill(e,d);frame.hardwareSources[1]=13;require(e.setControlFrame(frame),"mixed source");require(!e.sourceSharedMemo.valid,"mixed invalidation");fill(e,d);
 frame.effectiveValues[P_parityMode]=1;require(e.setControlFrame(frame),"Pure frame");setup();require(e.updateSourceSharedReadout(d)==U::solved,"Pure scalar");require(!e.sourceSharedMemo.valid,"Pure not cacheable");
 frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0;require(e.setControlFrame(frame),"Legacy frame");setup();require(e.updateSourceSharedReadout(d)==U::solved,"Legacy scalar");require(!e.sourceSharedMemo.valid,"Legacy not cacheable");
 frame.effectiveValues[P_sourcePerformance]=1;require(e.setControlFrame(frame),"source restore");fill(e,d);
 unsigned trapChecks=0;for(unsigned bit:{7u,8u,9u,10u,11u}){setup(0x1fbfu&~(1u<<bit));require(e.updateSourceSharedReadout(d)==U::solved,"unmasked nonprecision fallback");require(!e.sourceSharedMemo.valid,"unmasked not cached");++trapChecks;setup();fill(e,d);}
 // Both arms must still trap for precision, even with a ready cache and a
 // preexisting precision sticky bit. The parent process retains its FP state.
 for(auto policy:{RenderPolicy::sourcePerformance,RenderPolicy::isolation,RenderPolicy::legacy}) for(bool changed:{false,true}){
  e.global.renderPolicy=policy;
  auto trial=d;if(changed)trial[1][7].pressure=.625;
  const auto* start=reinterpret_cast<const unsigned char*>(&e.sourceShared);const std::vector<unsigned char> before(start,start+sizeof(e.sourceShared));
  std::vector<unsigned char> reference;
  for(bool cand:{false,true}){
   int pipes[2];require(pipe(pipes)==0,"pipe");pid_t p=fork();require(p>=0,"fork");
   if(p==0){
    auto child=std::make_unique<Engine>(e);auto&ch=*child;
    close(pipes[0]);trapPipe=pipes[1];trapOutput=&ch.sourceShared;
    struct sigaction sa{};sa.sa_sigaction=trapHandler;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);if(sigaction(SIGFPE,&sa,nullptr))_exit(97);
    if(sigsetjmp(trapReturn,1)==0){setup(0x1fbfu&~(1u<<12));if(cand)(void)ch.updateSourceSharedReadout(trial);else(void)s::evaluateShared(ch.sourceBoundaries,ch.global.resonance,trial,ch.sourceShared);_exit(88);}
    if(cand&&policy!=RenderPolicy::sourcePerformance&&ch.sourceSharedMemo.valid)_exit(96);
    setup();if(cand)(void)ch.updateSourceSharedReadout(trial);else(void)s::evaluateShared(ch.sourceBoundaries,ch.global.resonance,trial,ch.sourceShared);
    const auto after=currentFp();const auto resumed=canonical(ch.sourceShared);exactWrite(pipes[1],&after,sizeof(after));exactWrite(pipes[1],resumed.data(),resumed.size());close(pipes[1]);_exit(77);
   }
   close(pipes[1]);std::vector<unsigned char> receipt;unsigned char buffer[2048];for(;;){auto n=read(pipes[0],buffer,sizeof(buffer));require(n>=0,"read");if(!n)break;receipt.insert(receipt.end(),buffer,buffer+n);}close(pipes[0]);int result=0;require(waitpid(p,&result,0)==p,"wait");require(WIFEXITED(result)&&WEXITSTATUS(result)==77,"precision trap preserved");
   require(receipt.size()==2*sizeof(s::SharedReadout)+2*sizeof(TrapFp),"trap receipt bytes");require(std::equal(before.begin(),before.end(),receipt.begin()),"transactional partial readout at trap");
   if(!cand)reference=receipt;else require(receipt==reference,"fault FP context and resumed readout/flags");++trapChecks;
  }
 }

 setup();std::printf("PASS lifecycle=reset,same_frame,rejected_frame,copy,prepare,setParams,mixed,Pure,Legacy trap_checks=%u\n",trapChecks);
}

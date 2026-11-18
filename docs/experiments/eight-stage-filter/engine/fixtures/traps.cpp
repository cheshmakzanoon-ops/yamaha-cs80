// Diagnostic executable only: the archived normal core is built without access overrides.
#include "observe_engine.h"
#include "core/Ig00156Eight.h"
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <setjmp.h>
#include <signal.h>
#include <ucontext.h>
#include <vector>
#include <xmmintrin.h>
using namespace cs80;
using namespace wsc_staging_audit;
unsigned long long eightAttempts=0,eightHits=0,eightConsumed=0;
extern "C" void filterEightObserve(unsigned success,const Ig00156Eight::Stages&) noexcept {++eightAttempts;eightHits+=success;}
extern "C" void filterEightConsumed() noexcept {++eightConsumed;}
namespace {
Engine* active;float left,right;sigjmp_buf recover;volatile sig_atomic_t seen,code,sigcsr,sigcw,sigsw,sigerr;volatile std::uintptr_t pc;
extern "C" void trap(int sig,siginfo_t*info,void*ctx){ auto* u=static_cast<ucontext_t*>(ctx);auto* f=u->uc_mcontext.fpregs;if(!f)_Exit(82);seen=sig;code=info->si_code;sigcsr=f->mxcsr;sigcw=f->cwd;sigsw=f->swd;sigerr=errno;pc=u->uc_mcontext.gregs[REG_RIP];siglongjmp(recover,1); }
void clean(){std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x1f80);errno=23;}
struct Emit{std::vector<std::uint64_t> out;void operator()(const char*,std::uint64_t x){out.push_back(x);}};
struct FP{unsigned csr;unsigned short cw,sw;int err;}; FP current(){FP x{_mm_getcsr(),0,0,errno};asm volatile("fnstcw %0; fnstsw %1":"=m"(x.cw),"=m"(x.sw)::"memory");return x;}
void save(FILE*f,int id,int step,Engine&e,FP prior){Emit o;o.out.reserve(18000);field(o,"case",id);field(o,"step",step);field(o,"l",left);field(o,"r",right);field(o,"csr",prior.csr);field(o,"cw",prior.cw);field(o,"sw",prior.sw);field(o,"err",prior.err);field(o,"signo",int(seen));field(o,"code",int(code));const auto before=current();engineOrderingState(o,e);const auto after=current();if(before.csr!=after.csr||before.cw!=after.cw||before.sw!=after.sw||before.err!=after.err)_Exit(84);auto n=o.out.size();if(std::fwrite(&n,sizeof n,1,f)!=1||std::fwrite(o.out.data(),8,n,f)!=n)_Exit(83);}
ControlFrame setupFrame(){ControlFrame f;auto&p=f.effectiveValues;p[P_sourcePerformance]=1.;p[P_parityMode]=p[P_voiceMode]=p[P_tuningMode]=0.;p[P_mpeOn]=1.;p[P_mix]=.5f;p[P_drift]=.37f;p[P_voiceDispersion]=.18f;p[P_driftModel]=0.;p[P_analogNoise]=p[P_arpOn]=p[P_kasScanTiming]=p[P_chorusOn]=p[P_ringAmount]=0;p[P_fx1Type]=p[P_fx2Type]=p[P_fx3Type]=0.;f.hardwareSources[0]=f.hardwareSources[1]=0;return f;}
void require(bool ok,const char*why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(3);}}
float publicPcm[2]{};void sample(){publicPcm[0]=left;publicPcm[1]=right;active->render(publicPcm,1);left=publicPcm[0];right=publicPcm[1];}
}
int main(int argc,char**argv){if(argc!=3)return 2;auto out=std::fopen(argv[1],"wbx");auto meta=std::fopen(argv[2],"wx");require(out&&meta,"unique outputs");struct sigaction sa{};sa.sa_sigaction=trap;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);require(sigaction(SIGFPE,&sa,nullptr)==0,"sigaction");
 const int notes[]={36,43,48,55,60,64,67,72};
 const int lanes[]={0,5,15,0,5,15,0,0,0,0,1,2,3,1,2,3,1,2,3,1,2,3};
 for(int kind=0;kind<22;kind++){
  clean(); auto target=std::make_unique<Engine>();active=target.get();active->prepare(48000.);auto frame=setupFrame();require(active->setControlFrame(frame),"frame");active->reset();for(auto n:notes)active->noteOn(n,.8f);require(active->activeVoices()==8,"8 voices");left=right=0.;for(int i=0;i<32;i++)sample();
  // A public physical restore (incl cache invalidation), or a directed test-only
  // poisoned field retaining its genuine cached endpoint. Never edit the candidate.
  const auto payload=std::bit_cast<double>(std::uint64_t{0x7ff0000000000123});auto& o=active->voices[lanes[kind]/2].ranks[lanes[kind]%2].wsc;
  if(kind<6){if((kind%3)==0){auto s=o.getState();if(kind<3)s.residual[0][s.residualCursor]=payload;else s.elapsedSeconds=payload;o.restoreState(s);}else if(kind<3)o.state.residual[std::size_t(kind)%3][o.state.residualCursor]=payload;else o.state.elapsedSeconds=payload;}
  if(kind==9)o.state.residual[0][o.state.residualCursor]=payload;
  // Nonfinite local inputs after 1/2/3 proven ordinary preceding ranks:
  // current tail, elapsed seed, and a dormant tail two cursor positions ahead.
  if(kind>=10&&kind<13)o.state.residual[1][o.state.residualCursor]=payload;
  if(kind>=13&&kind<16)o.state.elapsedSeconds=payload;
  if(kind>=16&&kind<19)o.state.residual[2][(o.state.residualCursor+2)%OscillatorResidualKernel::length]=payload;
  if(kind>=19){
    const double hz=o.getState().heldFrequencyHz,dt=1./96000.;o.reset(0.,hz);
    (void)o.advanceIdle(o.getProfile().resetSecondsNominal-dt,hz,WscControls{});
    auto bound=o.getState();bound.resetRemainingSeconds=dt;o.restoreState(bound);
    require(std::bit_cast<std::uint64_t>(o.getState().resetRemainingSeconds)==std::bit_cast<std::uint64_t>(dt),"exact physical boundary");
  }

  left=std::bit_cast<float>(0x3eaaaaabU);right=-left;clean();seen=code=sigcsr=sigcw=sigsw=sigerr=0;pc=0;save(out,kind,-1,*active,current());clean();
  eightAttempts=eightHits=eightConsumed=0;
  if(sigsetjmp(recover,1)==0){
   if(kind<6)_mm_setcsr(0x1f00u); // SSE invalid only; x87 still masked.
   if(kind==6)_mm_setcsr(0x0f80u); // SSE precision only; genuine prior scalar safeguard case.
   if(kind==7){const unsigned short cw=0x037e;asm volatile("fldcw %0"::"m"(cw):"memory");}
   if(kind==8){std::fesetround(FE_DOWNWARD);_mm_setcsr((_mm_getcsr()&~0x003fu)|0x8040u);}
   sample();
  }
  if(seen){left=publicPcm[0];right=publicPcm[1];}
  if(kind<=8){require(eightAttempts==0 && eightHits==0 && eightConsumed==0,"exceptional gate reached eight helper");std::fprintf(stderr,"case %d eight attempts=0 hits=0 consumed=0\n",kind);}
  FP end=seen?FP{static_cast<unsigned>(sigcsr),static_cast<unsigned short>(sigcw),static_cast<unsigned short>(sigsw),int(sigerr)}:current();clean();save(out,kind,0,*active,end);
  std::fprintf(meta,"%d,%d,%d,%#lx,%#x,%#x,%#x,%d\n",kind,int(seen),int(code),static_cast<unsigned long>(pc),end.csr,end.cw,end.sw,end.err);
  require((kind<7)==bool(seen),"expected real trap / expected control return");
  // Do not restore the oscillator: compare the real surviving partially committed
  // state, directly continued for four ordinary samples under masked exceptions.
  for(int i=1;i<=4;i++){clean();sample();auto after=current();clean();save(out,kind,i,*active,after);}
 }
 std::fclose(meta);std::fclose(out);std::puts("twenty-two cases, seven real SIGFPE plus twelve ordinal/mixed cases; partial state and four masked continuation samples archived");}

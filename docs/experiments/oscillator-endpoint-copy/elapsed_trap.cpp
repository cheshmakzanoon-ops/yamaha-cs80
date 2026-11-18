#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#define private public
#include "core/ReconstructedOscillator.h"
#undef private
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <csetjmp>
#include <dlfcn.h>
#include <ucontext.h>
#include <xmmintrin.h>

using O=cs80::ReconstructedOscillator;
using C=cs80::WscControls;
using V=cs80::WscOutputVolts;
using Tick=V(O::*)(double,const C&) noexcept;
struct Target { void* lib=nullptr; Tick tick{}; std::uintptr_t entry=0; };
Target openTarget(const char* path){
 Target t;t.lib=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!t.lib){std::fprintf(stderr,"dlopen: %s\n",dlerror());std::exit(3);}
 void* symbol=dlsym(t.lib,"_ZN4cs8023ReconstructedOscillator4tickEdRKNS_11WscControlsE");if(!symbol){std::fprintf(stderr,"dlsym: %s\n",dlerror());std::exit(3);}
 // This explicitly targets the x86-64 Itanium C++ ABI used by the frozen objects.
 struct MemberRepresentation { std::uintptr_t address;std::ptrdiff_t adjustment; } repr{reinterpret_cast<std::uintptr_t>(symbol),0};
 static_assert(sizeof(repr)==sizeof(Tick));std::memcpy(&t.tick,&repr,sizeof(t.tick));t.entry=repr.address;return t;
}
sigjmp_buf jumpPoint;
volatile sig_atomic_t signalSeen=0,codeSeen=0;
volatile std::uintptr_t pcSeen=0;
volatile unsigned csrSeen=0,swSeen=0,cwSeen=0;
void onSignal(int sig,siginfo_t* info,void* ctx){
 auto* u=static_cast<ucontext_t*>(ctx);signalSeen=sig;codeSeen=info->si_code;pcSeen=u->uc_mcontext.gregs[REG_RIP];
 csrSeen=u->uc_mcontext.fpregs->mxcsr;swSeen=u->uc_mcontext.fpregs->swd;cwSeen=u->uc_mcontext.fpregs->cwd;
 siglongjmp(jumpPoint,1);
}
struct Interrupt { int signal=0,code=0,error=0;std::uintptr_t offset=0;unsigned csr=0,sw=0,cw=0;bool returned=false; };
Interrupt interrupt(Target& t,O& o){
 std::fenv_t saved{};std::fegetenv(&saved);const unsigned csr=_mm_getcsr();const int err=errno;
 std::fesetenv(FE_DFL_ENV);std::feclearexcept(FE_ALL_EXCEPT);errno=19;signalSeen=0;codeSeen=0;
 bool returned=false;
 if(sigsetjmp(jumpPoint,1)==0){feenableexcept(FE_INVALID);(void)(o.*t.tick)(0.,C{});returned=true;}
 Interrupt out{int(signalSeen),int(codeSeen),errno,pcSeen-t.entry,csrSeen,swSeen,cwSeen,returned};
 std::fesetenv(&saved);_mm_setcsr(csr);errno=err;return out;
}
bool bits(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
bool values(const V& a,const V& b){return bits(a.sawPin5,b.sawPin5)&&bits(a.pulsePin7,b.pulsePin7)&&bits(a.sinePin2,b.sinePin2);}
bool whole(const O& a,const O& b){return std::memcmp(&a,&b,sizeof(O))==0;}
void finiteResiduals(O::State& s){for(auto& branch:s.residual)for(double& x:branch){auto u=std::bit_cast<std::uint64_t>(x);if((u&0x7ff0000000000000ULL)==0x7ff0000000000000ULL&&(u&0xfffffffffffffULL))x=.5;}}
int main(int argc,char** argv){
 if(argc!=3)return 3;auto stock=openTarget(argv[1]),candidate=openTarget(argv[2]);
 struct sigaction sa{};sa.sa_sigaction=onSignal;sigemptyset(&sa.sa_mask);sa.sa_flags=SA_SIGINFO;sigaction(SIGFPE,&sa,nullptr);
 alignas(O) static unsigned char memory[3][sizeof(O)]{};O& seed=*new(memory[0])O;O& a=*new(memory[1])O;O& b=*new(memory[2])O;
 seed.reset(.37,0.);auto clean=seed.getState();clean.residual={};seed.restoreState(clean);a=seed;b=seed;
 auto x=(a.*stock.tick)(0.,C{}),y=(b.*candidate.tick)(0.,C{});
 if(!values(x,y)||!whole(a,b)){std::puts("prewarm mismatch");return 4;}
 seed=a;clean=seed.getState();int mismatches=0,traps=0,directReplayFailures=0;
 for(int validCache=0;validCache<2;++validCache){
  a=seed;b=seed;
  auto s=clean;s.elapsedSeconds=std::bit_cast<double>(std::uint64_t(0x7ff0000000000123ULL));
  if(validCache){
   // Test-only exceptional seed, retaining the otherwise valid cached endpoint.
   a.state.elapsedSeconds=s.elapsedSeconds;b.state.elapsedSeconds=s.elapsedSeconds;
  }else{a.restoreState(s);b.restoreState(s);}
  if(!whole(a,b)){std::puts("pretrap mismatch");return 4;}
  const auto ta=interrupt(stock,a),tb=interrupt(candidate,b);traps+=(ta.signal==SIGFPE)+(tb.signal==SIGFPE);
  const bool same=whole(a,b),nodes=std::memcmp(&a.endNodes,&b.endNodes,sizeof(a.endNodes))==0,valid=a.endNodesValid==b.endNodesValid;
  int differing=0;for(std::size_t i=0;i<sizeof(O);++i)differing+=(memory[1][i]!=memory[2][i]);
  std::printf("seed_valid_cache=%d stock_sig=%d code=%d pc=+0x%zx csr=%x sw=%x cw=%x errno=%d candidate_sig=%d code=%d pc=+0x%zx csr=%x sw=%x cw=%x errno=%d whole_equal=%d differing_bytes=%d nodes20_equal=%d validity_equal=%d value128=%a/%a value136=%a/%a\n",validCache,ta.signal,ta.code,std::size_t(ta.offset),ta.csr,ta.sw,ta.cw,ta.error,tb.signal,tb.code,std::size_t(tb.offset),tb.csr,tb.sw,tb.cw,tb.error,same,differing,nodes,valid,a.endNodes.constant.sinePin2,b.endNodes.constant.sinePin2,a.endNodes.ungatedValue.sawPin5,b.endNodes.ungatedValue.sawPin5);
  mismatches+=!same;
  const auto sa=a.getState(),sb=b.getState();
  // Continue with an ordinary public control change and masked exceptions.
  // Do not restore first: the still-valid cache itself is the interrupted continuation.
  int directMismatch=0,directDiagnosticMismatch=0;for(int n=0;n<4;++n){const auto va=(a.*stock.tick)(0.,C{.1,false,true}),vb=(b.*candidate.tick)(0.,C{.1,false,true});directMismatch+=!values(va,vb);directDiagnosticMismatch+=std::memcmp(&a.getDiagnostics(),&b.getDiagnostics(),sizeof(O::Diagnostics))!=0;if(n==0)std::printf("seed_valid_cache=%d first_continuation_event_count=%zu/%zu first_event_before_saw=%a/%a\n",validCache,a.getDiagnostics().eventCount,b.getDiagnostics().eventCount,a.getDiagnostics().events[0].before.sawPin5,b.getDiagnostics().events[0].before.sawPin5);}
  directReplayFailures+=directMismatch+directDiagnosticMismatch;
  auto safeA=sa,safeB=sb;safeA.elapsedSeconds=safeB.elapsedSeconds=0.;a.restoreState(safeA);b.restoreState(safeB);
  int ownMismatch=0;for(int n=0;n<4;++n){const auto va=(a.*stock.tick)(0.,C{}),vb=(b.*candidate.tick)(0.,C{});ownMismatch+=!values(va,vb);}
  a.restoreState(clean);b.restoreState(clean);
  int commonMismatch=0;for(int n=0;n<4;++n){const auto va=(a.*stock.tick)(0.,C{}),vb=(b.*candidate.tick)(0.,C{});commonMismatch+=!values(va,vb);}
  std::printf("seed_valid_cache=%d direct_public_continuation_output_mismatches=%d direct_public_diagnostic_mismatches=%d own_restored_replay_output_mismatches=%d common_restored_replay_output_mismatches=%d\n",validCache,directMismatch,directDiagnosticMismatch,ownMismatch,commonMismatch);
 }
 std::printf("traps=%d whole_object_interrupt_mismatches=%d direct_public_continuation_mismatches=%d bytes_per_object=%zu\n",traps,mismatches,directReplayFailures,sizeof(O));return mismatches||directReplayFailures?1:0;
}

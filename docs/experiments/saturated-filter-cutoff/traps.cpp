#include <signal.h>
#include <setjmp.h>
#include <ucontext.h>
#define main audit_normal_main
#include "differential.cpp"
#undef main
static sigjmp_buf jumpPoint;static volatile sig_atomic_t trapped=0;static volatile unsigned trappedMx=0;std::FILE* trapsFile;
static void handler(int, siginfo_t* info,void* context){trapped=info->si_code;auto*u=static_cast<ucontext_t*>(context);trappedMx=u->uc_mcontext.fpregs->mxcsr;_mm_setcsr(0x1f80);asm volatile("fninit":::"memory");siglongjmp(jumpPoint,1);}
void trappingSet(S&s,const char*label,float fc,float q,float sr,unsigned csr){trapped=0;trappedMx=0;setup(csr);const auto hitsBefore=audit_cache_hits;if(sigsetjmp(jumpPoint,1)==0)set(s,fc,q,sr,false);std::fprintf(trapsFile,"%llu,%s,signal=%d,trap_mxcsr=%x,new_hits=%llu\n",(unsigned long long)eventNo,label,int(trapped),unsigned(trappedMx),audit_cache_hits-hitsBefore);dump(label,s);}
int main(int argc,char**argv){if(argc!=4)return 2;file=std::fopen(argv[1],"wb");indexFile=std::fopen(argv[2],"w");trapsFile=std::fopen(argv[3],"w");if(!file||!indexFile||!trapsFile)return 3;struct sigaction sa{};sa.sa_sigaction=handler;sigemptyset(&sa.sa_mask);sa.sa_flags=SA_SIGINFO;sigaction(SIGFPE,&sa,nullptr);const auto saved=_mm_getcsr();
struct C{const char*label;float fc,q,sr;unsigned csr;bool restore;};
const float snan=std::bit_cast<float>(0x7f800001u),tiny=std::bit_cast<float>(1u);
const std::array<C,13> cases{{
 {"precision new high",100001,2,96000,0x1f80&~0x1000,false},
 {"precision Q-only",100000,3,96000,0x1f80&~0x1000,false},
 {"precision exact early return",100000,2,96000,0x1f80&~0x1000,false},
 {"precision after restore",100001,2,96000,0x1f80&~0x1000,true},
 {"invalid mask ordinary",100001,2,96000,0x1f80&~0x80,false},
 {"denormal mask ordinary",100001,2,96000,0x1f80&~0x100,false},
 {"zero-div mask ordinary",100001,2,96000,0x1f80&~0x200,false},
 {"overflow mask ordinary",100001,2,96000,0x1f80&~0x400,false},
 {"underflow mask ordinary",100001,2,96000,0x1f80&~0x800,false},
 {"invalid signaling raw",snan,2,96000,0x1f80&~0x80,false},
 {"invalid signaling Q",100001,snan,96000,0x1f80&~0x80,false},
 {"divide by zero rate",100001,2,0,0x1f80&~0x200,false},
 {"denormal Q",100001,tiny,96000,0x1f80&~0x100,false}
}};
for(const auto&n:nets)for(float prelude:{1000.f,100000.f})for(const auto&c:cases){++constructorNo;setup(0x1f80);S s(n);runSet(s,"prelude",prelude,2,96000,false,0x1fa0);if(c.restore){auto m=s.getMemory();setup(0x1fa0);s.restoreMemory(m);dump("restore",s);}trappingSet(s,c.label,c.fc,c.q,c.sr,c.csr);runSet(s,"recover high new raw",100003,2,96000,false,0x1fa0);runSet(s,"recover hit",100004,2,96000,false,0x1fa0);runProcess(s,"post trap process",.17f,0x1fa0);}
_mm_setcsr(saved);std::fclose(file);std::fclose(indexFile);std::fclose(trapsFile);std::fprintf(stderr,"trap_events=%llu constructors=%u explicit_hit_witnesses=%llu\n",(unsigned long long)eventNo,constructorNo,audit_cache_hits);}

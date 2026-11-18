#define main unused_trace_main
#include "trace.cpp"
#undef main
#include <setjmp.h>
#include <ucontext.h>
#include <unistd.h>
Engine* trappedEngine=nullptr;int trapFd=-1;sigjmp_buf jumpBack;
void writeRaw(const void* data,std::size_t n){const auto*p=static_cast<const unsigned char*>(data);while(n){const auto done=write(trapFd,p,n);if(done<=0)_exit(90);p+=done;n-=std::size_t(done);}}
void publicTrap(int, siginfo_t*,void*raw){const auto*ctx=static_cast<ucontext_t*>(raw);const auto*f=ctx->uc_mcontext.fpregs;if(!f)_exit(91);const unsigned csr=f->mxcsr;const unsigned short cw=f->cwd,sw=f->swd;writeRaw(&csr,sizeof(csr));writeRaw(&cw,sizeof(cw));writeRaw(&sw,sizeof(sw));const auto&r=trappedEngine->sourceShared;
#define W(field) writeRaw(&r.field,sizeof(r.field))
 W(qVolts);W(q);W(levelPinVolts);W(levelGain);W(qBusVolts);W(levelBusVolts);W(masterResVolts);W(saturatedVoices);W(masterResSaturated);
#undef W
 siglongjmp(jumpBack,1);
}
void check(bool p,const char*s){if(!p){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
int main(int argc,char**argv){if(argc!=2)return 2;struct sigaction sa{};sa.sa_sigaction=publicTrap;sa.sa_flags=SA_SIGINFO;sigemptyset(&sa.sa_mask);check(sigaction(SIGFPE,&sa,nullptr)==0,"sigaction");
for(int kind=0;kind<3;kind++){
 mode(3);auto engine=std::make_unique<Engine>();auto&e=*engine;Case c;auto frame=makeFrame(c,0);e.prepare(48000);check(e.setControlFrame(frame),"initial");std::array<float,128>audio{};e.render(audio.data(),64);
#if CS80_RESEARCH_SHARED_READOUT_RETENTION
 check(e.sourceSharedMemo.valid,"cache ready");
#endif
 auto supplied=frame;if(kind!=1)supplied.effectiveValues[P_volume]=std::bit_cast<float>(std::uint32_t(0x7f800001));
 const std::string file=std::string(argv[1])+"-"+std::to_string(kind)+".bin";Out out{std::fopen(file.c_str(),"wb")};check(out.f,"open");out.pod(kind);state(out,e);std::fflush(out.f);trapFd=fileno(out.f);trappedEngine=&e;
 if(sigsetjmp(jumpBack,1)==0){unsigned short cw=0x37f;asm volatile("fnclex\n\tfldcw %0"::"m"(cw):"memory");_mm_setcsr(0x1fbfu&~(kind==1?(1u<<12):(1u<<7)));errno=91;if(kind==0)e.setParams(supplied.effectiveValues);else(void)e.setControlFrame(supplied);check(false,"expected public trap");}
 mode(3);state(out,e);
#if CS80_RESEARCH_SHARED_READOUT_RETENTION
 check(e.sourceSharedMemo.valid==(kind==2),"ordered invalidation/rejected-frame preservation");
#endif
 check(e.setControlFrame(frame),"post-trap correction");mode(3);e.render(audio.data(),64);fp(out);out.pod(audio);state(out,e);check(std::fclose(out.f)==0,"close");std::printf("PASS public_trap=%d partial_state_and_continuation\n",kind);
}
}

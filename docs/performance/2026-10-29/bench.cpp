#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <time.h>
#include <sched.h>
#include <xmmintrin.h>
using cs80::Engine;
using namespace cs80;
using Clock=std::chrono::steady_clock;
static double cpuSeconds(){ timespec t{}; clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t); return t.tv_sec+1e-9*t.tv_nsec; }
static double percentile(const std::vector<double>& s,double q){ return s[std::min(s.size()-1,(size_t)std::ceil(q*(s.size()-1)))]; }
static ParamValues fixture(bool isolation,int scenario){
 ParamValues p;
 p[P_parityMode]=isolation?1.f:0.f; p[P_mix]=.5f; p[P_volume]=.5f;
 p[P_footMode]=scenario?1.f:0.f; p[P_outputLevel]=1.f;
 p[P_chorusOn]=1.f; p[P_chorusDepth]=.5f; p[P_chorusSpeed]=.55f;
 p[P_ringSpeed]=77.f; p[P_ringDepth]=.2f; p[P_ringAmount]=.15f;
 p[P_subOriginalMode]=1.f; p[P_subOriginalSpeed]=4.2f;
 p[P_subVco]=.07f; p[P_subVcf]=.12f; p[P_subVca]=.08f;
 for(int r=0;r<2;++r){
  p[chParam(r,CP_saw)]=1.f; p[chParam(r,CP_pulse)]=1.f;
  p[chParam(r,CP_sine)]=.2f; p[chParam(r,CP_noise)]=.02f;
  p[chParam(r,CP_pwmDepth)]=.35f; p[chParam(r,CP_pwmSpeed)]=.6f+.2f*r;
  p[chParam(r,CP_hpfOn)]=p[chParam(r,CP_lpfOn)]=1.f;
  p[chParam(r,CP_hpfCutoff)]=120.f; p[chParam(r,CP_lpfCutoff)]=3600.f;
  p[chParam(r,CP_hpfRes)]=.3f; p[chParam(r,CP_lpfRes)]=.5f;
  p[chParam(r,CP_vA)]=.01f; p[chParam(r,CP_vD)]=.1f; p[chParam(r,CP_vS)]=.75f;
  p[chParam(r,CP_fA)]=.01f; p[chParam(r,CP_fD)]=.1f;
  p[chParam(r,CP_vLevel)]=.7f; p[chParam(r,CP_vcfLevel)]=.7f;
  p[chParam(r,CP_pan)]=r?.3f:-.3f;
 }
 return p;
}
struct Case{int rate,block,scenario;};
void run(Case c,bool iso,int samples){
 Engine e; e.prepare(c.rate); auto p=fixture(iso,c.scenario); e.setParams(p); e.setExpression(.8f);
 for(int n:{36,43,48,55,60,64,67,72}) e.noteOn(n,.8f);
 std::vector<float> out(2*c.block); std::vector<double> elapsed,eventElapsed;
 elapsed.reserve(samples); eventElapsed.reserve(samples/16+1);
 const int warmup=(int)std::ceil(.5*c.rate/c.block);
 for(int i=0;i<warmup;++i){ e.setParams(p); e.render(out.data(),c.block); }
 if(e.activeVoices()!=8){std::fprintf(stderr,"Expected 8 voices before timing\n");std::exit(2);}
 const double cpuStart=cpuSeconds(); int bad=0,misses=0; double checksum=0,peak=0;
 const double deadlineUs=1e6*c.block/c.rate;
 for(int i=0;i<samples;++i){
  const bool event=c.scenario==2 && i%16==0;
  float expression=.8f;
  if(c.scenario){ double phase=std::fmod((double)(i*c.block)/c.rate*.75,1.); expression=.15f+.8f*(float)(1.-std::abs(2.*phase-1.)); }
  if(event) p[P_outputLevel]=1.f-p[P_outputLevel];
  const auto start=Clock::now();
  e.setParams(p); // match the plugin's per-block parameter application, no JUCE wrapper included
  if(c.scenario)e.setExpression(expression);
  e.render(out.data(),c.block);
  const auto end=Clock::now();
  const double us=std::chrono::duration<double,std::micro>(end-start).count();
  elapsed.push_back(us); if(event)eventElapsed.push_back(us); if(us>deadlineUs)++misses;
  for(float x:out){if(!std::isfinite(x))++bad; checksum+=x; peak=std::max(peak,(double)std::abs(x));}
 }
 const double cpu=cpuSeconds()-cpuStart;
 if(e.activeVoices()!=8 || bad){std::fprintf(stderr,"Invalid result voices=%d bad=%d\n",e.activeVoices(),bad);std::exit(3);}
 const double sum=std::accumulate(elapsed.begin(),elapsed.end(),0.);
 std::sort(elapsed.begin(),elapsed.end()); std::sort(eventElapsed.begin(),eventElapsed.end());
 const char* scenario[]={"sustain_exp","moving_wah","moving_wah_highlow"};
 std::printf("%d,%d,%d,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%d,%.6f,%zu,%.3f,%.3f,%.8g,%.8g\n",c.rate,c.block,iso,scenario[c.scenario],samples,elapsed.front(),percentile(elapsed,.5),percentile(elapsed,.95),percentile(elapsed,.99),elapsed.back(),sum/(samples*deadlineUs),percentile(elapsed,.5)/deadlineUs,percentile(elapsed,.99)/deadlineUs,misses,cpu,eventElapsed.size(),eventElapsed.empty()?0:percentile(eventElapsed,.5),eventElapsed.empty()?0:eventElapsed.back(),checksum,peak);
 std::fflush(stdout);
}
int main(int argc,char**argv){
 int n=argc>1?std::atoi(argv[1]):768; bool quick=argc>2;
 // Mirror juce::ScopedNoDenormals on x86 for this process only; no platform/security setting changed.
 const unsigned old=_mm_getcsr();_mm_setcsr(old|0x8040);
 cpu_set_t cpus;CPU_ZERO(&cpus); int affinity=sched_getaffinity(0,sizeof(cpus),&cpus);
 std::fprintf(stderr,"steady_clock_is_steady=%d MXCSR_before=0x%x MXCSR_timed=0x%x affinity_status=%d visible_cpus=%d scheduler=%d compiler=%s\n",Clock::is_steady,old,_mm_getcsr(),affinity,affinity==0?CPU_COUNT(&cpus):-1,sched_getscheduler(0),__VERSION__);
 std::puts("rate,block,isolation,scenario,callbacks,min_us,median_us,p95_us,p99_us,max_us,mean_rtf,median_rtf,p99_rtf,deadline_exceedances,thread_cpu_s_including_untimed_checks,highlow_events,event_median_us,event_max_us,checksum,peak");
 if(argc>2 && std::strcmp(argv[2],"confirm192")==0){for(int s=0;s<3;++s){bool first=s%2==0;run({192000,64,s},first,n);run({192000,64,s},!first,n);}return 0;}
 if(quick){run({48000,256,1},false,n);run({48000,256,1},true,n);return 0;}
 std::vector<Case> cases;for(int r:{44100,48000,96000,192000})for(int b:{64,256,1024})for(int s=0;s<3;++s)cases.push_back({r,b,s});
 std::mt19937 rng(800081);std::shuffle(cases.begin(),cases.end(),rng);
 int i=0;for(auto c:cases){bool first=(i++%2)==0;run(c,first,n);run(c,!first,n);}
 _mm_setcsr(old);
}

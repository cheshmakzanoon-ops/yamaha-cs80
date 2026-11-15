#define main batch_regression_main
#include "/workspace/scratch/ee447c520988/cs80-restored/audio-root-hybrid/tests/FilterBatchCapTests.cpp"
#undef main
#include "/workspace/scratch/ee447c520988/cs80-restored/audio-root-hybrid/tests/FilterIdleFallbackBaseline.h"
#include <chrono>
#include <algorithm>
#if defined(__GNUC__) || defined(__clang__)
#define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE
#endif
using Old=baseline::Ig00156Stage;
template<class Stage> using Group=std::array<Stage,4>;
template<class Stage> using Outputs=std::array<typename Stage::Output,4>;
template<class Stage> NOINLINE void scalar(Group<Stage>&s,const B::Inputs&in,Outputs<Stage>&out){for(int j=0;j<4;++j)out[j]=s[j].process(in[j]);}
NOINLINE bool vectorStep(Group<S>&s,const B::Inputs&in,B::Outputs&out,B::Kernel k){return processIg00156Four(ptr(s),in,out,k);}
Old oldStage(const C&c){Old s(c.lp?baseline::kIg00156LowPassNetwork:baseline::kIg00156HighPassNetwork);Old::Memory m{};auto&v=c.v;m.z={v[4],v[5],v[6],v[7]};m.v={v[8],v[9],v[10],v[11]};m.slope1=v[12];m.slope2=v[13];m.halfIntegrators=v[14];m.halfCouplings=v[15];s.restoreMemory(m);s.set(v[1],v[2],v[0],false);return s;}
Group<Old> oldFresh(){Group<Old>a{Old(baseline::kIg00156HighPassNetwork),Old(baseline::kIg00156LowPassNetwork),Old(baseline::kIg00156HighPassNetwork),Old(baseline::kIg00156LowPassNetwork)};for(auto&s:a)s.set(1000,1,96000);return a;}
volatile double sink=0;
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream f(argv[1]);if(!f)return 2;std::string l;std::vector<C>cases;while(getline(f,l)){if(l.rfind("CAP,",0))continue;std::stringstream ss(l);std::vector<std::string>r;std::string t;while(getline(ss,t,','))r.push_back(t);if(r.size()!=50)return 2;if(r[2]=="0")continue;C c{r[3]=="1",{}};for(int j=6;j<50;++j)c.v.push_back(strtod(r[j].c_str(),nullptr));cases.push_back(c);}if(cases.size()!=512)return 2;auto k=B::selectKernel();if(!k)return 77;
puts("scenario,round,variant,groups,ns_per_group,vector_commits,checksum");
for(int scenario=0;scenario<6;++scenario){
std::vector<Group<S>> original,work;std::vector<Group<Old>> old,oldWork;std::vector<B::Inputs> inputs;std::vector<B::Outputs> result(512),reference(512);std::vector<Outputs<Old>> oldResult(512);
for(auto&c:cases){auto a=fresh();auto b=oldFresh();B::Inputs in{};for(int lane=0;lane<4;++lane)if(scenario==4||lane==scenario){a[lane]=restore(c);b[lane]=oldStage(c);in[lane]=float(c.v[3]);}original.push_back(a);old.push_back(b);inputs.push_back(in);}work=original;oldWork=old;
// Untimed parity and observed successful-commit control, before cost measurement.
for(int j=0;j<512;++j){auto a=original[j],b=a;scalar(b,inputs[j],reference[j]);bool used=vectorStep(a,inputs[j],result[j],k);if(used!=(scenario==5))return 4;for(int lane=0;lane<4;++lane)if(!out(reference[j][lane],result[j][lane])||!mem(a[lane],b[lane]))return 5;}
for(int round=-3;round<31;++round)for(int order=0;order<3;++order){int variant=(order+(round+3)%3)%3;unsigned commits=0;work=original;oldWork=old; // restore outside timer
 auto start=std::chrono::steady_clock::now();
 for(int j=0;j<512;++j){if(variant==0)scalar(oldWork[j],inputs[j],oldResult[j]);else if(variant==1)scalar(work[j],inputs[j],result[j]);else commits+=vectorStep(work[j],inputs[j],result[j],k);}
 auto end=std::chrono::steady_clock::now();double sum=0;
 for(int j=0;j<512;++j)for(int lane=0;lane<4;++lane){double v=variant==0?oldResult[j][lane].bp:result[j][lane].bp;if(!std::isfinite(v))return 6;sum+=v;}sink=sum;
 if(round>=0)printf("%d,%d,%s,512,%.9f,%u,%.17g\n",scenario,round,variant==0?"baseline_scalar":variant==1?"repaired_scalar":"repaired_wrapper",std::chrono::duration<double,std::nano>(end-start).count()/512.,commits,sum);
 }
}return 0;}

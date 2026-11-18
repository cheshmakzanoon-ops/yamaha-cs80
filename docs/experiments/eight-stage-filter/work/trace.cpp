#include "fixture.inc"
#include "core/Ig00156Eight.h"
using namespace cs80;
std::uint64_t attempts{},hits{},consumed{},bad{};Engine* current{};
extern "C" void filterEightObserve(unsigned ok,const Ig00156Eight::Stages& s) noexcept {
 ++attempts;hits+=ok;bool distinct=true;for(unsigned i=0;i<8;++i)for(unsigned j=0;j<i;++j)distinct&=s[i]!=s[j];bad+=!distinct;
}
extern "C" void filterEightConsumed() noexcept {++consumed;}
namespace trace {
template<class T>void put(FILE*f,const T&x){if(fwrite(&x,sizeof x,1,f)!=1)std::abort();}
template<class T,std::size_t N>void put(FILE*f,const std::array<T,N>&x){for(const auto&a:x)put(f,a);}
void putFp(FILE*f,lane_cost::Fp x){put(f,x.csr);put(f,x.cw);put(f,x.sw);put(f,x.error);}
void stage(FILE*f,const Ig00156Stage&s){
 for(double x:{s.net.sourceOhms,s.net.groundOhms,s.net.inputFarads,s.net.feedbackOhms,s.net.feedbackFarads,s.net.integratorFarads,s.net.feedforwardOhms,s.gs,s.gg,s.gf,s.gTotal,s.inputScale,s.feedforwardHz,s.kHalf,s.kHalfCoupling,s.gmPart,s.ffPart,s.qSiemensValue,s.kiGs,s.xFromFb,s.xFromIn,s.xFromU,s.xFromBp,s.inFromX,s.invA11,s.zIn,s.zFb,s.zBp,s.zLp,s.vIn,s.vFb,s.vBp,s.vLp,s.slope1,s.slope2})put(f,x);
 for(float x:{s.lastCutoff,s.lastQ,s.lastRate})put(f,x);put(f,std::uint8_t(s.lastUnwarped));put(f,std::uint8_t(s.cutoffCoefficientsValid));
}
void osc(FILE*f,const ReconstructedOscillator&s){const auto&st=s.getState();for(double x:{st.charge,st.resetRemainingSeconds,st.inputBaselineVolts,st.triangleBaselineVolts,st.heldFrequencyHz,st.heldThresholdVolts,st.elapsedSeconds})put(f,x);put(f,std::uint8_t(st.sawEnabled));put(f,std::uint8_t(st.pulseEnabled));put(f,st.residual);put(f,st.residualCursor);put(f,st.resetCount);put(f,std::uint8_t(s.endNodesValid));const auto&n=s.endNodes;for(double x:{n.pin7,n.u,n.uSlope,n.triangle,n.triangleSlope,n.w,n.wSlope,n.triangleGain,n.value.sawPin5,n.value.pulsePin7,n.value.sinePin2,n.slope.sawPin5,n.slope.pulsePin7,n.slope.sinePin2,n.constant.sawPin5,n.constant.pulsePin7,n.constant.sinePin2,n.ungatedValue.sawPin5,n.ungatedValue.pulsePin7,n.ungatedValue.sinePin2})put(f,x);
 const auto&d=s.getDiagnostics();put(f,std::uint8_t(d.invalidControl));put(f,std::uint8_t(d.frequencyLimited));put(f,std::uint8_t(d.eventCapacityExceeded));put(f,d.eventCount);for(double x:{d.rawPin7Volts,d.coupledInputVolts,d.trianglePin3Volts,d.shaperInputVolts,d.raw.sawPin5,d.raw.pulsePin7,d.raw.sinePin2})put(f,x);for(size_t i=0;i<d.eventCount;++i){const auto&e=d.events[i];for(double x:{e.secondsFromSampleStart,e.absoluteSeconds,e.charge,e.before.sawPin5,e.before.pulsePin7,e.before.sinePin2,e.after.sawPin5,e.after.pulsePin7,e.after.sinePin2})put(f,x);put(f,e.kind);put(f,std::uint8_t(e.resetActive));}}
void capture(FILE*f,Engine&e,const lane_cost::Pcm&pcm){put(f,pcm);put(f,e.activeVoices());put(f,std::bit_cast<std::uint32_t>(e.audioRng));put(f,std::bit_cast<std::uint32_t>(e.modRng));for(const auto&x:e.idleNoise)put(f,std::bit_cast<std::uint32_t>(x));put(f,std::bit_cast<std::uint32_t>(e.modern.noteRng));for(const auto&v:e.voices)for(const auto&r:v.ranks){stage(f,r.hpStage);stage(f,r.lpStage);put(f,r.idleRefs.hpRate);put(f,r.idleRefs.lpRate);osc(f,r.wsc);}for(int rank=0;rank<2;++rank){auto x=e.getSourceCutoffReadout(rank);put(f,x.pinVolts);put(f,x.effectiveCv);put(f,x.loadedBusVolts);put(f,x.saturatedVoices);for(int voice=0;voice<8;++voice){auto a=e.getRankInfo(voice,rank);for(float z:{a.hpfHz,a.lpfHz,a.filterEnvelope,a.ampEnvelope,a.oscillatorHz,a.keyVolts,a.hpfQ,a.lpfQ})put(f,z);put(f,std::uint8_t(a.hpfEnabled));put(f,std::uint8_t(a.lpfEnabled));}}
 auto x=e.getSourceSharedReadout();put(f,x.q);put(f,x.qVolts);put(f,x.qBusVolts);put(f,x.levelBusVolts);put(f,x.levelPinVolts);put(f,x.levelGain);put(f,x.masterResVolts);put(f,x.saturatedVoices);put(f,std::uint8_t(x.masterResSaturated));
}
}

namespace census {
struct Event { unsigned block,n,ok,passes,updates; std::uint64_t masks; unsigned ids[8],inputs[8]; std::uint64_t before[8]; };
static Event events[20000]; static unsigned used=0,block=0; static bool failed=false;
static Ig00156Stage* owners[32];
std::uint64_t digest(const Ig00156Stage& s) noexcept {
 std::uint64_t h=1469598103934665603ull;
 auto add=[&](const auto& x){ const auto* b=reinterpret_cast<const unsigned char*>(&x);for(unsigned j=0;j<sizeof(x);++j){h^=b[j];h*=1099511628211ull;}};
#define W(field) add(s.field)
 W(net.sourceOhms);W(net.groundOhms);W(net.inputFarads);W(net.feedbackOhms);W(net.feedbackFarads);W(net.integratorFarads);W(net.feedforwardOhms);
 W(gs);W(gg);W(gf);W(gTotal);W(inputScale);W(feedforwardHz);W(kHalf);W(kHalfCoupling);W(gmPart);W(ffPart);W(qSiemensValue);W(kiGs);W(xFromFb);W(xFromIn);W(xFromU);W(xFromBp);W(inFromX);W(invA11);W(zIn);W(zFb);W(zBp);W(zLp);W(vIn);W(vFb);W(vBp);W(vLp);W(slope1);W(slope2);W(lastCutoff);W(lastQ);W(lastRate);W(lastUnwarped);W(cutoffCoefficientsValid);
#undef W
 return h;
}
void bind(Engine& e){unsigned i=0;for(auto& v:e.voices)for(auto& rank:v.ranks){owners[i++]=&rank.hpStage;owners[i++]=&rank.lpStage;}}
void write(const char* path){FILE*f=fopen(path,"wx");if(!f)std::abort();for(unsigned i=0;i<used;++i){const auto&e=events[i];fprintf(f,"{\"block\":%u,\"n\":%u,\"ok\":%u,\"passes\":%u,\"updates\":%u,\"masks\":%llu,\"lanes\":[",e.block,e.n,e.ok,e.passes,e.updates,(unsigned long long)e.masks);for(unsigned j=0;j<e.n;++j)fprintf(f,"%s[%u,%u,%llu]",j?",":"",e.ids[j],e.inputs[j],(unsigned long long)e.before[j]);fprintf(f,"]}\n");}fclose(f);}
}
extern "C" unsigned workBegin(unsigned n,Ig00156Stage* const* stages,const float* inputs) noexcept {
 if(census::block<1024 || census::block>1040)return ~0u;
 if(census::used>=20000){census::failed=true;return ~0u;}
 unsigned i=census::used++;auto&e=census::events[i];e.block=census::block;e.n=n;
 for(unsigned j=0;j<n;++j){unsigned id=0;while(id<32&&census::owners[id]!=stages[j])++id;if(id==32)census::failed=true;e.ids[j]=id;e.inputs[j]=std::bit_cast<unsigned>(inputs[j]);e.before[j]=census::digest(*stages[j]);}
 return i;
}
extern "C" bool workEnd(unsigned i,bool ok,unsigned passes,unsigned updates,std::uint64_t masks) noexcept {
 if(i!=~0u){auto&e=census::events[i];e.ok=ok;e.passes=passes;e.updates=updates;e.masks=masks;}return ok;
}
int main(int argc,char**argv){if(argc!=3)return 2;fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=0;lane_cost::Work w(1);current=w.engine.get();census::bind(*current);FILE*f=fopen(argv[1],"wbx");if(!f)return 3;for(unsigned b=0;b<1041;++b){census::block=b;if(b==1024)attempts=hits=consumed=bad=0;auto before=lane_cost::fp();w.step(b);auto after=lane_cost::fp();fenv_t env;fegetenv(&env);int error=errno;trace::put(f,b);trace::putFp(f,before);trace::putFp(f,after);trace::capture(f,*w.engine,w.pcm);fesetenv(&env);errno=error;if(!lane_cost::same(after,lane_cost::fp()))return 4;if(b==1039)printf("window attempts=%llu hits=%llu consumed=%llu bad=%llu\n",(unsigned long long)attempts,(unsigned long long)hits,(unsigned long long)consumed,(unsigned long long)bad);}fclose(f);if(census::failed)return 5;census::write(argv[2]);printf("with_continuation attempts=%llu hits=%llu consumed=%llu bad=%llu\n",(unsigned long long)attempts,(unsigned long long)hits,(unsigned long long)consumed,(unsigned long long)bad);}

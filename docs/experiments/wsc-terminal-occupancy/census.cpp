#define main unused_shipping_benchmark_main
#include "fixture/source_factory_benchmark.cpp"
#undef main
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstring>
#include <memory>

namespace census {
using U=std::uint64_t;
template<class T> void put(FILE*f,const T& x){if(std::fwrite(&x,sizeof x,1,f)!=1)std::abort();}
template<class T,std::size_t N> void put(FILE*f,const std::array<T,N>&a){for(const auto&x:a)put(f,x);}
U bits(double x){return std::bit_cast<U>(x);}
bool positive(U x){return x>0&&x<0x7ff0000000000000ULL;}
bool zero(U x){return (x&0x7fffffffffffffffULL)==0;}
struct Fp {unsigned csr;unsigned short cw,sw;int error;};
Fp fp(){Fp f;asm volatile("stmxcsr %0; fnstcw %1; fnstsw %2":"=m"(f.csr),"=m"(f.cw),"=m"(f.sw)::"memory");f.error=errno;return f;}
void putFp(FILE*f,Fp x){put(f,x.csr);put(f,x.cw);put(f,x.sw);put(f,x.error);}
bool sameFp(Fp a,Fp b){return a.csr==b.csr&&a.cw==b.cw&&a.sw==b.sw&&a.error==b.error;}
struct Pre {
 U requested{},threshold{},heldFrequency{},heldThreshold{},charge{},reset{},rate{},dt{};
 std::array<U,14> profile{};
 unsigned controls{},heldControls{},csr{},cw{};
 bool prepared{},valid{},unchangedFrequency{},unchangedControls{},cheapGate{};
};
struct Lane {
 Pre pre{};bool called{},done{},event{},refresh{},gates{},firstSpanFull{},firstNoReset{},failed{};
 unsigned iterations{},immediateReset{},branch{},physical{},inReset{},lastNode{};
 unsigned uRoots{},turnRoots{},shapeRoots{},rootFallback{};
 std::array<unsigned,4> records{},zeroRecords{};
 std::size_t diagnosticEvents{};
};
#define FIELDS(X) \
 X(samples) X(ticks) X(completed_ticks) X(duplicate_ticks) X(active0) X(active1) X(active2) X(active3) X(complete4) \
 X(prepared_lanes) X(pre_valid_lanes) X(unchanged_frequency_lanes) X(unchanged_controls_lanes) X(cheap_gate_lanes) X(first_stock_span_full_lanes) \
 X(control_event_lanes) X(control_refresh_lanes) X(gates_changed_lanes) X(iterations) X(immediate_resets) X(branch_steps) X(physical_steps) X(in_reset_steps) X(failed_lanes) \
 X(u_root_decisions) X(turn_decisions) X(shaper_root_decisions) X(root_fallback_decisions) X(reset_start_records) X(reset_end_records) X(branch_records) X(control_records) \
 X(zero_reset_start_records) X(zero_reset_end_records) X(zero_branch_records) X(zero_control_records) X(diag_zero_lanes) X(diag_zero_but_boundary_lanes) \
 X(no_internal_boundary_lanes) X(no_any_boundary_lanes) X(stable_no_boundary_lanes) X(terminal_ordinary_nodes_lanes) \
 X(same_rate_packs) X(same_profile_packs) X(same_gate_pair_packs) X(same_threshold_packs) X(same_requested_frequency_packs) X(same_nonfrequency_controls_packs) \
 X(pre_valid_packs) X(unchanged_frequency_packs) X(unchanged_controls_packs) X(cheap_gate_packs) X(first_stock_span_full_packs) X(cheap_and_first_span_full_packs) \
 X(no_internal_boundary_packs) X(no_any_boundary_packs) X(stable_no_boundary_packs) X(strong_unchanged_stable_packs) X(cheap_gate_and_no_boundary_packs) \
 X(terminal_ordinary_nodes_packs) X(terminal_after_earlier_work_packs) X(terminal_same_profile_rate_packs) X(diag_zero_packs) X(diag_zero_but_boundary_packs) \
 X(masked_nearest_packs) X(ftz_daz_on_packs) X(endpoints_invalid_packs)
enum Field{
#define ENUM(n) n,
 FIELDS(ENUM)
#undef ENUM
 fieldCount
};
const char* names[]={
#define NAME(n) #n,
 FIELDS(NAME)
#undef NAME
};
using Counts=std::array<U,fieldCount>;
std::array<std::array<Counts,4>,2> counts{};
std::array<Lane,16> lanes{};int phase=-1,current=-1;U duplicates=0;
constexpr const char* labels[]={"factory_held","factory_moving","panel_held","panel_moving","pure_held","legacy_held","factory_quiet","partial_three","release_wake"};
bool noInternal(const Lane&x){return x.done&&!x.failed&&x.iterations==1&&!x.immediateReset&&!x.branch&&!x.physical;}
bool noAny(const Lane&x){return noInternal(x)&&!x.event;}
bool stable(const Lane&x){return noAny(x)&&!x.refresh&&x.pre.valid;}
bool terminal(const Lane&x){return x.done&&!x.failed&&x.lastNode==1;}
}

namespace cs80 {
struct WscCensusAccess {
 static census::Pre pre(const ReconstructedOscillator& s,double hz,const WscControls& c){
  using namespace census;Pre p;const auto& q=s.profile;
  const double profile[]={q.resetSecondsNominal,q.referenceHzNominal,q.resetVoltsNominal,q.resetExcessVoltsNominal,q.couplingSecondsNominal,q.triangleSpanVoltsNominal,q.sliceVoltsInferred,q.pulseHalfSpanVoltsInferred,q.sineGainVoltsInferred,q.shaperOrdinateInferred[0],q.shaperOrdinateInferred[1],q.shaperOrdinateInferred[2],q.shaperOrdinateInferred[3],q.shaperOrdinateInferred[4]};
  for(unsigned i=0;i<14;++i)p.profile[i]=bits(profile[i]);
  p.requested=bits(hz);p.threshold=bits(c.pulseThresholdVolts);p.heldFrequency=bits(s.state.heldFrequencyHz);p.heldThreshold=bits(s.state.heldThresholdVolts);
  p.charge=bits(s.state.charge);p.reset=bits(s.state.resetRemainingSeconds);p.rate=bits(s.rate);p.dt=bits(s.dt);
  p.controls=unsigned(c.sawEnabled)|(unsigned(c.pulseEnabled)<<1);p.heldControls=unsigned(s.state.sawEnabled)|(unsigned(s.state.pulseEnabled)<<1);
  p.prepared=s.basis!=nullptr;p.valid=s.endNodesValid;p.unchangedFrequency=p.requested==p.heldFrequency;
  p.unchangedControls=p.controls==p.heldControls&&p.threshold==p.heldThreshold;
  const auto f=fp();p.csr=f.csr;p.cw=f.cw;
  p.cheapGate=p.prepared&&p.valid&&p.unchangedFrequency&&p.unchangedControls&&zero(p.reset)&&positive(p.charge)&&positive(p.requested)
    &&positive(p.rate)&&(p.csr&0x6000U)==0&&(p.csr&0x1f80U)==0x1f80U&&(p.cw&0x003fU)==0x003fU&&(p.cw&0x0c00U)==0;
  return p;
 }
 static void dumpOsc(FILE*f,const ReconstructedOscillator&s){
  using namespace census;const auto p=pre(s,s.state.heldFrequencyHz,{s.state.heldThresholdVolts,s.state.sawEnabled,s.state.pulseEnabled});
  for(U x:p.profile)put(f,x);
  for(double x:{s.rate,s.dt,s.converterGain,s.rampHalf,s.negligibleCornerScale})put(f,bits(x));
  put(f,std::uint8_t(s.basis!=nullptr));put(f,std::uint8_t(s.endNodesValid));
  const auto& st=s.state;
  for(double x:{st.charge,st.resetRemainingSeconds,st.inputBaselineVolts,st.triangleBaselineVolts,st.heldFrequencyHz,st.heldThresholdVolts,st.elapsedSeconds})put(f,bits(x));
  put(f,std::uint8_t(st.sawEnabled));put(f,std::uint8_t(st.pulseEnabled));
  for(const auto& row:st.residual)for(double x:row)put(f,bits(x));
  put(f,U(st.residualCursor));put(f,st.resetCount);
  const auto& n=s.endNodes;
  for(double x:{n.pin7,n.u,n.uSlope,n.triangle,n.triangleSlope,n.w,n.wSlope,n.triangleGain,
       n.value.sawPin5,n.value.pulsePin7,n.value.sinePin2,n.slope.sawPin5,n.slope.pulsePin7,n.slope.sinePin2,
       n.constant.sawPin5,n.constant.pulsePin7,n.constant.sinePin2,n.ungatedValue.sawPin5,n.ungatedValue.pulsePin7,n.ungatedValue.sinePin2})put(f,bits(x));
  const auto&d=s.diagnostics;
  put(f,std::uint8_t(d.invalidControl));put(f,std::uint8_t(d.frequencyLimited));put(f,std::uint8_t(d.eventCapacityExceeded));put(f,U(d.eventCount));
  for(double x:{d.rawPin7Volts,d.coupledInputVolts,d.trianglePin3Volts,d.shaperInputVolts,d.raw.sawPin5,d.raw.pulsePin7,d.raw.sinePin2})put(f,bits(x));
  for(std::size_t i=0;i<d.eventCount;++i){const auto&e=d.events[i];
   for(double x:{e.secondsFromSampleStart,e.absoluteSeconds,e.charge,e.before.sawPin5,e.before.pulsePin7,e.before.sinePin2,e.after.sawPin5,e.after.pulsePin7,e.after.sinePin2})put(f,bits(x));
   put(f,e.kind);put(f,std::uint8_t(e.resetActive));}
 }
 static void dump(FILE*f,const Engine&e){for(const auto&v:e.voices)for(const auto&r:v.ranks)dumpOsc(f,r.wsc);}
};
extern "C" void wsc_census_sample_begin() noexcept {
 if(census::phase<0)return;census::lanes={};census::current=-1;census::duplicates=0;
}
extern "C" void wsc_census_lane(int lane) noexcept {census::current=census::phase>=0?lane:-1;}
extern "C" void wsc_census_pre(const ReconstructedOscillator&s,double hz,const WscControls&c) noexcept {
 using namespace census;if(phase<0||current<0)return;auto&x=lanes[unsigned(current)];duplicates+=x.called;x.called=true;x.pre=WscCensusAccess::pre(s,hz,c);
}
extern "C" void wsc_census_flag(unsigned kind,unsigned flags) noexcept {
 using namespace census;if(phase<0||current<0)return;auto&x=lanes[unsigned(current)];
 switch(kind){
 case 10:x.event=flags&1;x.refresh=flags&2;x.gates=flags&4;break;
 case 11:++x.iterations;break;
 case 12:++x.immediateReset;break;
 case 14:case 17:x.failed=true;break;
 case 15:x.branch+=bool(flags&1);x.physical+=bool(flags&2);x.inReset+=bool(flags&4);break;
 case 16:x.lastNode=flags;break;
 case 30:++x.uRoots;break;case 31:++x.turnRoots;break;case 32:++x.shapeRoots;break;case 33:++x.rootFallback;break;
 case 21:case 22:case 24:case 28:{unsigned i=kind==21?0:kind==22?1:kind==24?2:3;++x.records[i];x.zeroRecords[i]+=flags==0;break;}
 default:break;
 }
}
extern "C" void wsc_census_span(double span,double physical,bool reset) noexcept {
 using namespace census;if(phase<0||current<0)return;auto&x=lanes[unsigned(current)];
 if(x.iterations==1){x.firstSpanFull=bits(span)==x.pre.dt&&bits(physical)>x.pre.dt;x.firstNoReset=!reset;}
}
extern "C" void wsc_census_post(const ReconstructedOscillator&s) noexcept {
 using namespace census;if(phase<0||current<0)return;auto&x=lanes[unsigned(current)];x.done=true;x.diagnosticEvents=s.getDiagnostics().eventCount;
}
extern "C" void wsc_census_sample_end() noexcept {
 using namespace census;if(phase<0)return;
 for(unsigned g=0;g<4;++g){auto&c=counts[unsigned(phase)][g];++c[samples];unsigned active=0;
  bool allDone=true,pValid=true,uf=true,uc=true,cg=true,fs=true,ni=true,na=true,st=true,te=true,dz=true,anyBoundary=false,earlyWork=false,fpMask=true,ftz=true;
  bool rate=true,profile=true,gates=true,threshold=true,freq=true;
  const auto&p0=lanes[g*4].pre;
  for(unsigned i=0;i<4;++i){const auto&x=lanes[g*4+i];if(!x.called){allDone=false;continue;}++active;++c[ticks];c[completed_ticks]+=x.done;
   const auto&p=x.pre;c[prepared_lanes]+=p.prepared;c[pre_valid_lanes]+=p.valid;c[unchanged_frequency_lanes]+=p.unchangedFrequency;c[unchanged_controls_lanes]+=p.unchangedControls;c[cheap_gate_lanes]+=p.cheapGate;c[first_stock_span_full_lanes]+=x.firstSpanFull&&x.firstNoReset;
   c[control_event_lanes]+=x.event;c[control_refresh_lanes]+=x.refresh;c[gates_changed_lanes]+=x.gates;c[iterations]+=x.iterations;c[immediate_resets]+=x.immediateReset;c[branch_steps]+=x.branch;c[physical_steps]+=x.physical;c[in_reset_steps]+=x.inReset;c[failed_lanes]+=x.failed;
   c[u_root_decisions]+=x.uRoots;c[turn_decisions]+=x.turnRoots;c[shaper_root_decisions]+=x.shapeRoots;c[root_fallback_decisions]+=x.rootFallback;
   for(unsigned k=0;k<4;++k){c[reset_start_records+k]+=x.records[k];c[zero_reset_start_records+k]+=x.zeroRecords[k];}
   const bool boundary=x.event||x.branch||x.physical||x.immediateReset;
   c[diag_zero_lanes]+=x.diagnosticEvents==0;c[diag_zero_but_boundary_lanes]+=x.diagnosticEvents==0&&boundary;
   c[no_internal_boundary_lanes]+=noInternal(x);c[no_any_boundary_lanes]+=noAny(x);c[stable_no_boundary_lanes]+=stable(x);c[terminal_ordinary_nodes_lanes]+=terminal(x);
   allDone&=x.done;pValid&=p.valid;uf&=p.unchangedFrequency;uc&=p.unchangedControls;cg&=p.cheapGate;fs&=x.firstSpanFull&&x.firstNoReset;
   ni&=noInternal(x);na&=noAny(x);st&=stable(x);te&=terminal(x);dz&=x.diagnosticEvents==0;anyBoundary|=boundary;
   earlyWork|=x.refresh||x.event||x.iterations>1||x.immediateReset||x.branch||x.physical;
   fpMask&=(p.csr&0x6000U)==0&&(p.csr&0x1f80U)==0x1f80U&&(p.cw&0x0c3fU)==0x003fU;ftz&=(p.csr&0x8040U)==0x8040U;
   rate&=p.rate==p0.rate&&p.dt==p0.dt;profile&=p.profile==p0.profile;gates&=p.controls==p0.controls;threshold&=p.threshold==p0.threshold;freq&=p.requested==p0.requested;
  }
  c[active0+active]++;c[duplicate_ticks]+=duplicates;
  if(active!=4||!allDone)continue;
  c[same_rate_packs]+=rate;c[same_profile_packs]+=profile;c[same_gate_pair_packs]+=gates;c[same_threshold_packs]+=threshold;c[same_requested_frequency_packs]+=freq;c[same_nonfrequency_controls_packs]+=gates&&threshold;
  c[pre_valid_packs]+=pValid;c[unchanged_frequency_packs]+=uf;c[unchanged_controls_packs]+=uc;c[cheap_gate_packs]+=cg;c[first_stock_span_full_packs]+=fs;c[cheap_and_first_span_full_packs]+=cg&&fs;
  c[no_internal_boundary_packs]+=ni;c[no_any_boundary_packs]+=na;c[stable_no_boundary_packs]+=st;c[strong_unchanged_stable_packs]+=st&&uf&&uc;c[cheap_gate_and_no_boundary_packs]+=cg&&na;
  c[terminal_ordinary_nodes_packs]+=te;c[terminal_after_earlier_work_packs]+=te&&earlyWork;c[terminal_same_profile_rate_packs]+=te&&profile&&rate;c[diag_zero_packs]+=dz;c[diag_zero_but_boundary_packs]+=dz&&anyBoundary;
  c[masked_nearest_packs]+=fpMask;c[ftz_daz_on_packs]+=ftz;c[endpoints_invalid_packs]+=!pValid;
 }
 current=-1;
}
}

namespace census {
void publicTrace(FILE*f,Engine&e,const std::array<float,128>&pcm){
 for(float x:pcm)put(f,std::bit_cast<unsigned>(x));put(f,e.activeVoices());
 for(int rank=0;rank<2;++rank){const auto s=e.getSourceToneInfo(rank);put(f,s.selector);put(f,int(s.route));const auto x=e.getSourceCutoffReadout(rank);
  put(f,x.pinVolts);put(f,x.effectiveCv);put(f,x.loadedBusVolts);put(f,x.saturatedVoices);
  for(int voice=0;voice<8;++voice){const auto a=e.getRankInfo(voice,rank);
   for(float v:{a.hpfHz,a.lpfHz,a.filterEnvelope,a.ampEnvelope,a.oscillatorHz,a.keyVolts,a.hpfQ,a.lpfQ})put(f,v);
   put(f,std::uint8_t(a.hpfEnabled));put(f,std::uint8_t(a.lpfEnabled));}
 }
 const auto x=e.getSourceSharedReadout();
 put(f,x.q);put(f,x.qVolts);put(f,x.qBusVolts);put(f,x.levelBusVolts);put(f,x.levelPinVolts);put(f,x.levelGain);put(f,x.masterResVolts);put(f,x.saturatedVoices);put(f,std::uint8_t(x.masterResSaturated));
 for(int i=0;i<8;++i){const auto v=e.getVoiceInfo(i);put(f,std::uint8_t(v.active));put(f,v.note);put(f,v.channel);put(f,std::uint8_t(v.held));put(f,std::uint8_t(v.pedalHeld));
  for(float a:{v.velocity,v.pressure,v.bend,v.ribbonRatio,v.touchA,v.touchB,v.touchC,v.timbre})put(f,a);}
 put(f,int(e.getRenderPolicy()));put(f,e.getOversamplingFactor());put(f,e.getSourceCompileCount());
}
void run(unsigned which,const char*prefix,bool perturb){
 Case config;config.rate=48000;config.block=64;config.selector=(which==2||which==3)?13:0;config.isolated=which==4;config.sourcePerformance=which!=4&&which!=5;
 const bool moving=which==1||which==3;config.performanceProfile=moving?1:0;
 auto frame=makeFrame(config,config.selector);frame.effectiveValues[P_drift]=.37f;frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_driftModel]=0.f;
 auto e=std::make_unique<Engine>();e->prepare(config.rate);if(!e->setControlFrame(frame))std::abort();e->reset();
 const unsigned initial=which==6?0:which==7?3:8;
 for(unsigned v=0;v<initial;++v)e->noteOn(notes[v],.8f,moving?int(v)+2:1);
 const auto path=std::string(prefix);FILE*f=std::fopen((path+".trace.bin").c_str(),"wbx");FILE*sf=std::fopen((path+".wsc.bin").c_str(),"wbx");if(!f||!sf)throw std::runtime_error("output exists or unavailable");
 std::array<float,128> pcm{};
 for(unsigned b=0;b<1536;++b){
  phase=b<256?0:b>=1024?1:-1;
  if(which==8){if(b==1040)frame.effectiveValues[P_parityMode]=1;if(b==1080){frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0;}if(b==1120)frame.effectiveValues[P_sourcePerformance]=1;}
  // Authentic shipping ordering: normal frame EVERY block, BEFORE new MIDI.
  if(!e->setControlFrame(frame))std::abort();
  if(moving&&b%4==0){const unsigned v=(b/4)%8;const float x=float((b*13)%97)/96.f;e->channelPressure(int(v)+2,x);e->pitchBend(int(v)+2,x*.1f);e->controlChange(74,x,int(v)+2);}
  if(which==8){if(b==192)for(int n:notes)e->noteOff(n);if(b==600)for(unsigned v=0;v<8;++v)e->noteOn(notes[v],.72f);if(b==1280)e->reset();if(b==1360)for(unsigned v=0;v<3;++v)e->noteOn(notes[v],.72f);}
  const auto before=fp();e->render(pcm.data(),64);const auto after=fp();
  fenv_t env{};if(fegetenv(&env))std::abort();const int savedErrno=errno;
  put(f,b);putFp(f,before);putFp(f,after);publicTrace(f,*e,pcm);
  if(b<4||b%32==0||b==191||b==192||b==599||b==600||b==1023||b==1024||b==1279||b==1280||b==1359||b==1360||b==1535){put(sf,b);putFp(sf,after);WscCensusAccess::dump(sf,*e);}
  if(fesetenv(&env))std::abort();errno=savedErrno;if(!sameFp(after,fp()))std::abort();
 }
 phase=-1;if(std::fclose(f)||std::fclose(sf))std::abort();
 // Known reporting-only negative: change one already-computed output count,
 // after every render and trace write. Never changes a hook decision or DSP.
 if(perturb)++counts[1][0][terminal_ordinary_nodes_packs];
 std::printf("scene,window,group");for(const char*n:names)std::printf(",%s",n);std::puts("");
 for(unsigned w=0;w<2;++w)for(unsigned g=0;g<4;++g){std::printf("%s,%s,%u",labels[which],w?"late":"onset",g);for(U x:counts[w][g])std::printf(",%llu",(unsigned long long)x);std::puts("");}
}
}
int main(int argc,char**argv){if(argc<3||argc>4)return 2;unsigned scene=9;for(unsigned i=0;i<9;++i)if(!std::strcmp(argv[1],census::labels[i]))scene=i;if(scene==9)return 2;
 if(fesetenv(FE_DFL_ENV))return 3;_mm_setcsr(0x9fc0U);errno=0;census::run(scene,argv[2],argc==4&&!std::strcmp(argv[3],"perturb"));return 0;}

// Same counter implementations as the already-frozen production occupancy census.
#include "frozen-observer-prefix.inc"
#ifndef SEMANTIC_OBSERVED
#define SEMANTIC_OBSERVED 0
#endif
namespace {
void env(){if(fesetenv(FE_DFL_ENV))std::abort();_mm_setcsr(0x9fc0U);errno=29;}
void check(bool x,const char*label){if(!x){std::fprintf(stderr,"FAILED: %s\n",label);std::exit(1);}}
void caseRun(bool exactResetEnd,FILE*trace){
 using namespace census;
 phase=-1;current=-1;env();
 std::array<ReconstructedOscillator,4> oscillators;
 const std::array<double,4> hz{220.,330.,440.,550.};
 const WscControls controls{};
 for(unsigned i=0;i<4;++i){
  check(oscillators[i].prepare(96000.,OscillatorNominalProfile{}),"prepare");
  oscillators[i].reset(.25,hz[i]);
  (void)oscillators[i].tick(hz[i],controls); // genuine ordinary prehistory/end cache
 }
 if(exactResetEnd){
  auto& o=oscillators[0];o.reset(0.,hz[0]);
  const double dt=1./96000.;
  // Build the reset trajectory with the public stock physical idle advance,
  // then use the public restore API to place its remaining duration exactly dt.
  (void)o.advanceIdle(o.getProfile().resetSecondsNominal-dt,hz[0],controls);
  auto state=o.getState();state.resetRemainingSeconds=dt;
  o.restoreState(state);
  check(std::bit_cast<std::uint64_t>(o.getState().resetRemainingSeconds)==std::bit_cast<std::uint64_t>(dt),"exact restored boundary");
 }
 counts={};phase=0;current=-1;env();const auto before=fp();
 std::array<WscOutputVolts,4> output{};
 cs80::wsc_census_sample_begin();
 for(unsigned i=0;i<4;++i){cs80::wsc_census_lane(int(i));output[i]=oscillators[i].tick(hz[i],controls);cs80::wsc_census_lane(-1);}
 cs80::wsc_census_sample_end();const auto after=fp();phase=-1;
 const auto&c=counts[0][0];
 if constexpr(SEMANTIC_OBSERVED){
  check(c[ticks]==4&&c[completed_ticks]==4&&c[complete4]==1,"one real complete pack");
  check(c[terminal_ordinary_nodes_packs]==(exactResetEnd?0U:1U),"terminal pack classification");
  check(c[terminal_ordinary_nodes_lanes]==(exactResetEnd?3U:4U),"terminal lane classification");
  if(exactResetEnd){check(lanes[0].lastNode==3&&lanes[0].physical==1,"actual physical terminal tag");check(c[reset_end_records]==1,"actual reset end record");}
  else check(c[physical_steps]==0&&c[branch_steps]==0,"ordinary control no boundary");
 }
 put(trace,std::uint8_t(exactResetEnd));putFp(trace,before);putFp(trace,after);
 for(unsigned i=0;i<4;++i){for(double x:{output[i].sawPin5,output[i].pulsePin7,output[i].sinePin2})put(trace,bits(x));WscCensusAccess::dumpOsc(trace,oscillators[i]);}
 std::printf("%s,ticks=%llu,complete4=%llu,terminal_packs=%llu,terminal_lanes=%llu,physical_steps=%llu,reset_end_records=%llu,lane0_last_node=%u,lane0_diag_events=%zu\n",
  exactResetEnd?"one_exact_reset_end":"four_ordinary",(unsigned long long)c[ticks],(unsigned long long)c[complete4],(unsigned long long)c[terminal_ordinary_nodes_packs],(unsigned long long)c[terminal_ordinary_nodes_lanes],(unsigned long long)c[physical_steps],(unsigned long long)c[reset_end_records],lanes[0].lastNode,oscillators[0].getDiagnostics().eventCount);
}
}
int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=std::fopen(argv[1],"wbx");if(!f)return 3;caseRun(false,f);caseRun(true,f);return std::fclose(f)?4:0;}

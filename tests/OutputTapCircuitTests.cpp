// Conditional source-informed circuit qualification, NOT measured hardware
// equivalence. Independent stamped transient/active oracles; existing offline
// OutputTaps AC reference remains separate and keeps its unknown defaults.
#include "core/OutputTapCircuit.h"
#include "reference/OutputTaps.h"
#include <array>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <random>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace { bool countAllocations=false; std::size_t allocations=0; }
void* operator new(std::size_t n) { if(countAllocations)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
using Tap=cs80::OutputTapCircuit;
using Triple=cs80::OriginalOutputCircuit;
using Mode=Tap::Mode;
using Kind=Tap::LoadKind;
using C=std::complex<double>;
namespace ref=cs80::reference::output_taps;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* label){++checks;if(!ok){if(failures<30)std::cerr<<label<<'\n';++failures;}}
void near(double a,double b,const char* label,double rel=3e-10,double abs=2e-12){check(std::abs(a-b)<=abs+rel*std::max(std::abs(a),std::abs(b)),label);}
void near(C a,C b,const char* label,double rel=3e-8,double abs=3e-10){check(std::abs(a-b)<=abs+rel*std::max(std::abs(a),std::abs(b)),label);}
std::array<double,11> physical(const Tap::Nodes& n){return {n.capacitorInputVolts,n.capacitorVolts,n.commonVolts,n.highTapVolts,n.lowTapVolts,n.jackVolts,n.sourceAmps,n.highSeriesAmps,n.lowSeriesAmps,n.shuntAmps,n.loadAmps};}
std::array<C,11> physical(const ref::Result& n){return {n.capacitorInputVolts,n.capacitorVolts,n.commonVolts,n.highTapVolts,n.lowTapVolts,n.jackVolts,n.sourceAmps,n.highSeriesAmps,n.lowSeriesAmps,n.shuntAmps,n.loadAmps};}
bool identical(const Tap::Nodes& a,const Tap::Nodes& b){return physical(a)==physical(b)&&a.audibleJackVolts==b.audibleJackVolts;}

// Exact rollback/partition comparisons execute through the SAME uninlined
// runtime call site. Compiler specialization/FMA choices at separate inline
// call sites must not masquerade as changed model state on other architectures.
#if defined(_MSC_VER)
#define OUTPUT_TEST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define OUTPUT_TEST_NOINLINE __attribute__((noinline))
#else
#define OUTPUT_TEST_NOINLINE
#endif
OUTPUT_TEST_NOINLINE Tap::Nodes runtimeProcess(Tap& model,double input) noexcept { return model.process(input); }
OUTPUT_TEST_NOINLINE Triple::Outputs runtimeProcess(Triple& model,double left,double right) noexcept { return model.process(left,right); }
#undef OUTPUT_TEST_NOINLINE

// Small independent long-double, row-scaled partial-pivot matrix solver. Used
// only by tests; no reduced Rp/T/K coefficients from the implementation enter it.
template<std::size_t N> using Matrix=std::array<std::array<long double,N>,N>;
template<std::size_t N> using Vector=std::array<long double,N>;
template<std::size_t N> Vector<N> solve(Matrix<N> a,Vector<N> b){
 for(std::size_t i=0;i<N;++i){long double scale=0;for(auto x:a[i])scale=std::max(scale,std::abs(x));if(scale==0)throw std::runtime_error("empty oracle row");for(auto& x:a[i])x/=scale;b[i]/=scale;}
 for(std::size_t k=0;k<N;++k){std::size_t pivot=k;for(std::size_t i=k+1;i<N;++i)if(std::abs(a[i][k])>std::abs(a[pivot][k]))pivot=i;if(std::abs(a[pivot][k])<1e-25L)throw std::runtime_error("singular oracle");std::swap(a[k],a[pivot]);std::swap(b[k],b[pivot]);
  for(std::size_t i=k+1;i<N;++i){auto q=a[i][k]/a[k][k];for(std::size_t j=k;j<N;++j)a[i][j]-=q*a[k][j];b[i]-=q*b[k];}}
 Vector<N> x{};for(std::size_t r=N;r-->0;){auto v=b[r];for(std::size_t c=r+1;c<N;++c)v-=a[r][c]*x[c];x[r]=v/a[r][r];}return x;
}
template<std::size_t N> void branch(Matrix<N>& a,int i,int j,long double g){a[i][i]+=g;a[j][j]+=g;a[i][j]-=g;a[j][i]-=g;}
template<std::size_t N> void fixed(Matrix<N>& a,Vector<N>& b,int node,long double v){a[node].fill(0);a[node][node]=1;b[node]=v;}
template<std::size_t N> void resistors(Matrix<N>& a,Vector<N>& b,const Tap::Profile& p,Mode mode,long double u){
 branch(a,1,2,1.0L/p.highSeriesOhms);branch(a,1,3,1.0L/p.lowSeriesOhms);a[3][3]+=1.0L/p.lowShuntOhms;
 if(p.sourceOhms>0){a[0][0]+=1.0L/p.sourceOhms;b[0]+=u/p.sourceOhms;}
 else fixed(a,b,0,u);
 const int load=mode==Mode::high?2:3;
 if(p.load.kind==Kind::finite)a[load][load]+=1.0L/p.load.ohms;
 else if(p.load.kind==Kind::shorted)fixed(a,b,load,0);
}
// Hold the actual capacitor voltage with an ideal voltage source, solve its
// current after a topology event, then build a trapezoidal companion. This
// independently rebases old-end current instead of reusing the old topology.
long double rebasedCurrent(const Tap::Profile& p,Mode mode,long double u,long double v){
 Matrix<5> a{};Vector<5> b{};
 a[0][4]=1;a[1][4]=-1;a[4][0]=1;a[4][1]=-1;b[4]=v;
 resistors(a,b,p,mode,u);return solve(a,b)[4];
}
struct TransientOracle {
 long double v=0,previous=0;
 Tap::Nodes process(double input,double fs,const Tap::Profile& p,Mode mode){
  const long double oldCurrent=rebasedCurrent(p,mode,previous,v);
  const long double g=2.0L*p.couplingFarads*fs,j=-g*v-oldCurrent;
  Matrix<4> a{};Vector<4> b{};branch(a,0,1,g);b[0]-=j;b[1]+=j;resistors(a,b,p,mode,input);
  const auto x=solve(a,b);v=x[0]-x[1];previous=input;
  Tap::Nodes out;out.capacitorInputVolts=static_cast<double>(x[0]);out.capacitorVolts=static_cast<double>(v);out.commonVolts=static_cast<double>(x[1]);out.highTapVolts=static_cast<double>(x[2]);out.lowTapVolts=static_cast<double>(x[3]);out.jackVolts=static_cast<double>(mode==Mode::high?x[2]:x[3]);
  out.sourceAmps=static_cast<double>(g*v+j);out.highSeriesAmps=static_cast<double>((x[1]-x[2])/p.highSeriesOhms);out.lowSeriesAmps=static_cast<double>((x[1]-x[3])/p.lowSeriesOhms);out.shuntAmps=static_cast<double>(x[3]/p.lowShuntOhms);
  if(p.load.kind==Kind::finite)out.loadAmps=out.jackVolts/p.load.ohms;
  if(p.load.kind==Kind::shorted)out.loadAmps=mode==Mode::high?out.highSeriesAmps:out.lowSeriesAmps-out.shuntAmps;
  return out;
 }
};
void ports(const Tap::Nodes& n,const Tap::Profile& p,Mode mode,double input){
 near(input,n.capacitorInputVolts+p.sourceOhms*n.sourceAmps,"source KVL");near(n.capacitorInputVolts,n.capacitorVolts+n.commonVolts,"capacitor KVL");
 near(n.sourceAmps,n.highSeriesAmps+n.lowSeriesAmps,"common KCL");near(n.commonVolts-n.highTapVolts,p.highSeriesOhms*n.highSeriesAmps,"HIGH resistor KVL");near(n.commonVolts-n.lowTapVolts,p.lowSeriesOhms*n.lowSeriesAmps,"LOW resistor KVL");near(n.lowTapVolts,p.lowShuntOhms*n.shuntAmps,"shunt KVL");
 near(n.lowSeriesAmps,n.shuntAmps+(mode==Mode::low?n.loadAmps:0),"LOW KCL");near(n.highSeriesAmps,mode==Mode::high?n.loadAmps:0,"HIGH KCL");
 if(p.load.kind==Kind::open)check(n.loadAmps==0,"open current exactly zero");
 if(p.load.kind==Kind::shorted)check(n.jackVolts==0,"short voltage exactly zero");
 if(p.load.kind==Kind::finite)near(n.jackVolts,p.load.ohms*n.loadAmps,"finite load KVL");
 if(mode==Mode::low||p.load.kind==Kind::open)check(n.highSeriesAmps==0,"unloaded HIGH branch current exactly zero");
}
void independentTransient(){
 std::mt19937 rng(0x8052);std::uniform_real_distribution<double> random(0,1);double worst=0;
 for(double fs:{8000.,48000.,192000.,1536000.})for(int fixture=0;fixture<18;++fixture){
  Tap::Profile p;p.sourceOhms=std::array{0.,100.,1000.,10000.}[fixture%4];p.load={std::array{Kind::open,Kind::shorted,Kind::finite}[fixture%3],std::array{100.,600.,10000.,100000.,1e9}[fixture%5]};
  p.couplingFarads=std::array{1e-6,5e-6,8e-6,10e-6,12e-6,20e-6,100e-6}[fixture%7];p.highSeriesOhms=std::array{100.,560.,56000.,100000.}[fixture%4];p.lowSeriesOhms=std::array{1000.,4700.,100000.}[fixture%3];p.lowShuntOhms=std::array{100.,560.,560000.,1000000.}[fixture%4];
  Tap model;check(model.prepare(fs,p,Mode::high),"transient fixture accepted");TransientOracle oracle;Mode mode=Mode::high;
  for(int n=0;n<600;++n){if(n%7==0){mode=mode==Mode::high?Mode::low:Mode::high;const auto before=model.getState();model.setMode(mode);check(before.capacitorVolts==model.getState().capacitorVolts&&before.previousSourceVolts==model.getState().previousSourceVolts&&before.readoutGain==model.getState().readoutGain,"mode event preserves physical charge and software history");}
   const double u=n<400?2*random(rng)-1:0;const auto before=model.getState();const double oldCurrent=static_cast<double>(rebasedCurrent(p,mode,before.previousSourceVolts,before.capacitorVolts));
   const auto got=model.process(u),expected=oracle.process(u,fs,p,mode);const auto av=physical(got),bv=physical(expected);
   for(std::size_t k=0;k<av.size();++k){near(av[k],bv[k],"independent stamped companion transient",2e-8,2e-11);worst=std::max(worst,std::abs(av[k]-bv[k]));}
   near(2*p.couplingFarads*fs*(got.capacitorVolts-before.capacitorVolts),got.sourceAmps+oldCurrent,"rebased charge trapezoid",3e-8,3e-11);ports(got,p,mode,u);
   near(got.audibleJackVolts,model.getState().readoutGain*(u-got.capacitorVolts),"readout uses SAME physical capacitor");
  }
 }
 std::cout<<"max independent transient port error "<<worst<<'\n';
}

ref::Inputs reference(const Tap::Profile& p,Mode mode,double f){
 ref::Inputs q;q.mode=mode==Mode::high?ref::Mode::high:ref::Mode::low;q.frequencyHz=f;q.sourceVolts=1.;q.sourceOhms=p.sourceOhms;q.highSeriesOhms=p.highSeriesOhms;q.lowSeriesOhms=p.lowSeriesOhms;q.lowShuntOhms=p.lowShuntOhms;
 q.load.kind=p.load.kind==Kind::open?ref::LoadKind::open:p.load.kind==Kind::shorted?ref::LoadKind::shorted:ref::LoadKind::finite;q.load.ohms=p.load.ohms;return q;
}
std::array<C,12> measured(const Tap::Profile& p,Mode mode,double fs,double desired,double& frequency){
 // Coherent measured sinusoid. Settling uses an INDEPENDENT unit-voltage stamp
 // to establish tau; it does not read production coefficients or output gains.
 const double tau=p.couplingFarads/static_cast<double>(rebasedCurrent(p,mode,1,0));
 const int count=std::max(2048,static_cast<int>(std::ceil(2*fs/desired)));
 const int cycles=std::max(1,static_cast<int>(std::round(desired*count/fs)));
 frequency=cycles*fs/count;const int settle=static_cast<int>(std::ceil(27*tau*fs));
 Tap model;model.prepare(fs,p,mode);std::array<C,12> sums{};
 for(int n=0;n<settle+count;++n){const double phase=2*std::numbers::pi*frequency*n/fs;const auto out=model.process(std::cos(phase));if(n>=settle){const C basis=std::polar(2.0/count,-phase);const auto values=physical(out);for(std::size_t k=0;k<values.size();++k)sums[k]+=values[k]*basis;sums[11]+=out.audibleJackVolts*basis;}}
 return sums;
}
void acAgainstReference(){
 double maximum=0;int fixtures=0;
 for(int fixture=0;fixture<14;++fixture)for(auto mode:{Mode::high,Mode::low}){
  Tap::Profile p;
  if(fixture<5)p.load={std::array{Kind::open,Kind::shorted,Kind::finite,Kind::finite,Kind::finite}[fixture],std::array{0.,0.,600.,10000.,100000.}[fixture]};
  if(fixture==5)p.sourceOhms=0;
  if(fixture==6)p.sourceOhms=1000;
  if(fixture==7)p.sourceOhms=10000;
  if(fixture==8)p.highSeriesOhms=56000;
  if(fixture==9)p.lowShuntOhms=560000;
  if(fixture>=10)p.couplingFarads=std::array{5e-6,8e-6,12e-6,20e-6}[fixture-10];
  const double pole=static_cast<double>(rebasedCurrent(p,mode,1,0))/(2*std::numbers::pi*p.couplingFarads);
  for(double desired:{pole*.5,pole*2,20.,1000.,20000.}){
   const double fs=fixture==6?192000.:48000.;double f=0;const auto got=measured(p,mode,fs,desired,f);
   // Offline solver fixes C=10uF. Scaling its frequency preserves s*C for
   // sensitivity profiles, without modifying its explicit conditional API.
   const double warped=fs/std::numbers::pi*std::tan(std::numbers::pi*f/fs);
   const auto expected=physical(ref::solve(reference(p,mode,warped*p.couplingFarads/ref::couplingFarads)));
   for(std::size_t k=0;k<expected.size();++k){near(got[k],expected[k],"measured AC against independent offline reference");maximum=std::max(maximum,std::abs(got[k]-expected[k]));}
   near(got[11],expected[5],"settled audible readout equals physical endpoint");++fixtures;
  }
 }
 std::cout<<fixtures<<" measured AC fixtures, max complex port error "<<maximum<<'\n';
}

void continuation(Tap a,Tap b){
 check(a.sampleRate()==b.sampleRate()&&a.getMode()==b.getMode(),"rejection preserves rate and mode");
 check(a.getState().capacitorVolts==b.getState().capacitorVolts&&a.getState().previousSourceVolts==b.getState().previousSourceVolts&&a.getState().readoutGain==b.getState().readoutGain,"rejection preserves every charged/readout state");
 for(int n=0;n<113;++n){if(n==51){a.setMode(Mode::high);b.setMode(Mode::high);}check(identical(runtimeProcess(a,std::sin(n*.13)),runtimeProcess(b,std::sin(n*.13))),"atomic rejection retains bit-identical future trajectory");}
}
void boundariesRollbackAndGuards(){
 using Profile=Tap::Profile;constexpr std::array<double Profile::*,5> fields{&Profile::couplingFarads,&Profile::sourceOhms,&Profile::highSeriesOhms,&Profile::lowSeriesOhms,&Profile::lowShuntOhms};
 constexpr std::array<double,5> lo{1e-6,0.,100.,1000.,100.},hi{100e-6,10000.,100000.,100000.,1000000.};const double inf=std::numeric_limits<double>::infinity(),nan=std::numeric_limits<double>::quiet_NaN();
 Profile active{8e-6,750,2300,27000,7800,{Kind::finite,3800}},candidate{20e-6,9876,12345,45000,765432,{Kind::finite,76543}};Tap charged;charged.prepare(44100,active,Mode::high);for(int n=0;n<333;++n)charged.process(.8*std::sin(n*.019));charged.setMode(Mode::low);for(int n=0;n<37;++n)charged.process(.3);
 check(charged.getState().capacitorVolts!=0&&charged.getState().readoutGain!=charged.selectedCoefficients().jackGain,"rollback fixture charged with pending readout slew");
 auto rejects=[&](double fs,const Profile& p,Mode mode){auto a=charged;check(!a.prepare(fs,p,mode),"invalid replacement rejected");const auto& saved=a.getProfile();for(auto field:fields)check(saved.*field==active.*field,"invalid replacement preserves profile field");check(saved.load.kind==active.load.kind&&saved.load.ohms==active.load.ohms,"invalid replacement preserves load");continuation(a,charged);};
 for(std::size_t k=0;k<fields.size();++k){for(double bad:{-inf,inf,nan,std::nextafter(lo[k],-inf),std::nextafter(hi[k],inf)}){auto p=candidate;p.*fields[k]=bad;check(!Tap::validProfile(p),"invalid profile rejected independently");rejects(192000,p,Mode::high);}for(double bound:{lo[k],hi[k]}){auto p=candidate;p.*fields[k]=bound;Tap a;check(a.prepare(192000,p,Mode::low),"inclusive profile endpoint accepted");for(double x:physical(a.process(1)))check(std::isfinite(x),"endpoint profile finite");}}
 for(double bad:{-inf,inf,nan,std::nextafter(100.,-inf),std::nextafter(1e9,inf)}){auto p=candidate;p.load.ohms=bad;rejects(192000,p,Mode::high);}
 for(double bound:{100.,1e9}){auto p=candidate;p.load.ohms=bound;Tap a;check(a.prepare(192000,p,Mode::low),"inclusive finite-load endpoint accepted");}
 for(double fs:{-inf,inf,nan,std::nextafter(Tap::minimumRate,-inf),std::nextafter(Tap::maximumRate,inf)})rejects(fs,candidate,Mode::high);
 for(auto bad:{static_cast<Mode>(-1),static_cast<Mode>(99)}){rejects(192000,candidate,bad);auto a=charged;check(!a.setMode(bad)&&!a.reset(bad),"invalid mode/reset rejected");continuation(a,charged);}
 for(auto kind:{static_cast<Kind>(-1),static_cast<Kind>(99)}){auto p=candidate;p.load.kind=kind;rejects(192000,p,Mode::high);}
 for(double fs:{Tap::minimumRate,Tap::maximumRate}){auto a=charged;Tap b;check(a.prepare(fs,candidate,Mode::low)&&b.prepare(fs,candidate,Mode::low),"inclusive rate valid");continuation(a,b);}
 for(double input:{-inf,inf,nan}){auto a=charged,b=charged;check(identical(runtimeProcess(a,input),runtimeProcess(b,0)),"all nonfinite input replaced by zero");}
 for(double sign:{-1.,1.}){auto a=charged,b=charged;check(identical(runtimeProcess(a,sign*std::numeric_limits<double>::max()),runtimeProcess(b,sign*Tap::maximumInputMagnitude)),"finite input symmetric numerical clamp");}
 for(auto mode:{Mode::high,Mode::low}){auto a=charged;a.reset(mode);check(a.getState().capacitorVolts==0&&a.getState().previousSourceVolts==0,"reset zeroes sole capacitor and source history");check(a.getState().readoutGain==a.selectedCoefficients().jackGain,"reset initializes current endpoint Ks");for(int n=0;n<100;++n){auto out=a.process(0);for(auto x:physical(out))check(x==0,"zero-input reset/startup exactly silent");check(out.audibleJackVolts==0,"software readout startup silent");}}
}

void softwareReadoutAndBounds(){
 for(double fs:{8000.,48000.,96000.,192000.,1536000.})for(auto target:{Mode::high,Mode::low}){
  Tap a;const auto initial=target==Mode::high?Mode::low:Mode::high;a.prepare(fs,Tap::Profile{},initial);for(int n=0;n<151;++n)a.process(.5);const auto before=a.getState();a.setMode(target);const auto goal=a.selectedCoefficients().jackGain;
  check(a.getState().capacitorVolts==before.capacitorVolts,"selector never resets charge");check(a.getState().readoutGain==before.readoutGain,"selector never jumps readout");
  const int count=static_cast<int>(fs*.005);for(int n=0;n<count;++n){const auto out=a.process(.5);if(n==0){near(out.jackVolts,goal*(.5-out.capacitorVolts),"physical load selected immediately");check(std::abs(out.jackVolts-out.audibleJackVolts)>1e-4,"raw and software readout intentionally different during slew");}}
  near(a.getState().readoutGain,goal+(before.readoutGain-goal)*std::exp(-1.),"5ms is sample-rate-independent time constant",2e-12);
 }
 double maximumB=0,peak=0;
 // Exhaustive resistor/C corner combinations; each load case and both rate
 // endpoints. The convex-state invariant survives worst-case every-sample mode
 // toggling, unlike endpoint shadow histories or charge resets.
 for(double fs:{8000.,1536000.})for(int mask=0;mask<32;++mask)for(auto kind:{Kind::open,Kind::shorted,Kind::finite})for(double rl:{100.,1e9}){
  Tap::Profile p;p.couplingFarads=mask&1?100e-6:1e-6;p.sourceOhms=mask&2?10000:0;p.highSeriesOhms=mask&4?100000:100;p.lowSeriesOhms=mask&8?100000:1000;p.lowShuntOhms=mask&16?1000000:100;p.load={kind,rl};Tap a;check(a.prepare(fs,p,Mode::high),"all inclusive numerical corners accepted");
  for(int n=0;n<768;++n){const auto mode=n%2?Mode::high:Mode::low;a.setMode(mode);const auto c=a.selectedCoefficients();maximumB=std::max(maximumB,c.integrationB);check(c.integrationB>0&&c.integrationB<.5&&c.jackGain>=0&&c.jackGain<=1,"convex coefficients in proved passive domain");
   const double input=(n%7<3?1.:-1.)*Tap::maximumInputMagnitude;const auto out=a.process(input);check(std::abs(out.capacitorVolts)<=Tap::maximumInputMagnitude*(1+1e-14),"one-cap convex voltage invariant");check(a.getState().readoutGain>=0&&a.getState().readoutGain<=1,"dezipper gain convex invariant");check(std::abs(out.jackVolts)<=2*Tap::maximumInputMagnitude&&std::abs(out.audibleJackVolts)<=2*Tap::maximumInputMagnitude,"raw and audible 2A bound");for(double x:physical(out))check(std::isfinite(x),"extreme alternating topology finite physical ports");peak=std::max({peak,std::abs(out.jackVolts),std::abs(out.audibleJackVolts)});
  }
 }
 near(maximumB,1/(1+2*(100.*1100/(100+1100))*1e-6*8000),"worst b corner reaches predicted bound");
 std::cout<<"domain max b="<<maximumB<<", alternating-input peak="<<peak<<'\n';
}

// Separate full ideal-active graph: LI/RI terminal nodes, A/B, three inverting
// virtual-ground nodes and three driver outputs. Each 100k/47k/input-source/
// feedback resistor is stamped; ideal-amplifier constraints close the graph.
std::array<double,3> activeOracle(double li,double ri,double rsL,double rsR,double rfL,double rfR,double rfG){
 Matrix<10> a{};Vector<10> b{};constexpr int LI=0,RI=1,A=2,B=3,NL=4,NR=5,NG=6,OL=7,OR=8,OG=9;
 branch(a,LI,A,1e-5L);branch(a,RI,B,1e-5L);branch(a,A,NL,1.0L/47000);branch(a,B,NR,1.0L/47000);branch(a,A,NG,1e-5L);branch(a,B,NG,1e-5L);branch(a,NL,OL,1.0L/rfL);branch(a,NR,OR,1.0L/rfR);branch(a,NG,OG,1.0L/rfG);
 if(rsL==0)fixed(a,b,LI,li);else{a[LI][LI]+=1.0L/rsL;b[LI]+=li/rsL;}
 if(rsR==0)fixed(a,b,RI,ri);else{a[RI][RI]+=1.0L/rsR;b[RI]+=ri/rsR;}
 // Output rows supply unconstrained op-amp currents, so replace each with its
 // virtual-ground constraint, not with the reduced 0.47 formula under test.
 a[OL].fill(0);a[OL][NL]=1;a[OR].fill(0);a[OR][NR]=1;a[OG].fill(0);a[OG][NG]=1;
 const auto x=solve(a,b);return {static_cast<double>(x[OL]),static_cast<double>(x[OR]),static_cast<double>(x[OG])};
}
void activeMatrixAndTripleLifecycle(){
 std::mt19937 rng(0x47080);std::uniform_real_distribution<double> random(0,1);double maxResidual=0;
 for(int k=0;k<1000;++k){const double li=k<4?std::array{1.,0.,1.,1.}[k]:2*random(rng)-1;const double ri=k<4?std::array{0.,1.,1.,-1.}[k]:2*random(rng)-1;
  const double rsL=k<4?0:300000*random(rng),rsR=k<4?0:300000*random(rng),rfL=150000+470000*random(rng),rfR=150000+470000*random(rng),rfG=150000+470000*random(rng);
  const auto source=activeOracle(li,ri,rsL,rsR,rfL,rfR,rfG);const double predicted=.47*(rfG/rfL*source[0]+rfG/rfR*source[1]);near(source[2],predicted,"independently stamped GENERAL matrix relation",2e-12);maxResidual=std::max(maxResidual,std::abs(source[2]-predicted));
 }
 auto source=activeOracle(1,0,0,0,220000,220000,220000);near(source[0],-220./194.,"optional absolute LEFT driver gain is not unity");near(source[2],-103.4/194.,"optional absolute GENERAL driver gain");
 std::cout<<"1000 independent active matrices, max reduced-relation residual "<<maxResidual<<'\n';
 // Cross-kernel triple-versus-scalar bit identity is not a model invariant:
 // optimizing compilers can choose different legal FMA contractions at their
 // separate inline sites. Instead qualify ALL THREE channels against the same
 // independent stamped companion used above, with its UNCHANGED error bounds.
 // Bit-exact rollback and partition assertions below still use one runtime path.
 for(double ratioL:{.25,.5,1.,2.,4.})for(double ratioR:{.25,.5,1.,2.,4.}){
  Triple::Profile p;p.generalToLeftFeedbackRatio=ratioL;p.generalToRightFeedbackRatio=ratioR;p.left.load={Kind::finite,600};p.right.sourceOhms=1000;p.right.couplingFarads=20e-6;p.general.lowShuntOhms=560000;
  Triple a;a.prepare(96000,p,Mode::high);const std::array<Tap::Profile,3> profiles{p.left,p.right,p.general};std::array<TransientOracle,3> oracles{};std::array<std::array<long double,2>,3> endpoints{};std::array<long double,3> readout{};
  for(std::size_t channel=0;channel<3;++channel){for(auto mode:{Mode::high,Mode::low})endpoints[channel][mode==Mode::high?0:1]=ref::solve(reference(profiles[channel],mode,1e30)).jackVolts.real();readout[channel]=endpoints[channel][0];}
  // The high-frequency offline limit supplies K without copying the production
  // resistance reduction. The 5ms software policy uses an independent exp-form
  // recurrence, rather than the production expm1 update or production Ks state.
  const long double readoutDecay=std::exp(-1.0L/(.005L*96000));Mode mode=Mode::high;bool distinct=false;
  for(int n=0;n<1200;++n){
   if(n==317||n==901){mode=n==317?Mode::low:Mode::high;a.setMode(mode);}
   const double ul=n<700?.6*std::sin(n*.13):0,ur=n<700?.3*std::cos(n*.037):0;const auto out=a.process(ul,ur);const std::array<Tap::Nodes,3> actual{out.left,out.right,out.general};const std::array<double,3> inputs{ul,ur,.47*(ratioL*ul+ratioR*ur)};
   for(std::size_t channel=0;channel<3;++channel){
    const auto expected=oracles[channel].process(inputs[channel],96000,profiles[channel],mode);const auto gotPorts=physical(actual[channel]),expectedPorts=physical(expected);
    for(std::size_t port=0;port<gotPorts.size();++port)near(gotPorts[port],expectedPorts[port],"all three circuits clock against independent transient oracle while unheard/idle",2e-8,2e-11);
    const long double target=endpoints[channel][mode==Mode::high?0:1];readout[channel]=target+(readout[channel]-target)*readoutDecay;
    near(actual[channel].audibleJackVolts,static_cast<double>(readout[channel]*(inputs[channel]-oracles[channel].v)),"triple independent software readout follows same physical capacitor",2e-8,2e-11);
   }
   distinct|=std::abs(out.general.jackVolts-.5*(out.left.jackVolts+out.right.jackVolts))>.01;
  }
  check(distinct,"GENERAL is not average of unequally loaded jacks");
 }
 // Equal nominal profiles give 0.94 of either output for correlated sources;
 // anti-correlated sources cancel BEFORE loading. Separate initial/reset state.
 Triple a;for(int n=0;n<2000;++n){const double u=std::sin(n*.023);const auto out=a.process(u,u);near(out.general.jackVolts,.94*out.left.jackVolts,"equal-source GENERAL 0.94 relation");}
 a.reset(Mode::low);for(int n=0;n<2000;++n){auto out=a.process(std::sin(n*.023),-std::sin(n*.023));check(out.general.jackVolts==0&&out.general.audibleJackVolts==0,"anti-correlated GENERAL exactly cancels");}
 // Listening happens outside this object. Different external readout choices
 // and arbitrary host block partitions do not stop any capacitor's clock.
 Triple referenceModel,partitioned;std::vector<Triple::Outputs> expected;for(int n=0;n<1500;++n){if(n%173==0)referenceModel.setMode(n%346?Mode::low:Mode::high);expected.push_back(runtimeProcess(referenceModel,n<900?std::sin(n*.057):0,n<900?.2:0));}
 int index=0;for(int block:{1,7,113,3,512,864})for(int i=0;i<block;++i){if(index%173==0)partitioned.setMode(index%346?Mode::low:Mode::high);const auto out=runtimeProcess(partitioned,index<900?std::sin(index*.057):0,index<900?.2:0);check(identical(out.left,expected[index].left)&&identical(out.right,expected[index].right)&&identical(out.general,expected[index].general),"block/listening partition invariant");++index;}
 check(index==1500,"partition fixture exhausts same sample count");
 // A failed triple prepare must not replace even its first valid channel.
 Triple charged;for(int n=0;n<333;++n)charged.process(.3,.8);charged.setMode(Mode::low);for(int n=0;n<7;++n)charged.process(.2,.1);
 for(int field=0;field<5;++field){auto p=Triple::Profile{};p.left.sourceOhms=9876;p.right.sourceOhms=1234;p.general.sourceOhms=7654;p.generalToLeftFeedbackRatio=.5;p.generalToRightFeedbackRatio=2;
  if(field==0)p.left.highSeriesOhms=0;
  if(field==1)p.right.lowSeriesOhms=0;
  if(field==2)p.general.couplingFarads=0;
  if(field==3)p.generalToLeftFeedbackRatio=.249999;
  if(field==4)p.generalToRightFeedbackRatio=4.000001;
  auto bad=charged,good=charged;check(!bad.prepare(192000,p,Mode::high),"triple invalid prepare rejected");for(int n=0;n<100;++n){const auto x=runtimeProcess(bad,.1,.2),y=runtimeProcess(good,.1,.2);check(identical(x.left,y.left)&&identical(x.right,y.right)&&identical(x.general,y.general),"triple invalid replacement atomic continuation");}
 }
 for(auto invalid:{static_cast<Mode>(-1),static_cast<Mode>(99)}){auto bad=charged,good=charged;check(!bad.setMode(invalid)&&!bad.reset(invalid),"triple invalid selector/reset rejected");for(int n=0;n<100;++n){const auto x=runtimeProcess(bad,.1,.2),y=runtimeProcess(good,.1,.2);check(identical(x.left,y.left)&&identical(x.right,y.right)&&identical(x.general,y.general),"triple invalid control preserves charged continuation");}}
 for(auto mode:{Mode::high,Mode::low}){auto resetModel=charged;resetModel.reset(mode);const auto out=resetModel.process(0,0);for(auto circuit:{&resetModel.leftCircuit(),&resetModel.rightCircuit(),&resetModel.generalCircuit()})check(circuit->getState().capacitorVolts==0&&circuit->getState().previousSourceVolts==0&&circuit->getState().readoutGain==circuit->selectedCoefficients().jackGain,"triple reset initializes all physical/readout states");check(out.left.audibleJackVolts==0&&out.right.audibleJackVolts==0&&out.general.audibleJackVolts==0,"triple reset silence");}
 for(double ratio:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-.5,std::nextafter(.25,0.),std::nextafter(4.,5.)})for(bool left:{false,true}){auto p=Triple::Profile{};if(left)p.generalToLeftFeedbackRatio=ratio;else p.generalToRightFeedbackRatio=ratio;check(!Triple::validProfile(p),"every GENERAL feedback ratio boundary rejected");}
 auto guarded=charged,explicitlyBounded=charged;for(double l:{std::numeric_limits<double>::quiet_NaN(),-1e100,1e100}){const auto x=runtimeProcess(guarded,l,1e100),y=runtimeProcess(explicitlyBounded,Tap::boundedInput(l),1e6);check(identical(x.left,y.left)&&identical(x.right,y.right)&&identical(x.general,y.general),"triple sanitizes sources before GENERAL sum");}
}

// Exact linear-input evolution for each actual new-topology interval. An
// independent voltage-source stamp yields tau; no production recurrence used.
long double exactLinear(long double v,long double u0,long double u1,long double h,long double tau){const long double oneMinus=-std::expm1(-h/tau);return v+(u0-v)*oneMinus+(u1-u0)*(1-tau/h*oneMinus);}
double forcing(double t){if(t<.005)return t/.005;if(t<.025)return 1;if(t<.03)return 1-(t-.025)/.005;return 0;}
void analogConvergenceAndTails(){
 for(bool switching:{false,true}){
  double previous=1e100;std::cout<<(switching?"switched":"fixed")<<" physical-time linear/step/decay RMS errors";
  for(double fs:{48000.,96000.,192000.,384000.}){Tap::Profile p;p.load={Kind::finite,600};Tap a;a.prepare(fs,p,Mode::high);long double exact=0,u0=0;double sum=0;int observed=0;Mode mode=Mode::high;
   for(int n=1;n<=static_cast<int>(fs*.12);++n){if(switching&&(n==static_cast<int>(fs*.02)+1||n==static_cast<int>(fs*.04)+1)){mode=mode==Mode::high?Mode::low:Mode::high;a.setMode(mode);}const double u=forcing(n/fs);const long double tau=p.couplingFarads/rebasedCurrent(p,mode,1,0);exact=exactLinear(exact,u0,u,1/fs,tau);u0=u;const auto out=a.process(u);if(n%static_cast<int>(fs/12000)==0){const double e=out.capacitorVolts-static_cast<double>(exact);sum+=e*e;++observed;}}
   const double rms=std::sqrt(sum/observed);std::cout<<' '<<rms;check(rms<previous*.29,"physical-time piecewise-linear/charged-switch second-order convergence");previous=rms;
  }
  std::cout<<'\n';
 }
 for(bool switching:{false,true}){
  constexpr long double omega=2*std::numbers::pi_v<long double>*900;double previous=1e100;std::cout<<(switching?"switched":"fixed")<<" physical-time sine RMS errors";
  for(double fs:{48000.,96000.,192000.,384000.}){Tap::Profile p;p.load={Kind::finite,600};Tap a;a.prepare(fs,p,Mode::high);Mode mode=Mode::high;long double exact=0;double sum=0;int observed=0;
   for(int n=1;n<=static_cast<int>(fs*.06);++n){if(switching&&(n==static_cast<int>(fs*.02)+1||n==static_cast<int>(fs*.04)+1)){mode=mode==Mode::high?Mode::low:Mode::high;a.setMode(mode);}const long double tau=p.couplingFarads/rebasedCurrent(p,mode,1,0),h=1.0L/fs,t0=(n-1)*h,t1=n*h;
    const auto particular=[&](long double t){return (std::sin(omega*t)-omega*tau*std::cos(omega*t))/(1+omega*omega*tau*tau);};exact=particular(t1)+(exact-particular(t0))*std::exp(-h/tau);const auto out=a.process(static_cast<double>(std::sin(omega*t1)));if(n%static_cast<int>(fs/12000)==0){const double error=out.capacitorVolts-static_cast<double>(exact);sum+=error*error;++observed;}
   }
   const double rms=std::sqrt(sum/observed);std::cout<<' '<<rms;check(rms<previous*.29,"fixed-time sine/charged-switch second-order convergence");previous=rms;
  }std::cout<<'\n';
 }
 for(double f:{3.,1000.,12000.}){Tap::Profile p;double previous=1e100;const auto analog=ref::solve(reference(p,Mode::high,f)).jackVolts;std::cout<<"same-frequency "<<f<<" Hz complex errors";
  for(double fs:{48000.,96000.,192000.,384000.}){
   // Same physical frequency with exact sinusoid solution and enough decay;
   // use a coherent count for the selected integer frequencies.
   Tap a;a.prepare(fs,p,Mode::high);const int count=static_cast<int>(fs),settle=static_cast<int>(2*fs);C result{};for(int n=0;n<settle+count;++n){const double phase=2*std::numbers::pi*f*n/fs;const auto out=a.process(std::cos(phase));if(n>=settle)result+=out.jackVolts*std::polar(2.0/count,-phase);}const double error=std::abs(result-analog);std::cout<<' '<<error;check(error<previous*.30,"unwarped physical-frequency second-order convergence");previous=error;
  }std::cout<<'\n';
 }
 // Exact discrete constant-step/decay result and closed-form analog tail at
 // 27 time constants. The first startup interval linearly joins Uprev=0 to 1.
 for(auto mode:{Mode::high,Mode::low})for(bool slow:{false,true}){
  Tap::Profile p;if(slow){p.couplingFarads=100e-6;p.sourceOhms=10000;p.highSeriesOhms=100000;p.lowSeriesOhms=100000;p.lowShuntOhms=1000000;p.load={Kind::open,0};}
  constexpr double fs=8000;Tap a;a.prepare(fs,p,mode);const long double tau=p.couplingFarads/rebasedCurrent(p,mode,1,0);const long double h=1/fs,b=h/(2*tau+h),decay=1-2*b;
  const int count=static_cast<int>(std::ceil(27*tau*fs));Tap::Nodes out;for(int n=1;n<=count;++n){out=a.process(1);if(n==1||n==100||n==count){const double expected=static_cast<double>(1-(1-b)*std::pow(decay,n-1));near(out.capacitorVolts,expected,"closed-form discrete DC step",3e-10,2e-11);}}
  check(std::abs(out.jackVolts)<1e-9,"longest declared DC charging tail explicitly bounded");const long double start=out.capacitorVolts;long double exact=exactLinear(start,1,0,h,tau);out=a.process(0);for(int n=1;n<count;++n)out=a.process(0);exact*=std::exp(-(count-1)*h/tau);near(out.capacitorVolts,static_cast<double>(exact),"closed-form analog long decay",1e-7,3e-11);check(std::abs(out.jackVolts)<1e-9,"silence drains longest tail without freezing/reset");
 }
}

void endpointAndMutationWitnesses(){
 struct Row{double load,rs,rh,rb,high,low;};
 constexpr std::array<Row,8> table{{{100000,100,560,560,.974926955,.103956279},{0,100,560,560,.981343284,.104477612},{10000,100,560,560,.920745839,.099488346},{600,100,560,560,.467999288,.056910569},{100000,100,56000,560,.628670732,.103956279},{100000,100,560,560000,.993267556,.946457545},{100000,0,560,560,.994431185,.105933807},{100000,1000,560,560,.828652340,.089003039}}};
 for(const auto& row:table){Tap::Profile p;p.sourceOhms=row.rs;p.highSeriesOhms=row.rh;p.lowShuntOhms=row.rb;p.load={row.load==0?Kind::open:Kind::finite,row.load};Tap a;a.prepare(96000,p,Mode::high);near(a.selectedCoefficients().jackGain,row.high,"documented chosen-profile HIGH table",0,5.1e-10);a.setMode(Mode::low);near(a.selectedCoefficients().jackGain,row.low,"documented chosen-profile LOW table",0,5.1e-10);}
 Tap::Profile p;p.load={Kind::finite,600};p.sourceOhms=1000;const auto nominal=ref::solve(reference(p,Mode::high,3));
 // Prove the chosen witnesses distinguish the named circuit mistakes, rather
 // than merely checking correct-model values at insensitive frequencies.
 const C noDivider=600.0/(1000.0+560.0+600.0+C(0,-1/(2*std::numbers::pi*3*10e-6)));
 check(std::abs(noDivider-nominal.jackVolts)>1e-3,"mutation witness: always-connected LOW divider");
 // A wire instead of C has real, frequency-flat transfer.
 Tap a;a.prepare(96000,p,Mode::high);check(std::abs(C(a.selectedCoefficients().jackGain)-nominal.jackVolts)>.01,"mutation witness: omitted coupling capacitor");
 for(int resistor=0;resistor<4;++resistor){auto mutant=p;if(resistor==0)mutant.sourceOhms=0;if(resistor==1)mutant.highSeriesOhms*=2;if(resistor==2)mutant.lowSeriesOhms*=2;if(resistor==3)mutant.lowShuntOhms*=2;const auto mode=resistor==3?Mode::low:Mode::high;check(std::abs(ref::solve(reference(mutant,mode,3)).jackVolts-ref::solve(reference(p,mode,3)).jackVolts)>1e-4,"mutation witness: Rs/Rh/Ra/Rb change detected");}
 check(std::abs(ref::solve(reference(p,Mode::low,3)).jackVolts-nominal.jackVolts)>.01,"mutation witness: contacts swapped");
 auto opened=p;opened.load={Kind::open,0};check(std::abs(ref::solve(reference(opened,Mode::high,3)).jackVolts-nominal.jackVolts)>.1,"mutation witness: finite load treated as open");auto shorted=p;shorted.load={Kind::shorted,0};Tap s;s.prepare(96000,shorted,Mode::high);const auto out=s.process(1);check(out.jackVolts==0&&out.loadAmps!=0,"mutation witness: short is NOT open or zero current");
 // Charged switching rejects reset/freeze and two independent endpoint shadow
 // capacitors: the unselected LOW shadow has a different physical charge.
 Tap physicalModel,highShadow,lowShadow;physicalModel.prepare(48000,p,Mode::high);highShadow.prepare(48000,p,Mode::high);lowShadow.prepare(48000,p,Mode::low);
 for(int n=0;n<777;++n){physicalModel.process(.8);highShadow.process(.8);lowShadow.process(.8);}physicalModel.setMode(Mode::low);auto resetMutant=physicalModel;resetMutant.reset(Mode::low);const auto got=physicalModel.process(.2),shadow=lowShadow.process(.2),reset=resetMutant.process(.2);check(std::abs(got.jackVolts-shadow.jackVolts)>1e-4,"mutation witness: two endpoint capacitors rejected");check(std::abs(got.jackVolts-reset.jackVolts)>1e-3,"mutation witness: switch charge reset rejected");
 auto frozen=physicalModel;for(int n=0;n<800;++n)physicalModel.process(0);check(std::abs(physicalModel.process(.1).jackVolts-frozen.process(.1).jackVolts)>1e-3,"mutation witness: idle charge freeze rejected");
 check(std::abs(got.jackVolts-.1*got.jackVolts)>1e-3,"mutation witness: legacy LOW multiplier rejected");check(std::abs(got.jackVolts-got.jackVolts/physicalModel.selectedCoefficients().jackGain)>1e-3,"mutation witness: independent endpoint normalization rejected");
 const auto active=activeOracle(1,.3,2000,57000,220000,330000,470000);check(std::abs(active[2]-.5*(470000./220000*active[0]+470000./330000*active[1]))>.01,"mutation witness: 0.47 replaced by 0.5 rejected");
}
void realtimeAllocation(){
 Triple a;const auto before=allocations;double witness=0;countAllocations=true;
 for(int n=0;n<5000;++n){if(n%31==0)a.setMode(n%62?Mode::low:Mode::high);const auto out=a.process(std::sin(n*.13),std::cos(n*.03));witness+=out.left.audibleJackVolts+out.general.audibleJackVolts;}
 countAllocations=false;check(allocations==before,"three-channel processing/selector controls allocate no C++ heap");check(std::isfinite(witness)&&witness!=0,"allocation probe observes output");
}
// Phase 1.5 gain staging: inferred driver headroom law, not a measured device.
void circuitDriverHeadroom(){
 using cs80::kCircuitOutputReference; using cs80::kCircuitDriverKnee; using cs80::kCircuitDriverCeiling;
 check(kCircuitOutputReference==0.5,"circuit output reference is the documented -6 dB calibration");
 bool unity=true,bounded=true,monotonic=true,odd=true;double previous=-1e9;
 for(int n=-40000;n<=40000;++n){
  const double x=n*1e-4,y=cs80::circuitOutputDriver(x);
  if(std::abs(x)<=kCircuitDriverKnee&&y!=x)unity=false;
  if(std::abs(y)>kCircuitDriverCeiling)bounded=false;
  if(y<previous)monotonic=false;previous=y;
  if(cs80::circuitOutputDriver(-x)!=-y)odd=false;
 }
 check(unity,"driver is exactly unity at and below the -1 dBFS knee");
 check(bounded,"driver output never exceeds the ceiling");
 check(monotonic,"driver transfer is monotonic");
 check(odd,"driver transfer is odd-symmetric");
 const double k=kCircuitDriverKnee,h=1e-7;
 near((cs80::circuitOutputDriver(k+h)-k)/h,1.0,"driver slope is continuous at the knee",0,1e-6);
 check(cs80::circuitOutputDriver(1e9)<=kCircuitDriverCeiling&&cs80::circuitOutputDriver(3.0)>.99,"driver approaches the ceiling for large inputs");
}
} // namespace
int main(int argc,char** argv){
 try{
  const bool quick=argc>1&&std::string_view(argv[1])=="--mutation-probe";
  boundariesRollbackAndGuards();circuitDriverHeadroom();endpointAndMutationWitnesses();independentTransient();activeMatrixAndTripleLifecycle();realtimeAllocation();
  if(!quick){softwareReadoutAndBounds();acAgainstReference();analogConvergenceAndTails();}
  std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
 }catch(const std::exception& e){std::cerr<<"Unexpected oracle rejection: "<<e.what()<<'\n';return 1;}
}

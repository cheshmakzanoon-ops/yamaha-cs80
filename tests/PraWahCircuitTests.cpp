// Numerical consistency of a declared ideal circuit + inferred profile, NOT
// measured hardware equality. Independent AC oracle and transient nodal solve.
#include "core/PraWahCircuit.h"
#include "reference/PraWah.h"
#include <array>
#include <cstdlib>
#include <new>
#include <complex>
#include <iostream>
#include <limits>
#include <numbers>
#include <random>
#include <vector>
namespace { bool countAllocations=false; std::size_t allocations=0; }
void* operator new(std::size_t size) { if(countAllocations)++allocations; if(void* p=std::malloc(size?size:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
using cs80::PraWahCircuit;
using C=std::complex<double>;
namespace {
int checks=0, failures=0;
void check(bool ok,const char* message){++checks;if(!ok){if(failures<25)std::cerr<<message<<'\n';++failures;}}
void near(double a,double b,const char* what,double tol=2e-10){check(std::abs(a-b)<=tol*(1+std::abs(a)+std::abs(b)),what);}
void near(C a,C b,const char* what,double tol=2e-8){check(std::abs(a-b)<=tol*(1+std::abs(a)+std::abs(b)),what);}
std::array<double,9> values(PraWahCircuit::Nodes n){return {n.x,n.n,n.a,n.p,n.b,n.q,n.c,n.d,n.y};}
std::array<C,9> values(cs80::reference::pra_wah::Response n){return {n.x,n.n,n.a,n.p,n.b,n.q,n.c,n.d,n.y};}
cs80::reference::pra_wah::Boundary boundary(const PraWahCircuit::Profile& p,double optical){
 return {optical,p.parallelOhms,p.vr1Ohms,p.vr2Ohms,p.sourceOhms,{p.dryLoadSiemens,0},{0,0}};
}
// Independent physical-node companion assembly. No all-pass reduction, nX/n0,
// source admittance reduction or implementation state-update helper is reused.
struct TransientOracle {
 std::array<double,4> v{},i{};
 std::array<double,9> tick(double u,double fs,const PraWahCircuit::Profile& p,double opt) {
  constexpr int X=0,N=1,A=2,P=3,B=4,Q=5,Cn=6,D=7,Y=8,E=9;
  constexpr std::array<double,4> capacitance{10e-6,2.2e-9,33e-9,1e-6};
  std::array<double,4> g,j;
  for(int k=0;k<4;++k){g[k]=2*capacitance[k]*fs;j[k]=-g[k]*v[k]-i[k];}
  const double rt=p.vr1Ohms+1/(1/opt+1/p.parallelOhms);
  std::array<std::array<double,10>,10> m{};std::array<double,10> rhs{};
  m[X][X]=g[0]+2e-4+p.dryLoadSiemens;m[X][E]=-g[0];m[X][N]=-1e-4;rhs[X]=j[0];
  m[N][N]=2e-4+1/470.;m[N][X]=-1e-4;m[N][Cn]=-1/470.;
  m[A][A]=1;m[A][N]=1;
  m[P][P]=1/rt+g[1];m[P][A]=-1/rt;rhs[P]=-j[1];
  m[B][B]=1;m[B][P]=-2;m[B][A]=1;
  m[Q][Q]=1e-4+g[2];m[Q][B]=-1e-4;rhs[Q]=-j[2];
  m[Cn][Cn]=1;m[Cn][Q]=-2;m[Cn][B]=1;
  m[D][D]=g[3]+2e-4;m[D][Cn]=-g[3];rhs[D]=-j[3];
  m[Y][Y]=1;m[Y][D]=p.vr2Ohms/10000.;
  m[E][E]=1+p.sourceOhms*g[0];m[E][X]=-p.sourceOhms*g[0];rhs[E]=u-p.sourceOhms*j[0];
  for(int r=0;r<10;++r){double scale=0;for(double q:m[r])scale=std::max(scale,std::abs(q));for(double& q:m[r])q/=scale;rhs[r]/=scale;}
  for(int k=0;k<10;++k){int pivot=k;for(int r=k+1;r<10;++r)if(std::abs(m[r][k])>std::abs(m[pivot][k]))pivot=r;std::swap(m[k],m[pivot]);std::swap(rhs[k],rhs[pivot]);
   for(int r=k+1;r<10;++r){double factor=m[r][k]/m[k][k];for(int c=k;c<10;++c)m[r][c]-=factor*m[k][c];rhs[r]-=factor*rhs[k];}}
  std::array<double,10> x{};
  for(int r=9;r>=0;--r){double q=rhs[r];for(int c=r+1;c<10;++c)q-=m[r][c]*x[c];x[r]=q/m[r][r];}
  v={x[E]-x[X],x[P],x[Q],x[D]-x[Cn]};for(int k=0;k<4;++k)i[k]=g[k]*v[k]+j[k];
  return {x[X],x[N],x[A],x[P],x[B],x[Q],x[Cn],x[D],x[Y]};
 }
};
void setupAndGuards(){
 PraWahCircuit a,b;PraWahCircuit::Profile p;
 check(a.prepare(96000,p,.4),"prepare nominal");b=a;
 for(int k=0;k<100;++k){a.process(std::sin(k));b.process(std::sin(k));}
 auto before=a.capacitorState();
 for(double invalid:{-1.,0.,7999.,1536001.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
  check(!a.prepare(invalid,p,.4),"invalid rate rejected");
 for(int field=0;field<8;++field){auto bad=p;double* ptrs[]={&bad.parallelOhms,&bad.vr1Ohms,&bad.vr2Ohms,&bad.sourceOhms,&bad.dryLoadSiemens,&bad.heelOhms,&bad.toeOhms,&bad.opticalTauSeconds};*ptrs[field]=std::numeric_limits<double>::quiet_NaN();check(!a.prepare(96000,bad,.4),"nonfinite profile rejected");}
 // Every declared finite bound and nonfinite setup pedal must fail atomically.
 const std::array<double,8> low{10000,0,0,0,0,100,100,0};
 const std::array<double,8> high{10000000,25000,100000,100000,.001,10000000,10000000,1};
 for(int field=0;field<8;++field)for(int side=0;side<2;++side){
  auto bad=p;double* ptrs[]={&bad.parallelOhms,&bad.vr1Ohms,&bad.vr2Ohms,&bad.sourceOhms,&bad.dryLoadSiemens,&bad.heelOhms,&bad.toeOhms,&bad.opticalTauSeconds};
  *ptrs[field]=std::nextafter(side?high[field]:low[field],side?std::numeric_limits<double>::infinity():-std::numeric_limits<double>::infinity());
  check(!a.prepare(96000,bad,.4),"finite profile boundary violation rejected");
 }
 for(double invalid:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
  check(!a.prepare(96000,p,invalid),"nonfinite setup pedal rejected atomically");
  a.setPedal(invalid);a.setOpticalResistance(invalid);
 }
 a.setOpticalResistance(0);a.setOpticalResistance(1e8);
 for(int k=0;k<4;++k){check(before[k].voltage==a.capacitorState()[k].voltage,"invalid setup charge unchanged");check(before[k].current==a.capacitorState()[k].current,"invalid setup current unchanged");}
 for(int k=0;k<100;++k){auto x=values(a.process(.2)),y=values(b.process(.2));check(x==y,"invalid setup entire state unchanged");}
 auto z=a;auto bad=a;check(values(z.process(0))==values(bad.process(std::numeric_limits<double>::infinity())),"nonfinite input zeroed");
 z=a;bad=a;check(values(z.process(1e6))==values(bad.process(1e300)),"numerical input bound explicit");
 a.setPedal(-3);near(a.targetOpticalResistance(),p.heelOhms,"heel clamp");a.setPedal(3);near(a.targetOpticalResistance(),p.toeOhms,"toe clamp");
 a.reset(.3);for(auto cap:a.capacitorState())check(cap.voltage==0&&cap.current==0,"reset zero charge");
 near(a.opticalResistance(),std::exp(.7*std::log(p.heelOhms)+.3*std::log(p.toeOhms)),"reset no arbitrary startup optical sweep");
 for(int k=0;k<100;++k)for(double x:values(a.process(0)))check(x==0,"startup silence exactly zero");
 p.opticalTauSeconds=0;check(a.prepare(8000,p,0),"zero smoothing valid");a.setPedal(1);a.process(0);near(a.opticalResistance(),p.toeOhms,"zero smoothing immediate");
}
void atomicRollbackAndBoundaryControls(){
 using Profile=PraWahCircuit::Profile;
 constexpr std::array<double Profile::*,8> fields{
  &Profile::parallelOhms,&Profile::vr1Ohms,&Profile::vr2Ohms,&Profile::sourceOhms,
  &Profile::dryLoadSiemens,&Profile::heelOhms,&Profile::toeOhms,&Profile::opticalTauSeconds
 };
 constexpr std::array<double,8> low{10000,0,0,0,0,100,100,0};
 constexpr std::array<double,8> high{10000000,25000,100000,100000,.001,10000000,10000000,1};
 const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
 // Deliberately nondefault active state, charged and partway through a slew.
 // Rejected candidates differ in every field, rate and target, so premature
 // writes cannot hide behind the same nominal settings used by the old guard.
 const Profile active{220000,12345,34567,5432,.000123,700,7000000,.037};
 const Profile candidate{100000,23456,76543,9876,.000321,900000,300,.091};
 PraWahCircuit charged;
 check(charged.prepare(44100,active,.23),"rollback fixture setup");
 for(int n=0;n<401;++n)charged.process(.4*std::sin(n*.027));
 charged.setPedal(.86);
 for(int n=0;n<37;++n)charged.process(.3*std::cos(n*.013));
 check(charged.opticalResistance()!=charged.targetOpticalResistance(),"rollback fixture has pending optical slew");
 const auto sameState=[&](const PraWahCircuit& a,const PraWahCircuit& b){
  check(a.sampleRate()==b.sampleRate(),"rejection preserves sample rate");
  for(auto field:fields)check(a.getProfile().*field==b.getProfile().*field,"rejection preserves each profile field");
  check(a.opticalResistance()==b.opticalResistance(),"rejection preserves current optical resistance");
  check(a.targetOpticalResistance()==b.targetOpticalResistance(),"rejection preserves optical target");
  for(int k=0;k<4;++k){
   check(a.capacitorState()[k].voltage==b.capacitorState()[k].voltage,"rejection preserves charged voltage");
   check(a.capacitorState()[k].current==b.capacitorState()[k].current,"rejection preserves capacitor current");
  }
 };
 const auto sameContinuation=[&](PraWahCircuit a,PraWahCircuit b){
  sameState(a,b);
  for(int n=0;n<193;++n){
   // A later valid pedal change also exercises cached endpoint logarithms.
   if(n==97){a.setPedal(.31);b.setPedal(.31);}
   check(values(a.process(.2*std::sin(n*.11)))==values(b.process(.2*std::sin(n*.11))),"rejected operation leaves future samples bit-identical");
  }
  sameState(a,b);
 };
 const auto rejects=[&](double rate,const Profile& profile,double pedal){
  auto a=charged;
  check(!a.prepare(rate,profile,pedal),"invalid replacement rejected");
  sameContinuation(a,charged);
 };
 for(std::size_t k=0;k<fields.size();++k){
  for(double value:{-inf,inf,nan,std::nextafter(low[k],-inf),std::nextafter(high[k],inf)}){
   auto p=candidate;p.*fields[k]=value;
   check(!PraWahCircuit::validProfile(p),"invalid profile fails independent validation");
   rejects(192000,p,.67);
  }
  // Every declared inclusive endpoint is accepted, not merely the defaults.
  for(double value:{low[k],high[k]}){
   auto p=candidate;p.*fields[k]=value;PraWahCircuit a;
   check(PraWahCircuit::validProfile(p)&&a.prepare(192000,p,.67),"inclusive profile boundary accepted");
   for(double x:values(a.process(.1)))check(std::isfinite(x),"profile boundary output finite");
  }
 }
 for(double rate:{-inf,inf,nan,std::nextafter(PraWahCircuit::minimumRate,-inf),std::nextafter(PraWahCircuit::maximumRate,inf)})
  rejects(rate,candidate,.67);
 for(double pedal:{-inf,inf,nan})rejects(192000,candidate,pedal);
 for(double pedal:{-inf,inf,nan}){auto a=charged;a.setPedal(pedal);sameContinuation(a,charged);}
 for(double ohms:{-inf,inf,nan,std::nextafter(PraWahCircuit::minimumOpticalOhms,-inf),std::nextafter(PraWahCircuit::maximumOpticalOhms,inf)}){
  auto a=charged;a.setOpticalResistance(ohms);sameContinuation(a,charged);
 }
 for(double rate:{PraWahCircuit::minimumRate,PraWahCircuit::maximumRate}){
  auto a=charged;PraWahCircuit fresh;
  check(a.prepare(rate,candidate,.67)&&fresh.prepare(rate,candidate,.67),"inclusive rate boundary accepted");
  sameContinuation(a,fresh); // Valid replacement must discard every old state.
 }
 for(double pedal:{-std::numeric_limits<double>::max(),std::numeric_limits<double>::max()}){
  PraWahCircuit a;check(a.prepare(96000,active,pedal),"finite extreme setup pedal clamped");
  near(a.opticalResistance(),pedal<0?active.heelOhms:active.toeOhms,"prepare initializes clamped reversed-polarity endpoint");
 }
 for(double ohms:{PraWahCircuit::minimumOpticalOhms,PraWahCircuit::maximumOpticalOhms}){
  auto a=charged;a.setOpticalResistance(ohms);near(a.targetOpticalResistance(),ohms,"direct optical endpoint accepted");
  check(a.opticalResistance()==charged.opticalResistance(),"valid optical target does not jump current state");
  a.reset();near(a.opticalResistance(),ohms,"reset selects direct optical endpoint immediately");
 }
 for(double invalid:{-inf,inf,nan}){
  auto a=charged,b=charged;
  a.reset(invalid);b.reset();sameContinuation(a,b); // Invalid pedal ignored; explicit reset still occurs.
  a=charged;b=charged;
  check(values(a.process(invalid))==values(b.process(0)),"all nonfinite audio input becomes zero");
 }
 auto a=charged,b=charged;
 check(values(a.process(-std::numeric_limits<double>::max()))==values(b.process(-PraWahCircuit::maximumInputMagnitude)),"negative finite audio is clamped symmetrically");
}
void opticalAndCharge(){
 PraWahCircuit a;PraWahCircuit::Profile p;a.prepare(96000,p,0);
 for(int k=0;k<4000;++k)a.process(std::sin(k*.013));
 auto before=a.capacitorState();a.setPedal(1);
 for(int k=0;k<4;++k){check(before[k].voltage==a.capacitorState()[k].voltage,"pedal setter preserves charge");check(before[k].current==a.capacitorState()[k].current,"pedal setter preserves current");}
 for(int k=0;k<1152;++k)a.process(0);
 near(std::log(a.opticalResistance()),std::log(p.toeOhms)+(std::log(p.heelOhms)-std::log(p.toeOhms))*std::exp(-1.),"12ms is log-R time constant",2e-12);
 before=a.capacitorState();a.setOpticalResistance(12345);
 for(int k=0;k<4;++k){check(before[k].voltage==a.capacitorState()[k].voltage,"resistance setter preserves charge");check(before[k].current==a.capacitorState()[k].current,"resistance setter preserves current");}
}
void independentTransient(){
 std::mt19937 rng(0x80);std::uniform_real_distribution<double> un(0,1);
 for(double fs:{8000.,88200.,192000.,1536000.})for(int fixture=0;fixture<8;++fixture){
  PraWahCircuit::Profile p;p.opticalTauSeconds=fixture%2?.012:0;p.sourceOhms=fixture==0?0:100000*un(rng);p.parallelOhms=std::exp(std::log(1e4)+un(rng)*std::log(1e3));p.vr1Ohms=fixture==0?0:25000*un(rng);p.vr2Ohms=fixture==1?0:100000*un(rng);p.dryLoadSiemens=fixture==0?0:.001*un(rng);
  PraWahCircuit a;a.prepare(fs,p,.5);TransientOracle oracle;
  for(int n=0;n<700;++n){if(n%13==0)a.setOpticalResistance(std::exp(std::log(100.)+un(rng)*std::log(1e5)));double u=2*un(rng)-1;auto before=a.capacitorState();auto out=a.process(u);auto expected=oracle.tick(u,fs,p,a.opticalResistance());auto got=values(out);
   for(int k=0;k<9;++k)near(got[k],expected[k],"independent transient nodes",2e-8);
   auto after=a.capacitorState();for(int k=0;k<4;++k){near(after[k].voltage,oracle.v[k],"independent capacitor voltage",2e-8);near(after[k].current,oracle.i[k],"independent capacitor current",2e-8);}
   const double rt=p.vr1Ohms+1/(1/a.opticalResistance()+1/p.parallelOhms);
   near(after[0].current,out.x*1e-4+(out.x-out.n)*1e-4+p.dryLoadSiemens*out.x,"input KCL",1e-8);
   near((out.n-out.x)*1e-4+out.n*1e-4+(out.n-out.c)/470,0,"feedback KCL");
   near(after[1].current,(out.a-out.p)/rt,"P KCL");near(after[2].current,(out.b-out.q)*1e-4,"Q KCL");near(after[3].current+out.d*2e-4,0,"D KCL",1e-8);
   constexpr std::array<double,4> cap{10e-6,2.2e-9,33e-9,1e-6};for(int k=0;k<4;++k)near(cap[k]*(after[k].voltage-before[k].voltage)*2*fs,after[k].current+before[k].current,"physical charge trapezoid continuity",1e-8);
  }
 }
}
std::array<C,9> measured(const PraWahCircuit::Profile& p,double fs,double f,double optical){
 PraWahCircuit a;a.prepare(fs,p,0);a.setOpticalResistance(optical);a.reset();std::array<C,9> sum{};
 const int settle=static_cast<int>(fs*2), count=static_cast<int>(fs/4);
 for(int n=0;n<settle+count;++n){const double phase=2*std::numbers::pi*f*n/fs;auto v=values(a.process(std::cos(phase)));if(n>=settle){C basis=std::polar(2.0/count,-phase);for(int k=0;k<9;++k)sum[k]+=v[k]*basis;}}
 return sum;
}
void acAndConvergence(){
 double maxError=0;
 for(double fs:{48000.,88200.,192000.})for(int fixture=0;fixture<4;++fixture)for(double f:{8.,332.,3400.,18000.}){
  PraWahCircuit::Profile p;p.opticalTauSeconds=0;double optical=fixture%2?2000:1000000;
  if(fixture==1){p.sourceOhms=0;p.vr1Ohms=0;p.vr2Ohms=100000;}
  if(fixture==2){p.sourceOhms=10000;p.parallelOhms=100000;p.vr1Ohms=12500;p.dryLoadSiemens=1e-4;}
  if(fixture==3){p.vr2Ohms=0;p.parallelOhms=1000000;p.vr1Ohms=25000;}
  auto got=measured(p,fs,f,optical);
  const double warped=fs/std::numbers::pi*std::tan(std::numbers::pi*f/fs);
  auto expected=values(cs80::reference::pra_wah::solve(boundary(p,optical),warped,1.));
  for(int k=0;k<9;++k){near(got[k],expected[k],"measured AC vs independently MNA-verified oracle",1e-7);maxError=std::max(maxError,std::abs(got[k]-expected[k]));}
 }
 std::cout<<"maximum measured complex AC error "<<maxError<<'\n';
 // Same physical frequency, NOT prewarped, errors should approach continuous graph.
 for(double f:{332.,3400.,12000.}){
  PraWahCircuit::Profile p;p.opticalTauSeconds=0;const double optical=f<1000?1e6:2000.;auto exact=cs80::reference::pra_wah::solve(boundary(p,optical),f,1.).y;
  double previous=1e100;for(double fs:{48000.,96000.,192000.,384000.}){auto got=measured(p,fs,f,optical)[8];double error=std::abs(got-exact);check(error<previous*.35,"unwarped frequency converges quadratically");previous=error;}
 }
}
void lifecycleAndSweeps(){
 // Circuit intentionally has no bypass/freeze API: caller selects X or Y while
 // continuing process. Two different selection histories must share all state.
 PraWahCircuit a,b;PraWahCircuit::Profile p;a.prepare(96000,p,.4);b=a;
 double maxPeak=0;
 for(int n=0;n<192000;++n){double pedal=.5+.5*std::sin(n*.0013);if(n%53==0){a.setPedal(pedal);b.setPedal(pedal);}double u=.3*std::sin(n*.031)+.2*std::sin(n*.008);auto x=a.process(u);auto y=b.process(u);const double selected=n<96000?x.x:x.y;check(std::isfinite(selected),"dry to wet continuously-clocked finite");check(values(x)==values(y),"selection does not alter circuit history");maxPeak=std::max(maxPeak,std::abs(x.y));}
 check(maxPeak<10,"nominal dynamic fixture bounded (not universal analog gain bound)");
 auto saved=a.capacitorState();for(int k=0;k<4;++k)check(saved[k].voltage==b.capacitorState()[k].voltage,"bypass history matches continuously clocked reference");
 // Profile sensitivity and extreme rates. Absolute bound here is deliberately
 // loose and applies only to these bounded-input fixtures, not all modulation.
 double extremePeak=0;
 for(double fs:{8000.,96000.,1536000.})for(int fixture=0;fixture<24;++fixture){
  PraWahCircuit::Profile q;q.parallelOhms=std::array{1e4,1e5,2.2e5,4.7e5,1e6,1e7}[fixture%6];q.vr1Ohms=std::array{0.,1000.,12500.,25000.}[fixture%4];q.vr2Ohms=std::array{0.,10000.,20000.,50000.,100000.}[fixture%5];q.sourceOhms=std::array{0.,1000.,5000.,10000.,100000.}[fixture%5];q.dryLoadSiemens=std::array{0.,1e-5,1e-4,.001}[fixture%4];q.opticalTauSeconds=std::array{0.,.003,.012,.03,.1,1.}[fixture%6];q.heelOhms=fixture%2?100:1e7;q.toeOhms=fixture%2?1e7:100;
  PraWahCircuit z;check(z.prepare(fs,q,0),"supported extreme profile");for(int n=0;n<12000;++n){if(n%97==0)z.setPedal((n/97)%2);auto out=z.process(n<6000?std::sin(n*.137):0);for(double val:values(out))check(std::isfinite(val),"dynamic extreme nodes finite");extremePeak=std::max(extremePeak,std::abs(out.y));}
 }
 check(extremePeak<100,"tested sensitivity trajectories bounded");std::cout<<"dynamic fixture peaks nominal="<<maxPeak<<" sensitivity="<<extremePeak<<'\n';
 // DC input disappears through coupling capacitors; zero input decays afterward.
 a.prepare(8000,p,.3);for(int n=0;n<160000;++n)a.process(1);auto dc=a.process(1);near(dc.x,0,"input DC rejected",1e-8);near(dc.y,0,"output DC rejected",1e-8);
 for(int n=0;n<160000;++n) a.process(0);
 near(a.process(0).y,0,"tail decays without state reset",1e-8);
}

std::vector<std::array<double,9>> physicalTrajectory(double fs) {
 PraWahCircuit a;PraWahCircuit::Profile p;p.opticalTauSeconds=0;a.prepare(fs,p,.5);
 std::vector<std::array<double,9>> samples;
 const int stride=static_cast<int>(fs/12000.0), length=static_cast<int>(fs*.08);
 for(int n=1;n<=length;++n){
  const double t=n/fs;
  a.setPedal(.5+.4*std::sin(2*std::numbers::pi*20*t));
  const double u=std::sin(2*std::numbers::pi*900*t)*(-std::expm1(-200*t));
  auto nodes=a.process(u);
  if(n%stride==0)samples.push_back(values(nodes));
 }
 return samples;
}
void physicalTimeConvergenceAndAllocation(){
 // Identical continuous input/control curves sampled at identical physical
 // observation times. The independent transient nodal tests above establish
 // the graph; refinement here tests that moving coefficients converge too.
 const auto reference=physicalTrajectory(1536000.);
 double previous=1e100;
 std::cout<<"dynamic trajectory RMS errors";
 for(double fs:{48000.,96000.,192000.,384000.}){
  const auto samples=physicalTrajectory(fs);check(samples.size()==reference.size(),"common physical observation times");
  double square=0;for(std::size_t n=0;n<samples.size();++n)for(int k=0;k<9;++k){double e=samples[n][k]-reference[n][k];square+=e*e;}
  const double rms=std::sqrt(square/(samples.size()*9));
  std::cout<<' '<<rms;check(rms<previous*.35,"time-varying circuit physical-time convergence");previous=rms;
 }
 std::cout<<'\n';
 PraWahCircuit a;const auto before=allocations;countAllocations=true;
 double witness=0;
 for(int n=0;n<5000;++n){a.setPedal((n%100)/99.);witness+=a.process(std::sin(n*.1)).y;}
 countAllocations=false;
 check(allocations==before,"process and controls allocate no C++ heap memory");check(std::isfinite(witness),"allocation probe actually processes output");
}
} // namespace
int main(){setupAndGuards();atomicRollbackAndBoundaryControls();opticalAndCharge();independentTransient();acAndConvergence();lifecycleAndSweeps();physicalTimeConvergenceAndAllocation();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

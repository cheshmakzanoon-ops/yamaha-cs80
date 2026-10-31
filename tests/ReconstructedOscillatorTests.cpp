// Conditional numerical qualification of an INFERRED instrument, not measured
// CS80 equivalence. Frozen proposal gates and exploratory history are documented.
#include "core/ReconstructedOscillator.h"
#include "core/Dsp.h"
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <string>
#include <iostream>
#include <limits>
#include <new>
#include <vector>
namespace { bool countAllocations=false;std::size_t allocationCount=0; }
#if defined(__GNUC__) || defined(__clang__)
#define OSC_TEST_NOINLINE __attribute__((noinline))
#else
#define OSC_TEST_NOINLINE
#endif
OSC_TEST_NOINLINE void* operator new(std::size_t n){if(countAllocations)++allocationCount;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
OSC_TEST_NOINLINE void* operator new[](std::size_t n){return ::operator new(n);}
OSC_TEST_NOINLINE void operator delete(void* p) noexcept{std::free(p);}OSC_TEST_NOINLINE void operator delete[](void* p) noexcept{std::free(p);}
OSC_TEST_NOINLINE void operator delete(void* p,std::size_t) noexcept{std::free(p);}OSC_TEST_NOINLINE void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
#undef OSC_TEST_NOINLINE
namespace {
using Osc=cs80::ReconstructedOscillator;using Out=cs80::WscOutputVolts;using Complex=std::complex<double>;
// Exact copied-state comparisons share one runtime kernel, avoiding distinct
// compiler specialization/FMA choices at separate inlined call sites.
#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
Out sharedTickKernel(Osc& oscillator,double hz,const cs80::WscControls& controls) noexcept {
 return oscillator.tick(hz,controls);
}
using TickFunction=Out (*)(Osc&,double,const cs80::WscControls&) noexcept;
TickFunction volatile sharedTick=sharedTickKernel;
constexpr double pi=3.14159265358979323846;
// Implementation acceptance thresholds, NOT recovered analog tolerances.
constexpr double timingToleranceSeconds=1e-9,frequencyToleranceCents=.01;
constexpr double selectedAnchorRelativeTolerance=.001;
constexpr double aliasGateDb=-70.,hostBandResidualGateDb=-45.;
int checks=0,failures=0;
void check(bool condition,const char* message){++checks;if(!condition){if(failures<30)std::cerr<<message<<'\n';++failures;}}
void near(double a,double b,double tolerance,const char* message){check(std::abs(a-b)<=tolerance,message);}
std::array<double,3> values(const Out& x){return {x.sawPin5,x.pulsePin7,x.sinePin2};}
void setupAllocationAndProfileBounds(){
 allocationCount=0;countAllocations=true;{Osc cold;check(cold.prepare(48000.,{}),"setup preparation succeeds with fixed storage");}countAllocations=false;check(allocationCount==0,"zero setup allocations including initial shared kernel");
 for(double fs:{8000.,44100.,192000.,1536000.})for(double rc:{.001,.1034,10.}){
  Osc o;cs80::OscillatorNominalProfile profile;profile.couplingSecondsNominal=rc;check(o.prepare(fs,profile),"supported RC/rate corner prepares");
  for(int n=0;n<4000;++n){cs80::WscControls c;c.pulseThresholdVolts=2.*std::sin(n*.083);auto v=o.tick(n%47<20?0.:o.maximumFrequencyHz()*(.1+.9*double(n%31)/30.),c);for(double x:values(v))check(std::isfinite(x),"supported profile corner finite");check(!o.getDiagnostics().eventCapacityExceeded,"bounded roots/event count at profile corners");}
 }
}
void timingAndControls(){
 for(double fs:{88200.,96000.,176400.,192000.,352800.,384000.})for(double f:{32.7032,130.8,261.6,523.2,1046.,2093.,4186.,8372.,16744.}){
  Osc o;o.prepare(fs,{});o.reset(0.,f);double previousReset=0.,lastStart=0.;int cycles=0;
  for(int n=0;n<int(fs*.08)+100;++n){o.tick(f);const auto& d=o.getDiagnostics();check(!d.eventCapacityExceeded,"bounded event capacity");
   for(std::size_t i=0;i<d.eventCount;++i){const auto& e=d.events[i];if(e.kind==Osc::resetStart){if(cycles){const double cents=1200.*std::log2(1./((e.absoluteSeconds-previousReset)*f));near(cents,0.,frequencyToleranceCents,"charge cycle frequency < .01 cent");}previousReset=lastStart=e.absoluteSeconds;++cycles;near(e.charge,0.,1e-10,"restoring pulse begins at zero charge");}
    if(e.kind==Osc::resetEnd){near(e.absoluteSeconds-lastStart,30e-6,timingToleranceSeconds,"fractional reset duration within 1ns");near(e.charge,1.-f*30e-6,1e-10,"charge conservation at reset completion");}}
  }
 }
 Osc o;o.prepare(96000.,{});o.reset(0.,261.6);o.tick(261.6);const double remaining=o.getState().resetRemainingSeconds;const auto count=o.getState().resetCount;o.tick(1046.);near(o.getState().resetRemainingSeconds,remaining-1./96000.,1e-15,"pitch change does not restart reset timer");check(o.getState().resetCount==count,"pitch change does not retrigger reset");o.tick(0.);check(o.getState().resetRemainingSeconds==0.,"running reset completes at zero current");const double q=o.getState().charge;for(int i=0;i<100;++i)o.tick(0.);near(o.getState().charge,q,0.,"zero current freezes ramp charge");
 const auto held=o.getState();cs80::WscControls bad;bad.pulseThresholdVolts=std::numeric_limits<double>::infinity();o.tick(std::numeric_limits<double>::quiet_NaN(),bad);check(o.getDiagnostics().invalidControl,"nonfinite transport flag");near(o.getState().heldFrequencyHz,held.heldFrequencyHz,0.,"retain last finite frequency");near(o.getState().heldThresholdVolts,held.heldThresholdVolts,0.,"retain last finite threshold");o.tick(1e9);check(o.getDiagnostics().frequencyLimited,"extrapolation limit reported");near(o.getState().heldFrequencyHz,o.maximumFrequencyHz(),0.,"declared extrapolation bound");
 o.reset(0.,0.);o.tick(0.);near(o.getDiagnostics().raw.pulsePin7,-1.5,0.,"stopped exact comparator equality is low");check(o.getState().resetRemainingSeconds==0.,"stopped zero charge does not trigger reset");
}
void profileAndPins(){
 cs80::OscillatorNominalProfile p;check(Osc::validProfile(p),"nominal profile valid");{auto high=p;high.referenceHzNominal=10000.;Osc low;check(low.prepare(8000.,high),"supported high-reference profile low-rate preparation");low.reset(0.,std::numeric_limits<double>::quiet_NaN());check(low.getState().heldFrequencyHz<=low.maximumFrequencyHz(),"invalid reset fallback remains frequency bounded");}
 for(int k=0;k<=4;++k){const double x=.25*k;near(Osc::shape(x),p.shaperOrdinateInferred[k],1e-15,"PWL knot exact");near(Osc::shape(-x),-Osc::shape(x),1e-15,"odd PWL");}
 near(Osc::shape(2.),1.,0.,"explicit shaper upper clamp");near(Osc::shape(-2.),-1.,0.,"explicit shaper lower clamp");
 Osc o;o.prepare(192000.,p);for(int n=0;n<192000;++n)o.tick(261.6);
 std::array<double,3> lo{1e9,1e9,1e9},hi{-1e9,-1e9,-1e9};double pinlo=1e9,pinhi=-1e9,trilo=1e9,trihi=-1e9;
 for(int n=0;n<192000;++n){o.tick(261.6);const auto& d=o.getDiagnostics();auto v=values(d.raw);for(int b=0;b<3;++b){lo[b]=std::min(lo[b],v[b]);hi[b]=std::max(hi[b],v[b]);}pinlo=std::min(pinlo,d.rawPin7Volts);pinhi=std::max(pinhi,d.rawPin7Volts);trilo=std::min(trilo,d.trianglePin3Volts);trihi=std::max(trihi,d.trianglePin3Volts);check(d.raw.pulsePin7==1.5||d.raw.pulsePin7==-1.5,"raw comparator exactly two levels");}
 near(pinhi-pinlo,3.5,3.5*selectedAnchorRelativeTolerance,"selected pin7 3.5 Vpp");near(trihi-trilo,3.2,3.2*selectedAnchorRelativeTolerance,"selected triangle 3.2 Vpp");near(hi[2]-lo[2],3.,3.*selectedAnchorRelativeTolerance,"fixed sine gain 3 Vpp at reference");
 const auto before=o.getState();cs80::WscControls disabled;disabled.sawEnabled=disabled.pulseEnabled=false;for(int n=0;n<100;++n)o.tick(261.6,disabled);check(o.getState().charge!=before.charge,"waveform gates do not freeze charge");check(o.getDiagnostics().raw.pulsePin7==1.5||o.getDiagnostics().raw.pulsePin7==-1.5,"physical pin diagnostics remain ungated");
 auto invalid=p;invalid.resetSecondsNominal=-1.;auto a=o,b=o;check(!a.prepare(48000.,invalid),"invalid prepare rejected");for(int n=0;n<100;++n)check(values(sharedTick(a,123.,{}))==values(sharedTick(b,123.,{})),"invalid prepare transactional");
}
// Independent periodic continuous model. No production scheduler or tick;
// solve the driven first RC exactly over a plateau and linear ramp, then use
// convergent trapezoidal integration for the second RC on a dense period grid.
struct PeriodicReference {
 double f,period,rc=.1034,reset=30e-6,gain=3.05/(1.-261.6*30e-6),b0=0.,bReset=0.;
 std::vector<std::array<double,3>> samples;
 PeriodicReference(double frequency,std::size_t size):f(frequency),period(1./f),samples(size){
  const double e0=std::exp(-reset/rc),duration=period-reset,e1=std::exp(-duration/rc),start=gain*(1.-f*reset),slope=-gain*f;
  const double bAfter0=3.5*(1.-e0);
  const double bEnd0=start+slope*duration-slope*rc+(bAfter0-start+slope*rc)*e1;
  b0=bEnd0/(-std::expm1(-period/rc));bReset=3.5+(b0-3.5)*e0;
  std::vector<double> triangle(size);for(std::size_t i=0;i<size;++i){const double t=period*double(i)/double(size),u=input(t);triangle[i]=3.2*std::min(1.,std::abs(u)/1.525);samples[i]={-u,u>0.?1.5:-1.5,0.};}
  const double h=period/double(size),a=h/(2*rc+h),r=1.-2*a;double baseline=0.;
  for(std::size_t i=0;i<size;++i)baseline=r*baseline+a*(triangle[i]+triangle[(i+1)%size]);
  baseline/=1.-std::pow(r,double(size));
  for(std::size_t i=0;i<size;++i){const double x=(triangle[i]-baseline)/1.6,ax=std::min(1.,std::abs(x));const int k=std::min(3,int(ax*4));const double fraction=ax*4-k;
   const double shape=std::sin(pi*k/8.)+fraction*(std::sin(pi*(k+1)/8.)-std::sin(pi*k/8.));samples[i][2]=-1.5017767959*std::copysign(shape,x);baseline=r*baseline+a*(triangle[i]+triangle[(i+1)%size]);}
 }
 double input(double t)const{if(t<reset)return (3.5-b0)*std::exp(-t/rc);const double start=gain*(1.-f*reset),slope=-gain*f;return slope*rc+(start-bReset-slope*rc)*std::exp(-(t-reset)/rc);}
 std::array<Complex,3> harmonic(int k)const{std::array<Complex,3> sum{};const Complex step=std::polar(1.,-2*pi*k/double(samples.size()));Complex phase=1.;for(const auto& s:samples){for(int b=0;b<3;++b)sum[b]+=s[b]*phase;phase*=step;}for(auto& v:sum)v/=double(samples.size());return sum;}
};
void independentComparator(){
 constexpr double fs=96000.;
 for(double f:{130.8,261.6,4186.,8372.}){PeriodicReference reference(f,1<<10);const double period=1./f,reset=30e-6,rc=.1034,k=3.05/(1.-261.6*reset),s=-k*f;
  for(double proxy:{-.25,.05,.10,.25,.50,.75,.90,.95,1.25}){const double threshold=1.525*(1.-2*proxy);cs80::WscControls controls;controls.pulseThresholdVolts=threshold;
   Osc o;o.prepare(fs,{});o.reset(0.,f);for(int i=0;i<int(3*fs);++i)o.tick(f,controls);
   std::vector<double> roots;const double u0=reference.input(0.),uResetLeft=u0*std::exp(-reset/rc),uResetRight=reference.input(reset),uEnd=s*rc+(k*(1.-f*reset)-reference.bReset-s*rc)*std::exp(-(period-reset)/rc);
   if((uEnd>threshold)!=(u0>threshold))roots.push_back(0.);
   if((uResetLeft>threshold)!=(uResetRight>threshold))roots.push_back(reset);
   if((u0-threshold)*(uResetLeft-threshold)<0.)roots.push_back(-rc*std::log(threshold/u0));
   if((uResetRight-threshold)*(uEnd-threshold)<0.)roots.push_back(reset-rc*std::log((threshold-s*rc)/(uResetRight-s*rc)));
   int edges=0;for(int i=0;i<int(fs*.025);++i){o.tick(f,controls);auto& d=o.getDiagnostics();for(std::size_t j=0;j<d.eventCount;++j){auto& e=d.events[j];if(e.before.pulsePin7==e.after.pulsePin7)continue;++edges;double phase=std::fmod(e.absoluteSeconds,period),distance=period;for(double root:roots){const double delta=std::abs(phase-root);distance=std::min(distance,std::min(delta,period-delta));}check(distance<10e-9,"independent static comparator root within 10ns");}}
   check(roots.empty()?edges==0:edges>0,"comparator edges match independent topology including outside/reset-only thresholds");
  }
 }
}
void referenceConvergence(){
 for(double f:{261.6,8372.,16744.}){PeriodicReference coarse(f,1<<18),fine(f,1<<19);for(int h:{0,1,2,3,7}){auto a=coarse.harmonic(h),b=fine.harmonic(h);for(int branch=0;branch<3;++branch)check(std::abs(a[branch]-b[branch])<4e-5,"independent periodic grid convergence");}}
}
void independentNumerics(){
 // This is the ACTUAL 2x engine host passband, not an unqualified direct-
 // oscillator full-Nyquist pass. Direct transition-band limits are disclosed.
 constexpr std::size_t n=16384;double worstResidual=-1e9,worstAlias=-1e9;
 for(double fs:{88200.,96000.,176400.,192000.,352800.,384000.})for(double target:{1046.,4186.,8372.,16744.}){
  const int bin=int(std::round(target*double(n)/fs));const double f=fs*bin/double(n);const int harmonics=int((fs*.25)/f);
  PeriodicReference ref(f,1<<19);Osc o;o.prepare(fs,{});o.reset(0.,f);const int settle=int(fs*1.5);for(int i=0;i<settle;++i)o.tick(f);
  std::vector<std::array<double,3>> output(n);for(auto& row:output)row=values(o.tick(f));
  std::array<double,3> error{},power{},off{};
  // Independent time-domain DFT, sparse harmonics and deliberately off-grid
  // image bins; use complex phase, one shared fixed latency, no fitted means.
  for(int h=0;h<=harmonics;++h){auto expected=ref.harmonic(h);const Complex shift=std::polar(1.,2*pi*f*(double(settle)+1.-Osc::latencyInternalSamples())*h/fs);std::array<Complex,3> measured{};const auto step=std::polar(1.,-2*pi*bin*h/double(n));Complex phase=1.;for(auto row:output){for(int b=0;b<3;++b)measured[b]+=row[b]*phase;phase*=step;}for(int b=0;b<3;++b){measured[b]/=double(n);expected[b]*=shift;error[b]+=std::norm(measured[b]-expected[b]);power[b]+=std::norm(expected[b]);}}
  for(int testbin:{1,3,17,37,129,511,1023,2047,4093}){if(testbin%bin==0)continue;std::array<Complex,3> measured{};Complex phase=1.,step=std::polar(1.,-2*pi*testbin/double(n));for(auto row:output){for(int b=0;b<3;++b)measured[b]+=row[b]*phase;phase*=step;}for(int b=0;b<3;++b)off[b]+=std::norm(measured[b]/double(n));}
  for(int b=0;b<3;++b){const double db=10*std::log10(error[b]/power[b]),alias=10*std::log10(off[b]/power[b]);worstResidual=std::max(worstResidual,db);worstAlias=std::max(worstAlias,alias);check(db<hostBandResidualGateDb,"independent complex harmonic residual < -45 dB in actual 2x host band");check(alias<aliasGateDb,"sampled off-harmonic probes < -70 dBc");}
 }
 std::cout<<"Independent host-band complex residual worst "<<worstResidual<<" dB; sampled off-harmonic probes "<<worstAlias<<" dB relative selected in-band power (sparse probes, not exhaustive alias energy)\n";
}
void fft(std::vector<Complex>& a){const auto n=a.size();for(std::size_t i=1,j=0;i<n;++i){auto bit=n>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j)std::swap(a[i],a[j]);}for(std::size_t len=2;len<=n;len<<=1){auto root=std::polar(1.,-2*pi/double(len));for(std::size_t i=0;i<n;i+=len){Complex w=1.;for(std::size_t j=0;j<len/2;++j){auto u=a[i+j],v=a[i+j+len/2]*w;a[i+j]=u+v;a[i+j+len/2]=u-v;w*=root;}}}for(auto& x:a)x/=double(n);}
void actualDecimatorPath(){
 constexpr int n=8192;double worst=-1e9,worstAlias=-1e9;
 for(double fs:{88200.,96000.,176400.,192000.,352800.,384000.})for(double target:{1046.,4186.,8372.,16744.}){
  const int bin=int(std::round(target*2*n/fs));const double f=fs*bin/(2*n);PeriodicReference ref(f,1<<19);
  Osc o;o.prepare(fs,{});o.reset(0.,f);std::array<cs80::Decimator,3> filters;for(auto& filter:filters)filter.prepare(float(fs));
  const int settle=int(fs*1.5)/2*2;for(int i=0;i<settle;++i){auto v=values(o.tick(f));for(int b=0;b<3;++b)filters[b].push(float(v[b]));}
  std::array<std::vector<Complex>,3> y,e,desired;for(int b=0;b<3;++b){y[b].resize(n);e[b].resize(n);desired[b].resize(n);}
  for(int k=0;k<n;++k){for(int phase=0;phase<2;++phase){auto v=values(o.tick(f));for(int b=0;b<3;++b)filters[b].push(float(v[b]));}for(int b=0;b<3;++b)y[b][k]=filters[b].output();}
  for(int b=0;b<3;++b)fft(y[b]);
  const auto& coefficients=filters[0].getCoefficients();
  for(int h=0;h<=int(fs*.5/f);++h){auto reference=ref.harmonic(h);Complex response=0.;for(std::size_t tap=0;tap<coefficients.size();++tap)response+=double(coefficients[tap])*std::polar(1.,-2*pi*f*h*double(tap)/fs);
   const auto shift=std::polar(1.,2*pi*f*h*(double(settle)+2.-Osc::latencyInternalSamples())/fs);const int k=(h*bin)%n;
   for(int b=0;b<3;++b){const auto amplitude=reference[b]*shift*response;e[b][k]+=amplitude;if(h)e[b][(n-k)%n]+=std::conj(amplitude);if(f*h<=fs*.25){desired[b][k]+=amplitude;if(h)desired[b][(n-k)%n]+=std::conj(amplitude);}}}
  for(int b=0;b<3;++b){double residual=0.,responseResidual=0.,power=0.,alias=0.;for(int k=1;k<n;++k){residual+=std::norm(y[b][k]-desired[b][k]);responseResidual+=std::norm(y[b][k]-e[b][k]);power+=std::norm(desired[b][k]);if(std::norm(desired[b][k])==0.)alias+=std::norm(y[b][k]);}
   const double fundamental=2.*std::norm(desired[b][bin]);const double db=10*std::log10(residual/power),adb=10*std::log10(alias/fundamental);worst=std::max(worst,db);worstAlias=std::max(worstAlias,adb);
   check(db<hostBandResidualGateDb,"ACTUAL 255tap 2x decimator against ideal-hostband with declared inband response < -45dB");check(10*std::log10(responseResidual/power)<hostBandResidualGateDb,"actual decimator response including transition leakage agrees");check(adb<aliasGateDb,"ACTUAL decimator total offharmonic energy relative fundamental < -70dBc");}

 }
 std::cout<<"Actual255tap 2x chain ideal-hostband residual worst "<<worst<<" dB; total off-harmonic energy "<<worstAlias<<" dBc\n";
}
void stateAndKernel(){
 const auto& kernel=cs80::OscillatorResidualKernel::shared();Osc stopped;stopped.reset(0.,0.);for(int n=0;n<32;++n)near(stopped.tick(0.).pulsePin7,-1.5*(1.+kernel.at(double(n+1)).step),2e-12,"zero audio prehistory startup convolution");near(kernel.firstMomentSamples(),Osc::latencyInternalSamples(),1e-10,"kernel first moment equals declared linear-phase delay");near(kernel.at(1000.).step,0.,0.,"BLEP tail zero");near(kernel.at(1000.).ramp,0.,0.,"moment-corrected BLAMP tail zero");near(kernel.at(0.).step,-1.,0.,"causal step residual initial condition");
 Osc a;a.prepare(96000.,{});for(int n=0;n<33;++n)a.tick(8372.);auto b=a;for(int n=0;n<1000;++n){const auto f=n%71<30?8372.:1046.;cs80::WscControls c;c.pulseThresholdVolts=.8*std::sin(n*.17);check(values(sharedTick(a,f,c))==values(sharedTick(b,f,c)),"complete copied state preserves overlapping AA/capacitor history");}
 countAllocations=true;for(int n=0;n<10000;++n){cs80::WscControls c;c.pulseThresholdVolts=1.525*std::sin(n*.019);const auto out=a.tick(50.+(n%4000)*4.,c);for(double x:values(out))check(std::isfinite(x)&&std::abs(x)<20.,"modulation finite and bounded");}countAllocations=false;check(allocationCount==0,"zero callback allocations");
}
void benchmark(){
 std::array<Osc,16> bank;for(std::size_t i=0;i<bank.size();++i){bank[i].prepare(96000.,{});bank[i].reset(double(i)/bank.size(),8372.);}
 volatile double sink=0.;std::array<double,200> times{};for(auto& ms:times){auto begin=std::chrono::steady_clock::now();for(int n=0;n<512;++n)for(auto& o:bank){auto x=o.tick(8372.);sink+=x.sawPin5+x.pulsePin7+x.sinePin2;}ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}
 std::sort(times.begin(),times.end());std::cout<<"16 oscillators, 48k host/2x, high note, 256-host block: median "<<times[100]<<" ms, p99 "<<times[198]<<" ms, max "<<times.back()<<" ms; host block deadline 5.333ms; sink "<<sink<<'\n';
}
// Auditable standalone listening path; three mono IEEE-float WAVs, same fixed
// 4.0 volts/full-scale adapter on EVERY pin, actual production2x decimator.
// No independently fitted gain/phase or waveform normalization is applied.
int render(const char* prefix,double hostRate,double frequency,double seconds){
 if(!std::isfinite(hostRate)||hostRate<44100.||hostRate>192000.||!std::isfinite(frequency)||frequency<0.||!std::isfinite(seconds)||seconds<=0.||seconds>60.)return 2;
 const auto count=static_cast<std::uint32_t>(hostRate*seconds);Osc o;if(!o.prepare(2*hostRate,{}))return 2;o.reset(0.,frequency);
 std::array<cs80::Decimator,3> filters;for(auto& filter:filters)filter.prepare(float(2*hostRate));
 std::array<std::ofstream,3> streams;const std::array<const char*,3> names{"-saw.wav","-pulse.wav","-sine.wav"};
 auto word=[](std::ofstream& out,std::uint32_t value,int bytes){for(int i=0;i<bytes;++i)out.put(char(value>>(8*i)));};
 for(int b=0;b<3;++b){streams[b].open(std::string(prefix)+names[b],std::ios::binary);if(!streams[b])return 2;auto& out=streams[b];out.write("RIFF",4);word(out,36+4*count,4);out.write("WAVEfmt ",8);word(out,16,4);word(out,3,2);word(out,1,2);word(out,std::uint32_t(hostRate),4);word(out,4*std::uint32_t(hostRate),4);word(out,4,2);word(out,32,2);out.write("data",4);word(out,4*count,4);}
 for(std::uint32_t sample=0;sample<count;++sample){for(int phase=0;phase<2;++phase){auto v=values(o.tick(frequency));for(int b=0;b<3;++b)filters[b].push(float(v[b]/4.0));}for(int b=0;b<3;++b){float x=filters[b].output();streams[b].write(reinterpret_cast<const char*>(&x),sizeof(x));}}
 std::cout<<"Rendered three mono float WAVs: "<<prefix<<"-{saw,pulse,sine}.wav; inferred profile "<<cs80::OscillatorNominalProfile::id<<"; fixed4.0V/fullscale; same physical clock and2xdecimator; coldstartup included\n";return 0;
}

}
int main(int argc,char** argv){if(argc==6&&std::string(argv[1])=="--render")return render(argv[2],std::strtod(argv[3],nullptr),std::strtod(argv[4],nullptr),std::strtod(argv[5],nullptr));setupAllocationAndProfileBounds();timingAndControls();profileAndPins();independentComparator();referenceConvergence();independentNumerics();actualDecimatorPath();stateAndKernel();if(argc==1)benchmark();std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

// Independent matrix verification of the numerical phase correction. The
// analog equations and all tolerances are those already registered for ws1.
#include "core/Ig00156Filter.h"
#include "reference/IG00156PhaseCorrection.h"
#include "reference/IG00156Network.h"
#include "Ws1Tolerances.h"
#include <complex>
#include <cstdio>
#include <iostream>
using namespace cs80;
namespace ref=cs80::reference::ig00156;
namespace {
constexpr double pi=3.14159265358979323846;
int failures=0,checks=0;
void check(bool ok,const char* name) {++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<'\n';}}
struct Truth : ref::PhaseCorrectedLinearStage {
 static ref::LinearSystem<4> system(const Ig00156Network& n,double fc,double q) {
  const ref::StageNetwork net{n.sourceOhms,n.groundOhms,n.inputFarads,n.feedbackOhms,n.feedbackFarads,n.integratorFarads,n.integratorFarads,n.feedforwardOhms,n.feedforwardOhms};
  const double gq=ref::qSiemensForLowFrequencyQ(net,q);
  const double w=ig00156IntegratorRate(n,fc,gq);
  const double gm=std::max(0.0,w*n.integratorFarads-1/n.feedforwardOhms);
  return ref::stageSystem(net,{gm,gm,gq,0},1/ref::passbandGain(net));
 }
 Truth(const Ig00156Network& n,double fc,double q,double fs) : PhaseCorrectedLinearStage(system(n,fc,q),fs) {}
};
void equivalence(const ws1::Tolerances& tol){
 double worst=0;const double ff=1/(2*pi*4.7e6*1.5e-9);
 for(double fs:{88200.,96000.,192000.,384000.})for(double fc:{60.,250.,1000.,4000.,12000.})for(double q:{.5,1.,2.,5.,9.})for(int kind=0;kind<3;++kind){
  const double hc=kind==2?std::max(60.,fc/4):fc;
  Ig00156Stage hp(kIg00156HighPassNetwork),lp(kIg00156LowPassNetwork);
  hp.set(float(hc-ff),float(q),float(fs));lp.set(float(fc-ff),float(q),float(fs));
  Truth ht(kIg00156HighPassNetwork,hc,q,fs),lt(kIg00156LowPassNetwork,fc,q,fs);
  double peak=0,error=0;
  for(int i=0;i<3000;++i){
   const double t=i/fs;
   const float u=float((i<40?1e-4:0)+1e-4*std::sin(2*pi*.73*fc*t)+6e-5*std::sin(2*pi*2.9*fc*t+.4));
   float y=u;double truth=u;
   if(kind!=1){y=hp.process(y).hp;truth=ht.step(truth,true);}
   if(kind!=0){y=lp.process(y).lp;truth=lt.step(truth,false);}
   error=std::max(error,std::abs(y-truth));peak=std::max(peak,std::abs(truth));
  }
  worst=std::max(worst,error/peak);
  if(error/peak>tol["equivalence_max_rel"])std::printf("equivalence fail fs%g fc%g q%g kind%d %.9g\n",fs,fc,q,kind,error/peak);
 }
 std::printf("phase-correction matrix equivalence: %.9g (limit %.9g)\n",worst,tol["equivalence_max_rel"]);
 check(worst<=tol["equivalence_max_rel"],"reduced nonlinear stage matches independent full-matrix small-signal step");
}
void smallSignal(const ws1::Tolerances& tol){
 for(double fs:{88200.,96000.,192000.,384000.}){
  double worstCore=0,worstBand=0;
  for(double fc:{60.,250.,1000.,4000.,12000.})for(double q:{.5,1.,2.,5.,9.})for(int kind=0;kind<3;++kind){
   Truth hp(kIg00156HighPassNetwork,kind==2?std::max(60.,fc/4):fc,q,fs),lp(kIg00156LowPassNetwork,fc,q,fs);
   auto response=[&](double f,bool discrete){
    std::complex<double> v=1;
    if(kind!=1)v*=discrete?hp.discrete(f,true):hp.continuous(f,true);
    if(kind!=0)v*=discrete?lp.discrete(f,false):lp.continuous(f,false);
    return v;
   };
   double peak=0;for(double f=5;f<std::min(24000.,.45*fs);f*=1.01)peak=std::max(peak,std::abs(response(f,false)));
   double core=0,band=0;
   for(double f=20;f<=std::min(20000.,fs/5);f*=1.02){
    const double e=std::abs(response(f,true)-response(f,false))/peak;
    if(f>=.5*fc&&f<=2*fc&&f<=fs/8)core=std::max(core,e);
    band=std::max(band,e);
   }
   check(core<=tol["small_signal_core_rel"],"whole-grid core-band limit");
   check(band<=tol[fs>=192000?"small_signal_band_hi_rate_rel":"small_signal_band_rel"],"whole-grid full-band limit");
   worstCore=std::max(worstCore,core);worstBand=std::max(worstBand,band);
  }
  std::printf("phase-corrected %g Hz: core %.9g band %.9g\n",fs,worstCore,worstBand);
 }
}
void direct(const ws1::Tolerances& tol){
 double worst=0;const double ff=1/(2*pi*4.7e6*1.5e-9);
 for(double fs:{88200.,96000.,192000.,384000.})for(double fc:{4000.,12000.})for(double q:{.5,5.})for(bool isHp:{false,true})for(double ratio:{.5,1.,2.}){
  const double f=fc*ratio;if(f>std::min(20000.,fs/5))continue;
  const auto net=isHp?kIg00156HighPassNetwork:kIg00156LowPassNetwork;
  Truth truth(net,fc,q,fs);Ig00156Stage rt(net);rt.set(float(fc-ff),float(q),float(fs));
  const int warm=int(fs*.2),count=int(std::round(.1*f)/f*fs);
  std::complex<double> acc=0;
  for(int i=0;i<warm+count;++i){
   const double ph=2*pi*f*i/fs;const auto out=rt.process(float(1e-4*std::sin(ph)));
   if(i>=warm)acc+=double(isHp?out.hp:out.lp)*std::complex<double>(std::sin(ph),std::cos(ph));
  }
  const auto measured=acc*(2.0/count/1e-4);
  double peak=0;for(double g=5;g<std::min(24000.,.45*fs);g*=1.01)peak=std::max(peak,std::abs(truth.continuous(g,isHp)));
  const double e=std::abs(measured-truth.continuous(f,isHp))/peak;
  const double lim=(ratio>=.5&&ratio<=2&&f<=fs/8)?tol["small_signal_core_rel"]:tol[fs>=192000?"small_signal_band_hi_rate_rel":"small_signal_band_rel"];
  check(e<=lim,"direct realtime lock-in within unchanged continuous-truth limit");worst=std::max(worst,e);
 }
 std::printf("phase-corrected direct realtime: worst normalized error %.9g\n",worst);
}
}
int main(int argc,char** argv){try{
 const ws1::Tolerances tol(argc>1?argv[1]:"tests/parity/tolerances-v2/ws1-voice.json","ig00156_nodal");
 equivalence(tol);smallSignal(tol);direct(tol);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;}

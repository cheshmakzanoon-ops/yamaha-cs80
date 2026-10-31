#pragma once
// Independent full-matrix discrete oracle for the existing continuous nodal
// system. Fourth-order linear phase correction (Pade [2/2]) with a causal
// cubic input-derivative difference. No production reduced-solver code is used.
// See docs/IG00156_PHASE_CORRECTION.md; this validates numerics, not hardware.
#include "IG00156Network.h"
#include <algorithm>
#include <complex>

namespace cs80::reference::ig00156 {
class PhaseCorrectedLinearStage {
public:
 LinearSystem<4> analog;
 Matrix<4> q{},p{};
 Vector<4> ab{},x{},history{};
 double h=0,beta=0;
 explicit PhaseCorrectedLinearStage(const LinearSystem<4>& system = {}, double fs = 48000.0) {
  configure(system, fs);
 }
 void configure(const LinearSystem<4>& system, double fs) {
  analog=system; ab={};
  h=1/fs;beta=h*h/12;
  for(int i=0;i<4;++i)for(int j=0;j<4;++j){
   double square=0;for(int k=0;k<4;++k)square+=analog.a[i][k]*analog.a[k][j];
   p[i][j]=(i==j?1.:0.)+h*.5*analog.a[i][j]+beta*square;
   q[i][j]=(i==j?1.:0.)-h*.5*analog.a[i][j]+beta*square;
  }
  for(int i=0;i<4;++i)for(int j=0;j<4;++j)ab[i]+=analog.a[i][j]*analog.b[j];
 }
 double step(double u,bool highpass) {
  const double deriv=(11*u-29*history[0]+27*history[1]-11*history[2]+2*history[3])/(6*h);
  Vector<4> rhs{};
  for(int i=0;i<4;++i){
   rhs[i]=analog.b[i]*(h*.5*(u+history[0])-beta*deriv)-beta*ab[i]*(u-history[0]);
   for(int j=0;j<4;++j)rhs[i]+=p[i][j]*x[j];
  }
  x=solve(q,rhs).value;
  for(int i=3;i>0;--i)history[i]=history[i-1];history[0]=u;
  return highpass?2*(x[1]+x[2])-x[3]:x[3];
 }
 // Full four-state Newton reference, independent of the production scalar
 // elimination and its cached slopes. Linear A already contains both gm
 // currents; N is the difference between linear and saturated OTA currents.
 double nonlinearStep(double u, double gmRate, double level, bool highpass) {
  const double deriv=(11*u-29*history[0]+27*history[1]-11*history[2]+2*history[3])/(6*h);
  Vector<4> rhs{};
  for(int i=0;i<4;++i){
   rhs[i]=analog.b[i]*(h*.5*(u+history[0])-beta*deriv)-beta*ab[i]*(u-history[0]);
   for(int j=0;j<4;++j)rhs[i]+=p[i][j]*x[j];
  }
  const auto correction=[&](const Vector<4>& at) {
   const double hp=2*(at[1]+at[2])-at[3];
   return Vector<4>{0,0,gmRate*(hp-level*std::tanh(hp/level)),
                    gmRate*(at[2]-level*std::tanh(at[2]/level))};
  };
  const auto previous=correction(x);
  for(int i=0;i<4;++i)rhs[i]+=.5*h*previous[i];
  Vector<4> trial=x;
  for(int iteration=0;iteration<64;++iteration){
   const auto n=correction(trial);
   Vector<4> residual{};
   double maximum=0;
   for(int i=0;i<4;++i){
    residual[i]=-rhs[i]-.5*h*n[i];
    for(int j=0;j<4;++j)residual[i]+=q[i][j]*trial[j];
    maximum=std::max(maximum,std::abs(residual[i]));
   }
   if(maximum<1e-13)break;
   auto jacobian=q;
   const double hp=2*(trial[1]+trial[2])-trial[3];
   const double th=std::tanh(hp/level),tb=std::tanh(trial[2]/level);
   const double dh=.5*h*gmRate*th*th,db=.5*h*gmRate*tb*tb;
   jacobian[2][1]-=2*dh;jacobian[2][2]-=2*dh;jacobian[2][3]+=dh;
   jacobian[3][2]-=db;
   const auto delta=solve(jacobian,residual).value;
   for(int i=0;i<4;++i)trial[i]-=delta[i];
  }
  x=trial;
  for(int i=3;i>0;--i)history[i]=history[i-1];history[0]=u;
  return highpass?2*(x[1]+x[2])-x[3]:x[3];
 }
 std::complex<double> continuous(double f,bool hp) const {
  return response(analog,hp?hpRow<4>(0):lpRow<4>(0),{0,2*3.14159265358979323846*f});
 }
 std::complex<double> discrete(double f,bool hp) const {
  using C=std::complex<double>;
  const C z=std::exp(C(0,-2*3.14159265358979323846*f*h));
  const C deriv=(11.-29.*z+27.*z*z-11.*z*z*z+2.*z*z*z*z)/(6*h);
  std::array<std::array<C,5>,4> m{};
  for(int i=0;i<4;++i){
   for(int j=0;j<4;++j)m[i][j]=q[i][j]-z*p[i][j];
   m[i][4]=analog.b[i]*(h*.5*(1.+z)-beta*deriv)-beta*ab[i]*(1.-z);
  }
  for(int k=0;k<4;++k){
   int pivot=k;for(int i=k+1;i<4;++i)if(abs(m[i][k])>abs(m[pivot][k]))pivot=i;
   std::swap(m[pivot],m[k]);
   for(int i=k+1;i<4;++i){const C ratio=m[i][k]/m[k][k];for(int j=k;j<5;++j)m[i][j]-=ratio*m[k][j];}
  }
  std::array<C,4> sol{};
  for(int i=3;i>=0;--i){C rhs=m[i][4];for(int j=i+1;j<4;++j)rhs-=m[i][j]*sol[j];sol[i]=rhs/m[i][i];}
  return hp?2.*(sol[1]+sol[2])-sol[3]:sol[3];
 }
};
}

// Offline graph-consistency tests. Fixtures are NOT measured CS-80 values.
#include "reference/PraWah.h"
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace cs80::reference::pra_wah;
namespace {
int checks=0, failures=0;
void check(bool yes, const char* what) { ++checks; if (!yes) { if(failures<20) std::cerr<<what<<'\n'; ++failures; } }
void near(Complex a, Complex b, const char* what, double tol=2e-8) {
    check(std::abs(a-b)<=tol*(1.0+std::abs(a)+std::abs(b)),what);
}
// Independent modified nodal analysis: stamps physical branches and ideal
// amplifier constraints. No all-pass factors or closed-form feedback denominator.
enum Node { E,X,N,U,A,T,P,V,B,Q,W,C,D,Z,Y,Count };
struct Circuit {
    std::vector<std::vector<Complex>> m;
    std::vector<Complex> rhs;
    Circuit():m(Count,std::vector<Complex>(Count)),rhs(Count){}
    int unknown() { const int i=static_cast<int>(m.size()); for(auto& row:m) row.push_back({}); m.emplace_back(i+1); rhs.push_back({}); return i; }
    void branch(int a,int b,Complex admittance) {
        if(a>=0) m[a][a]+=admittance;
        if(b>=0) m[b][b]+=admittance;
        if(a>=0&&b>=0) {m[a][b]-=admittance;m[b][a]-=admittance;}
    }
    void resistor(int a,int b,double ohms) {
        if(ohms!=0) { branch(a,b,1.0/ohms); return; }
        int i=unknown(); if(a>=0){m[a][i]+=1.;m[i][a]+=1.;} if(b>=0){m[b][i]-=1.;m[i][b]-=1.;}
    }
    void amplifier(int out,int minus,int plus) {
        const int i=unknown();m[out][i]=1.;m[i][minus]=1.;if(plus>=0)m[i][plus]=-1.;
    }
    std::vector<Complex> solve() {
        const auto size=m.size();
        // Row scaling makes the heterogeneous KCL/voltage constraints comparable.
        for(size_t r=0;r<size;++r) {double scale=0;for(auto v:m[r])scale=std::max(scale,std::abs(v));if(scale==0)throw std::runtime_error("empty MNA row");for(auto& v:m[r])v/=scale;rhs[r]/=scale;}
        for(size_t k=0;k<size;++k) {
            size_t pivot=k;for(size_t r=k+1;r<size;++r)if(std::abs(m[r][k])>std::abs(m[pivot][k]))pivot=r;
            if(std::abs(m[pivot][k])<1e-18)throw std::runtime_error("singular test MNA");
            std::swap(m[k],m[pivot]);std::swap(rhs[k],rhs[pivot]);
            for(size_t r=k+1;r<size;++r){Complex f=m[r][k]/m[k][k];for(size_t c=k;c<size;++c)m[r][c]-=f*m[k][c];rhs[r]-=f*rhs[k];}
        }
        std::vector<Complex> result(size);
        for(size_t r=size;r-->0;){Complex v=rhs[r];for(size_t c=r+1;c<size;++c)v-=m[r][c]*result[c];result[r]=v/m[r][r];}
        return result;
    }
};
std::vector<Complex> nodal(const Boundary& b,double hz,Complex vs) {
    Circuit g; const Complex s(0,2*std::numbers::pi*hz);
    if(b.sourceOhms==0){int i=g.unknown();g.m[E][i]=1.;g.m[i][E]=1.;g.rhs[i]=vs;}
    else {g.branch(E,-1,1./b.sourceOhms);g.rhs[E]+=vs/b.sourceOhms;}
    g.branch(E,X,s*10e-6);g.resistor(X,-1,10000);g.resistor(X,N,10000);
    g.resistor(N,U,10000);g.resistor(A,U,10000);g.resistor(C,N,470);
    g.amplifier(A,U,-1);g.resistor(A,V,10000);g.resistor(B,V,10000);
    g.resistor(A,T,b.opticalOhms);g.resistor(A,T,b.unresolvedParallelOhms);
    g.resistor(T,P,b.vr1Ohms);g.branch(P,-1,s*2.2e-9);g.amplifier(B,V,P);
    g.resistor(B,W,10000);g.resistor(C,W,10000);g.resistor(B,Q,10000);
    g.branch(Q,-1,s*33e-9);g.amplifier(C,W,Q);g.branch(C,D,s*1e-6);
    g.resistor(D,-1,10000);g.resistor(D,Z,10000);g.resistor(Y,Z,b.vr2Ohms);
    g.amplifier(Y,Z,-1);g.branch(X,-1,b.dryLoadSiemens);g.branch(Y,-1,b.outputLoadSiemens);
    return g.solve();
}
Boundary fixture(){return {38000,220000,12500,47000,1200,{2e-5,1e-5},{1e-4,-3e-6}};}
std::array<Complex,13> values(const Response& r){return {r.x,r.n,r.a,r.p,r.b,r.q,r.c,r.d,r.y,r.sourceCurrent,r.dryLoadCurrent,r.outputLoadCurrent,r.outputCurrent};}
void graphCase(const Boundary& b,double hz,Complex vs) {
    auto r=solve(b,hz,vs);auto v=nodal(b,hz,vs);
    const std::array<int,9> ids{X,N,A,P,B,Q,C,D,Y};auto rv=values(r);
    for(size_t i=0;i<ids.size();++i)near(rv[i],v[ids[i]],"independent MNA node");
    const Complex s(0,2*std::numbers::pi*hz);
    near(r.sourceCurrent,s*10e-6*(v[E]-v[X]),"source current");
    near(r.dryLoadCurrent,b.dryLoadSiemens*v[X],"dry current");
    near(r.outputLoadCurrent,b.outputLoadSiemens*v[Y],"output load current");
    near(r.outputCurrent,b.outputLoadSiemens*v[Y]-v[D]/10000.,"output total current");
    // The final MNA unknown is IC2B's output-to-ground current. Its opposite
    // is current supplied by that amplifier, independently including the
    // feedback short-current unknown when VR2 is exactly zero.
    near(r.outputCurrent,-v.back(),"independent MNA amplifier output current");
    near((r.n-r.x)/10000.+r.n/10000.+(r.n-r.c)/470.,{},"N KCL",1e-11);
    near(r.sourceCurrent-r.x/10000.-(r.x-r.n)/10000.-r.dryLoadCurrent,{},"X KCL",1e-11);
    near(s*1e-6*(r.d-r.c)+2.*r.d/10000.,{},"D KCL",1e-11);
    near((r.b-r.q)/10000.-s*33e-9*r.q,{},"Q KCL",1e-11);
}
template<class F>void rejects(F f,const char* what){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}catch(const std::overflow_error&){caught=true;}check(caught,what);}
}
int main(){
    for(double hz:{0.,.01,1.,10.,100.,300.,1000.,3000.,10000.,100000.,1e7})
    for(double opt:{100.,10000.,100000.,1e7})
    for(double vr1:{0.,12500.,25000.})
    for(double vr2:{0.,1.,50000.,100000.})
    for(double rs:{0.,1000.,100000.}) {
        auto b=fixture();b.opticalOhms=opt;b.vr1Ohms=vr1;b.vr2Ohms=vr2;b.sourceOhms=rs;
        graphCase(b,hz,{.7,-.3});
    }
    auto b=fixture();
    for(auto dry:{Complex{},Complex{1e-3,0},Complex{0,1e-3},Complex{0,-1e-3}}){b.dryLoadSiemens=dry;graphCase(b,1700,{1,0});}
    b=fixture();auto baseline=solve(b,1000,{1,0});
    b.outputLoadSiemens={.02,.03};auto loaded=solve(b,1000,{1,0});near(baseline.y,loaded.y,"ideal output voltage load independence",1e-14);
    check(std::abs(loaded.outputCurrent-baseline.outputCurrent)>1e-5,"output load affects current");
    b=fixture();b.dryLoadSiemens={.02,0};check(std::abs(solve(b,1000,{1,0}).y-baseline.y)>1e-4,"dry load affects response");
    b=fixture();b.sourceOhms=1e5;check(std::abs(solve(b,1000,{1,0}).y-baseline.y)>1e-4,"source impedance affects response");
    b=fixture();b.opticalOhms=100;check(std::abs(solve(b,1000,{1,0}).y-baseline.y)>1e-4,"optical resistance affects response");
    b=fixture();b.unresolvedParallelOhms=100;check(std::abs(solve(b,1000,{1,0}).y-baseline.y)>1e-4,"unresolved parallel resistance affects response");
    b=fixture();auto unity=values(solve(b,1234,{1,0}));const Complex scale(-.2,.8);auto scaled=values(solve(b,1234,scale));
    for(size_t i=0;i<unity.size();++i)near(scaled[i],scale*unity[i],"complex linearity",1e-12);
    for(auto v:values(solve(b,0,{1,2})))check(v==Complex{},"DC exactly zero");
    for(auto v:values(solve(b,1234,{})))check(v==Complex{},"zero excitation exactly zero");
    check(std::abs(solve(b,1e-7,{1,0}).y)<1e-12,"low-frequency two coupling caps");
    // Independent high-frequency circuit limit: both shunt capacitors short,
    // both coupling capacitors short, A=-N, B=-A, C=-B=-N.
    const double nxLimit=1.0/(2.0+2.0*10000.0/470.0);
    const Complex xLimit=(1.0/b.sourceOhms)/(1.0/b.sourceOhms+1e-4+(1.0-nxLimit)*1e-4+b.dryLoadSiemens);
    near(solve(b,1e12,{1,0}).y,(b.vr2Ohms/10000.0)*nxLimit*xLimit,"high-frequency circuit limit",1e-8);
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    for(double bad:{-1.,nan,inf}) {
        for(int field=0;field<5;++field){auto q=fixture();switch(field){case 0:q.opticalOhms=bad;break;case 1:q.unresolvedParallelOhms=bad;break;case 2:q.vr1Ohms=bad;break;case 3:q.vr2Ohms=bad;break;default:q.sourceOhms=bad;}rejects([&]{solve(q,1000,{1,0});},"invalid scalar boundary");}
        rejects([&]{solve(fixture(),bad,{1,0});},"invalid frequency");
    }
    Boundary unspecified;
    rejects([&]{solve(unspecified,1000,{1,0});},"default initialization fails closed");
    rejects([&]{solve(Boundary{38000},1000,{1,0});},"partial aggregate fails closed");
    for(int field=0;field<7;++field){auto z=fixture();switch(field){case 0:z.opticalOhms=nan;break;case 1:z.unresolvedParallelOhms=nan;break;case 2:z.vr1Ohms=nan;break;case 3:z.vr2Ohms=nan;break;case 4:z.sourceOhms=nan;break;case 5:z.dryLoadSiemens={nan,nan};break;default:z.outputLoadSiemens={nan,nan};}rejects([&]{solve(z,1000,{1,0});},"individually unspecified boundary fails closed");}
    for(int field=0;field<2;++field){auto q=fixture();if(field==0)q.opticalOhms=0;else q.unresolvedParallelOhms=0;rejects([&]{solve(q,1000,{1,0});},"zero mandatory resistance");}
    auto q=fixture();q.vr1Ohms=25000.1;rejects([&]{solve(q,1000,{1,0});},"VR1 range");q=fixture();q.vr2Ohms=100000.1;rejects([&]{solve(q,1000,{1,0});},"VR2 range");
    rejects([&]{solve(Boundary{},1000,{1,0});},"unspecified boundary fails closed");
    for(Complex bad:{Complex{nan,0},Complex{0,inf}})rejects([&]{solve(fixture(),1000,bad);},"invalid source phasor");
    for(Complex bad:{Complex{-1,0},Complex{nan,0},Complex{0,inf}})for(bool output:{false,true}){auto z=fixture();if(output)z.outputLoadSiemens=bad;else z.dryLoadSiemens=bad;rejects([&]{solve(z,1000,{1,0});},"invalid load");}
    rejects([&]{solve(fixture(),std::numeric_limits<double>::max(),{1,0});},"overflow frequency fails closed");
    q=fixture();q.opticalOhms=std::numeric_limits<double>::denorm_min();q.unresolvedParallelOhms=q.opticalOhms;q.vr1Ohms=0;
    rejects([&]{solve(q,1000,{1,0});},"underflow RC fails closed");
    q=fixture();q.opticalOhms=1e300;q.unresolvedParallelOhms=1e300;
    for(auto v:values(solve(q,1,{1,0})))check(finite(v),"stable parallel resistance avoids product overflow");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

// Offline graph-consistency fixtures, not measured CS-80 response.
#include "reference/Oe2Filters.h"
#include <array>
#include <iostream>
#include <vector>
using namespace cs80::reference::oe2_filters;
namespace {
int checks=0, failures=0;
void check(bool yes,const char* what){++checks;if(!yes){if(failures<20)std::cerr<<what<<'\n';++failures;}}
void near(Complex a,Complex b,const char* what,double tolerance=2e-8){check(std::abs(a-b)<=tolerance*(1.+std::abs(a)+std::abs(b)),what);}
template<class F> void rejects(F f,const char* what){bool caught=false;try{f();}catch(const std::invalid_argument&){caught=true;}catch(const std::overflow_error&){caught=true;}check(caught,what);}
struct Circuit {
    std::vector<std::vector<Complex>> m;
    std::vector<Complex> rhs;
    explicit Circuit(int nodes):m(nodes,std::vector<Complex>(nodes)),rhs(nodes){}
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
    int amplifier(int out,int minus,int plus) {
        const int i=unknown();m[out][i]=1.;m[i][minus]=1.;if(plus>=0)m[i][plus]=-1.;return i;
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
// Components are stamped individually. Cf terminates at the ideal output,
// while Cg terminates at its high-impedance input: equal voltages, distinct KCL.
enum Node {S,E,U,A,B,Y,Z,M,V,Count};
struct GraphResult {std::vector<Complex> nodes;Complex source,follower,amplifier;};
GraphResult nodal(double resistance,double hz,Complex vs,Complex followerLoad,
                  Complex finalLoad,bool reconstruction,Branch branch){
    Circuit g(Count);const Complex s(0,2*std::numbers::pi*hz);
    int sourceI=g.unknown();g.m[S][sourceI]=1.;g.m[sourceI][S]=1.;g.rhs[sourceI]=vs;
    if(reconstruction){g.resistor(S,E,resistance);g.branch(E,U,s*1e-6);g.resistor(U,-1,100000);}
    else {g.resistor(S,U,resistance);for(int node:{E,Z,M,V})g.resistor(node,-1,1);}
    g.resistor(U,A,10000);g.resistor(A,B,10000);g.branch(A,Y,s*1.8e-9);g.branch(B,-1,s*680e-12);
    int followerI=g.amplifier(Y,Y,B);g.branch(Y,-1,followerLoad);
    int ampI=-1;
    if(reconstruction){g.branch(Y,Z,s*0.1e-6);g.resistor(Z,M,10000);g.resistor(V,M,branch==Branch::upper?39000:100000);ampI=g.amplifier(V,M,-1);g.branch(V,-1,finalLoad);}
    auto v=g.solve();return {v,-v[sourceI],-v[followerI],ampI<0?Complex{}:-v[ampI]};
}
InputBoundary input(){return {1200,{1e-4,-2e-5}};}
ReconstructionBoundary reconstruction(){return {2200,{2e-5,1e-5},{1e-4,-3e-5},Branch::upper};}
std::array<Complex,7> values(const InputResponse& r){return {r.u,r.a,r.b,r.y,r.sourceCurrent,r.outputLoadCurrent,r.followerCurrent};}
std::array<Complex,12> values(const ReconstructionResponse& r){return {r.u,r.a,r.b,r.y,r.z,r.v,r.sourceCurrent,r.shuntCurrent,r.followerLoadCurrent,r.outputLoadCurrent,r.followerCurrent,r.amplifierCurrent};}
void inputCase(const InputBoundary& b,double hz,Complex vs){
    auto r=solveInput(b,hz,vs);auto g=nodal(b.sourceOhms,hz,vs,b.outputLoadSiemens,{},false,Branch::unknown);auto rv=values(r);
    const std::array<int,4> ids{U,A,B,Y};for(size_t i=0;i<ids.size();++i)near(rv[i],g.nodes[ids[i]],"input MNA nodes");
    near(r.sourceCurrent,g.source,"input MNA source current");near(r.followerCurrent,g.follower,"input MNA output current");
    near(r.outputLoadCurrent,b.outputLoadSiemens*g.nodes[Y],"input output load current");
    const Complex s(0,2*std::numbers::pi*hz);
    near((r.a-r.u)/10000.+(r.a-r.b)/10000.+s*1.8e-9*(r.a-r.y),{},"input A KCL",1e-11);
    near((r.b-r.a)/10000.+s*680e-12*r.b,{},"input B KCL",1e-11);
    near(r.sourceCurrent-(r.u-r.a)/10000.,{},"input U KCL",1e-11);
}
void reconstructionCase(const ReconstructionBoundary& b,double hz,Complex vs){
    auto r=solveReconstruction(b,hz,vs);auto g=nodal(b.sourceOhms,hz,vs,b.followerLoadSiemens,b.outputLoadSiemens,true,b.branch);auto rv=values(r);
    const std::array<int,6> ids{U,A,B,Y,Z,V};for(size_t i=0;i<ids.size();++i)near(rv[i],g.nodes[ids[i]],"reconstruction MNA nodes");
    near(r.sourceCurrent,g.source,"reconstruction MNA source current");near(r.followerCurrent,g.follower,"reconstruction MNA follower current");near(r.amplifierCurrent,g.amplifier,"reconstruction MNA amplifier current");
    near(r.shuntCurrent,g.nodes[U]/100000.,"reconstruction 100k current");
    near(r.followerLoadCurrent,b.followerLoadSiemens*g.nodes[Y],"reconstruction follower load current");near(r.outputLoadCurrent,b.outputLoadSiemens*g.nodes[V],"reconstruction final load current");
    const Complex s(0,2*std::numbers::pi*hz);
    near((r.a-r.u)/10000.+(r.a-r.b)/10000.+s*1.8e-9*(r.a-r.y),{},"reconstruction A KCL",1e-11);
    near((r.b-r.a)/10000.+s*680e-12*r.b,{},"reconstruction B KCL",1e-11);
    near(r.sourceCurrent-r.shuntCurrent-(r.u-r.a)/10000.,{},"reconstruction U KCL",1e-11);
    near(s*0.1e-6*(r.z-r.y)+r.z/10000.,{},"interstage Z KCL",1e-11);
}
}
int main(){
    const std::array<Complex,4> loads{Complex{},Complex{1e-3,0},Complex{0,1e-3},Complex{0,-1e-3}};
    for(double hz:{0.,.001,.1,1.,10.,100.,159.15494309189535,1000.,3000.,10000.,100000.,1e7})
    for(double rs:{0.,1.,1000.,10000.,1e6})
    for(Complex load:loads){auto b=input();b.sourceOhms=rs;b.outputLoadSiemens=load;inputCase(b,hz,{.7,-.3});
        for(Branch branch:{Branch::upper,Branch::lower}){auto r=reconstruction();r.sourceOhms=rs;r.followerLoadSiemens=load;r.outputLoadSiemens=load*.7;r.branch=branch;reconstructionCase(r,hz,{.7,-.3});}}
    auto i=input();auto r=reconstruction();const Complex scale(-.2,.8);
    auto iv=values(solveInput(i,1234,{1,0})),is=values(solveInput(i,1234,scale));for(size_t n=0;n<iv.size();++n)near(is[n],scale*iv[n],"input linearity",1e-12);
    auto rv=values(solveReconstruction(r,1234,{1,0})),rs=values(solveReconstruction(r,1234,scale));for(size_t n=0;n<rv.size();++n)near(rs[n],scale*rv[n],"reconstruction linearity",1e-12);
    for(auto v:values(solveInput(i,1000,{})))check(v==Complex{},"input zero excitation");
    for(auto v:values(solveReconstruction(r,1000,{})))check(v==Complex{},"reconstruction zero excitation");
    auto dc=solveInput(i,0,{1,2});check(dc.y==Complex(1,2)&&dc.sourceCurrent==Complex{},"input DC unity and zero source current");
    for(auto v:values(solveReconstruction(r,0,{1,2})))check(v==Complex{},"reconstruction DC zero");
    auto ib=solveInput(i,1000,{1,0});i.outputLoadSiemens={.1,.2};auto il=solveInput(i,1000,{1,0});near(ib.y,il.y,"ideal input voltage load invariance",1e-14);check(std::abs(ib.followerCurrent-il.followerCurrent)>1e-4,"input output current changes with load");
    auto rb=solveReconstruction(r,1000,{1,0});r.followerLoadSiemens={.1,.2};r.outputLoadSiemens={.2,.1};auto rl=solveReconstruction(r,1000,{1,0});near(rb.y,rl.y,"ideal reconstruction follower load invariance",1e-14);near(rb.v,rl.v,"ideal reconstruction amplifier load invariance",1e-14);check(std::abs(rb.followerCurrent-rl.followerCurrent)>1e-4,"follower current changes with load");check(std::abs(rb.amplifierCurrent-rl.amplifierCurrent)>1e-4,"final current changes with load");
    i=input();i.sourceOhms=1e5;check(std::abs(ib.y-solveInput(i,1000,{1,0}).y)>1e-4,"input source resistance sensitivity");
    r=reconstruction();r.sourceOhms=1e5;check(std::abs(rb.y-solveReconstruction(r,1000,{1,0}).y)>1e-4,"reconstruction source resistance sensitivity");
    r=reconstruction();auto upper=solveReconstruction(r,1000,{1,0});r.branch=Branch::lower;auto lower=solveReconstruction(r,1000,{1,0});near(lower.v,upper.v*(100./39.),"unequal feedback gains",1e-12);near(lower.y,upper.y,"branches share LP voltage",1e-14);
    for(Branch branch:{Branch::upper,Branch::lower}){r.branch=branch;auto high=solveReconstruction(r,1e8,{1,0});near(high.v/high.y,branch==Branch::upper?-3.9:-10.,"interstage high-frequency gain",2e-5);auto low=solveReconstruction(r,1e-7,{1,0});check(std::abs(low.v)<1e-12,"two reconstruction coupling caps block low frequency");}
    i=input();auto high1=solveInput(i,1e8,{1,0}),high2=solveInput(i,2e8,{1,0});near(high2.y/high1.y,.25,"input second-order rolloff",2e-5);
    // Corner of the interstage alone, not a claim about the combined filter.
    r=reconstruction();auto corner=solveReconstruction(r,1./(2*std::numbers::pi*10000*.1e-6),{1,0});near(corner.v/corner.y,Complex(-1.95,-1.95),"interstage RC corner",1e-12);
    // Cancellation-sensitive regressions use scaled components, not a global
    // absolute tolerance that could hide tiny but meaningful phasors.
    i=input();i.sourceOhms=1e30;
    const auto tinyPort=solveInput(i,1000,{1,0});
    const Complex sk(0,2*std::numbers::pi*1000);
    const Complex scaledPort=10000.+(1.+sk*10000.*680e-12)/(sk*680e-12*(1.+sk*10000.*1.8e-9));
    near(tinyPort.u*1e30,scaledPort,"huge source resistance preserves tiny port voltage",1e-11);
    r=reconstruction();r.sourceOhms=0;
    const auto fast=solveReconstruction(r,1e15,{1,0});
    const double omega=2*std::numbers::pi*1e15;
    const double conductance=1./100000.+1./10000.;
    const double expectedImaginary=(1./(10000.*10000.*1.8e-9)+conductance*conductance/1e-6)/omega;
    check(std::abs(fast.sourceCurrent.imag()/expectedImaginary-1.)<1e-10,"high-frequency tiny source-current quadrature preserved");
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    InputBoundary missingInput;ReconstructionBoundary missingReconstruction;
    rejects([&]{solveInput(missingInput,1000,{1,0});},"default input missing");rejects([&]{solveReconstruction(missingReconstruction,1000,{1,0});},"default reconstruction missing");
    rejects([&]{solveInput(InputBoundary{0},1000,{1,0});},"partial input missing");rejects([&]{solveReconstruction(ReconstructionBoundary{0},1000,{1,0});},"partial reconstruction missing");
    for(double bad:{-1.,nan,inf}){i=input();i.sourceOhms=bad;rejects([&]{solveInput(i,1000,{1,0});},"invalid input resistance");r=reconstruction();r.sourceOhms=bad;rejects([&]{solveReconstruction(r,1000,{1,0});},"invalid reconstruction resistance");rejects([&]{solveInput(input(),bad,{1,0});},"invalid input frequency");rejects([&]{solveReconstruction(reconstruction(),bad,{1,0});},"invalid reconstruction frequency");}
    for(Complex bad:{Complex{-1,0},Complex{nan,0},Complex{0,nan},Complex{inf,0},Complex{0,inf}}){i=input();i.outputLoadSiemens=bad;rejects([&]{solveInput(i,1000,{1,0});},"invalid input load");for(bool follower:{false,true}){r=reconstruction();if(follower)r.followerLoadSiemens=bad;else r.outputLoadSiemens=bad;rejects([&]{solveReconstruction(r,1000,{1,0});},"invalid reconstruction load");}}
    for(Complex bad:{Complex{nan,0},Complex{0,inf}}){rejects([&]{solveInput(input(),1000,bad);},"invalid input volts");rejects([&]{solveReconstruction(reconstruction(),1000,bad);},"invalid reconstruction volts");}
    for(Branch bad:{Branch::unknown,static_cast<Branch>(99)}){r=reconstruction();r.branch=bad;rejects([&]{solveReconstruction(r,1000,{1,0});},"invalid branch");}
    const double huge=std::numeric_limits<double>::max();rejects([&]{solveInput(input(),huge,{1,0});},"input frequency overflow");rejects([&]{solveReconstruction(reconstruction(),huge,{1,0});},"reconstruction frequency overflow");
    i=input();i.sourceOhms=huge;rejects([&]{solveInput(i,1e100,{1,0});},"input derived overflow");r=reconstruction();r.sourceOhms=huge;rejects([&]{solveReconstruction(r,1e100,{1,0});},"reconstruction derived overflow");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

#include "reference/LoadedControl.h"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace cs80::reference::loaded_control;
namespace {
int checks=0;
void check(bool ok, const char* label) { ++checks; if(!ok) throw std::runtime_error(label); }
void near(double a,double b,double tol,const char* label) { check(std::abs(a-b)<=tol,label); }
Inputs fixture() {
    Inputs p;
    for(auto& s:p.shared) s={0,0};
    p.fh=p.fl=p.hpTrim=p.lpTrim={0,0};
    p.hpReceiver=p.lpReceiver={0,0};
    p.hpGrade=p.lpGrade=Grade::A;
    return p;
}
void kcl(const Inputs& p,const Result& v) {
    double hp=v.hpVolts/shuntOhms(p.hpGrade,true)+p.hpReceiver.siemens*v.hpVolts+p.hpReceiver.amps;
    double lp=v.lpVolts/shuntOhms(p.lpGrade,false)+p.lpReceiver.siemens*v.lpVolts+p.lpReceiver.amps;
    // Literal source transcription here deliberately independent of model arrays.
    const double rh[]{120000,120000,120000,120000,120000,680000,100000};
    const double rl[]{39000,39000,39000,39000,39000,220000,47000};
    for(unsigned i=0;i<7;++i) {
        double ih=(v.hpVolts-v.sourceTerminalVolts[i])/rh[i];
        double il=(v.lpVolts-v.sourceTerminalVolts[i])/rl[i]; hp+=ih;lp+=il;
        if(p.shared[i].ohms>0) near((v.sourceTerminalVolts[i]-p.shared[i].volts)/p.shared[i].ohms-ih-il,0,1e-15,"source KCL");
        else near(v.sourceTerminalVolts[i],p.shared[i].volts,1e-14,"ideal source");
    }
    hp+=(v.hpVolts-p.fh.volts)/(39000+p.fh.ohms)+(v.hpVolts-p.hpTrim.volts)/(150000+p.hpTrim.ohms);
    lp+=(v.lpVolts-p.fl.volts)/(39000+p.fl.ohms)+(v.lpVolts-p.lpTrim.volts)/(150000+p.lpTrim.ohms);
    near(hp,0,1e-15,"HP KCL");near(lp,0,1e-15,"LP KCL");
}
}
int main() { try {
    near(shuntOhms(Grade::A,true),2700,0,"R6 A"); near(shuntOhms(Grade::B,true),2200,0,"R6 B"); near(shuntOhms(Grade::C,true),1800,0,"R6 C");
    near(shuntOhms(Grade::A,false),3300,0,"R7 A"); near(shuntOhms(Grade::B,false),2700,0,"R7 B"); near(shuntOhms(Grade::C,false),2200,0,"R7 C");
    auto p=fixture(); auto v=solve(p); near(v.hpVolts,0,0,"zero HP");near(v.lpVolts,0,0,"zero LP");
    p.shared[0]={1,0};v=solve(p);
    const double gh=1./2700+5./120000+1./680000+1./100000+1./39000+1./150000;
    const double gl=1./3300+5./39000+1./220000+1./47000+1./39000+1./150000;
    near(v.hpVolts,1./120000/gh,1e-15,"HP independent divider");near(v.lpVolts,1./39000/gl,1e-15,"LP independent divider");
    check(v.lpVolts>v.hpVolts,"unequal HP/LP excursion");
    // With all shared sources ideal, FH cannot move LP. Finite SUB impedance couples them.
    p=fixture();p.fh.volts=10;near(solve(p).lpVolts,0,0,"ideal source isolates nodes");
    p.shared[4].ohms=100000;check(solve(p).lpVolts>0,"finite source back-coupling");kcl(p,solve(p));
    for(auto hg:{Grade::A,Grade::B,Grade::C}) for(auto lg:{Grade::A,Grade::B,Grade::C})
    for(int trial=0;trial<100;++trial) {
        p=fixture();p.hpGrade=hg;p.lpGrade=lg;
        for(unsigned i=0;i<7;++i) p.shared[i]={std::sin(trial+i)*12.,(trial+i)%3==0?0.:1000.*(trial+i+1)};
        p.fh={trial*.03,1234};p.fl={-trial*.07,5678};
        p.hpTrim=trimWiper(-15,15,trial/99.);p.lpTrim=trimWiper(-15,15,1-trial/99.);
        p.hpReceiver={1e-6,2e-6};p.lpReceiver={3e-6,-4e-6};kcl(p,solve(p));
    }
    near(trimWiper(-15,15,.5).ohms,25000,0,"loaded trim resistance");
    near(trimWiper(-15,15,0).volts,-15,0,"trim low");near(trimWiper(-15,15,1).volts,15,0,"trim high");
    const auto rejected=[](Inputs bad) {try {(void)solve(bad);return false;}catch(const std::invalid_argument&){return true;}};
    check(rejected(Inputs{}),"unknown model rejected");
    p=fixture();p.hpReceiver={};check(rejected(p),"unknown receiver rejected");
    p=fixture();p.shared[3]={};check(rejected(p),"unknown source rejected");
    p=fixture();p.lpGrade=Grade::unknown;check(rejected(p),"unknown grade rejected");
    p=fixture();p.fh.ohms=-1;check(rejected(p),"negative source R rejected");
    p=fixture();p.lpReceiver.siemens=-1;check(rejected(p),"negative receiver G rejected");
    p=fixture();p.shared[0].volts=std::numeric_limits<double>::infinity();check(rejected(p),"infinite input rejected");
    std::cout<<checks<<" loaded-control checks passed (synthetic boundaries, no hardware validation)\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }

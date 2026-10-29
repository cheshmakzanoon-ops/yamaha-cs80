#include "reference/Mn3001Packet.h"
#include <cstdio>
#include <limits>
#include <vector>
using namespace cs80::reference::mn3001;
namespace {
int checks=0, failures=0;
void check(bool ok, const char* name) { ++checks; if(!ok) {++failures; std::printf("FAIL: %s\n", name);} }
Phase phase(int event) { return event%2 ? Phase::cp1 : Phase::cp2; }
template<class F> void rejects(F f) {
    bool rejected=false; try { f(); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"invalid event/reset rejected");
}
// Independent history/tag oracle: one acquisition per completed CP2; output at
// cycle k is acquisition k-256, repeated on the following CP1. Not a stage solver.
void sequence(int kind, bool irregular) {
    CommonClockDual model(Phase::cp2);
    std::vector<double> history;
    double time=0;
    for(int e=0;e<1600;++e) {
        time += irregular ? .01+(e%11)*.001 : .01;
        double x=kind==0 ? (e==0 ? 1. : 0.) : kind==1 ? 2. : e*.125-40.;
        if(e%2==0) history.push_back(x);
        auto y=model.step(time,phase(e),x,-3*x);
        bool present=e>=512;
        double expected=present ? history[static_cast<std::size_t>(e/2-256)] : 0.;
        check(y.a.normalizedSum()==expected,"impulse/constant/ramp history oracle");
        check(y.b.normalizedSum()==-3*expected,"independent channel scaling");
        check(y.a.out1.present==(present && e%2==0),"OUT1 CP2 validity");
        check(y.a.out2.present==(present && e%2==1),"OUT2 CP1 validity");
        check(y.a.out1.value==(e%2==0 ? expected : 0.),"OUT1 cleared on CP1");
        check(y.a.out2.value==(e%2==1 ? expected : 0.),"OUT2 cleared on CP2");
        const auto snapshot=model;
        for(int i=0;i<3;++i) check(model.output()==y,"stopped clock holds output");
        check(model==snapshot,"observation never advances state");
    }
}
}
int main() {
    for(int kind=0;kind<3;++kind) for(bool irregular:{false,true}) sequence(kind,irregular);
    CommonClockDual tied(Phase::cp2);
    for(int e=0;e<1100;++e) { auto y=tied.step(e,phase(e),e,-0.0+e); check(y.a==y.b,"OE57 tied input/clock equality"); }
    const auto saved=tied;
    auto bad=[&](double t,Phase p,double a,double b) {
        rejects([&]{ tied.step(t,p,a,b); }); check(tied==saved,"invalid event atomic across both channels");
    };
    const double inf=std::numeric_limits<double>::infinity(), nan=std::numeric_limits<double>::quiet_NaN();
    bad(1100,Phase::cp1,0,0); bad(1100,static_cast<Phase>(99),0,0);
    bad(1099,Phase::cp2,0,0); bad(1000,Phase::cp2,0,0);
    for(double v:{inf,-inf,nan}) { bad(v,Phase::cp2,0,0); bad(1100,Phase::cp2,v,0); bad(1100,Phase::cp2,0,v); }
    rejects([&]{tied.reset(static_cast<Phase>(-1));}); check(tied==saved,"invalid reset atomic");
    auto control=saved;
    check(tied.step(1100,Phase::cp2,42,17)==control.step(1100,Phase::cp2,42,17),"valid continuation after rejection");
    tied.reset(Phase::cp2); check(tied==CommonClockDual(Phase::cp2),"reset clears history and clock boundary");
    check(tied.output()==DualOutput{},"reset explicit empty signal");
    // CP1 may be the explicitly chosen starting phase: no acquisition yet.
    Channel first(Phase::cp1);
    check(first.step(-5,Phase::cp1,123)==Output{},"initial CP1 cannot acquire input");
    for(int e=0;e<=513;++e) {
        auto y=first.step(e,phase(e),e==0 ? 7 : 0);
        check(y.normalizedSum()==(e==512 || e==513 ? 7 : 0),"exact event512/513 impulse arrival");
    }
    Channel cp1Validation(Phase::cp1); const auto blank=cp1Validation;
    rejects([&]{cp1Validation.step(0,Phase::cp1,nan);}); check(cp1Validation==blank,"unused CP1 input still validated atomically");
    // Truly separate channels may receive independent event schedules/inputs.
    Channel a(Phase::cp2), b(Phase::cp2);
    for(int e=0;e<514;++e) a.step(e,phase(e),5);
    check(b.output()==Output{},"unclocked independent channel remains empty");
    for(int e=0;e<514;++e) b.step(10000+e*3.,phase(e),-9);
    check(a.output().normalizedSum()==5 && b.output().normalizedSum()==-9,"independent clocks and signals");
    std::printf("%d checks, %d failures (normalized signal packets; not analog voltage)\n",checks,failures);
    return failures ? 1 : 0;
}

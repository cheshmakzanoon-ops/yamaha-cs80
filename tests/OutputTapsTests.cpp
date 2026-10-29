// Synthetic conditional AC fixtures, not measured CS-80 output calibration.
#include "reference/OutputTaps.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
using namespace cs80::reference::output_taps;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks; if (!ok) { if (failures < 20) std::cerr << label << '\n'; ++failures; }
}
void near(Complex a, Complex b, const char* label, double relative = 2e-10, double absolute = 1e-15) {
    check(std::abs(a-b) <= absolute + relative * std::max(std::abs(a),std::abs(b)), label);
}
Inputs fixture() {
    Inputs p;
    p.mode = Mode::high; p.frequencyHz = 123;
    p.sourceVolts = {1.2,-.8}; p.sourceOhms = {310,120};
    p.highSeriesOhms = 560; p.lowSeriesOhms = 4700; p.lowShuntOhms = 560;
    p.load = {LoadKind::finite,{10000,-2100}};
    return p;
}
using Wide = std::complex<long double>;
Wide w(Complex z) { return {z.real(),z.imag()}; }
// Independent physical 4-node nodal stamp: P (pre-cap), X, H, L.
// Each resistor/capacitor is stamped separately; no branch reduction is reused.
std::array<Complex,4> oracle(const Inputs& p) {
    constexpr int n=4;
    std::array<std::array<Wide,n>,n> a {};
    std::array<Wide,n> b {}, x {};
    auto branch = [&](int i,int j,Wide y) {
        a[i][i]+=y; a[j][j]+=y; a[i][j]-=y; a[j][i]-=y;
    };
    branch(0,1,Wide(0,2*std::numbers::pi_v<long double>*p.frequencyHz*10e-6L));
    branch(1,2,1.0L/p.highSeriesOhms);
    branch(1,3,1.0L/p.lowSeriesOhms);
    a[3][3]+=1.0L/p.lowShuntOhms;
    if (p.sourceOhms != Complex {}) {
        const Wide y=1.0L/w(p.sourceOhms); a[0][0]+=y; b[0]+=y*w(p.sourceVolts);
    } else { a[0].fill({}); a[0][0]=1; b[0]=w(p.sourceVolts); }
    const int loadNode=p.mode==Mode::high?2:3;
    if (p.load.kind==LoadKind::finite) a[loadNode][loadNode]+=1.0L/w(p.load.ohms);
    if (p.load.kind==LoadKind::shorted) { a[loadNode].fill({}); a[loadNode][loadNode]=1; }
    for (int i=0;i<n;++i) {
        long double scale=0; for (auto v:a[i]) scale=std::max(scale,std::abs(v));
        for (auto& v:a[i]) v/=scale;
        b[i]/=scale;
    }
    for (int k=0;k<n;++k) {
        int pivot=k; for (int i=k+1;i<n;++i)
            if (std::abs(a[i][k])>std::abs(a[pivot][k])) pivot=i;
        if (std::abs(a[pivot][k])<1e-20L) throw std::runtime_error("oracle singular");
        std::swap(a[pivot],a[k]); std::swap(b[pivot],b[k]);
        for (int i=k+1;i<n;++i) {
            const auto factor=a[i][k]/a[k][k];
            for (int j=k;j<n;++j) a[i][j]-=factor*a[k][j];
            b[i]-=factor*b[k];
        }
    }
    for (int i=n-1;i>=0;--i) {
        Wide v=b[i];for(int j=i+1;j<n;++j)v-=a[i][j]*x[j];x[i]=v/a[i][i];
    }
    std::array<Complex,n> out {}; for(int i=0;i<n;++i)out[i]={static_cast<double>(x[i].real()),static_cast<double>(x[i].imag())};
    return out;
}
void verify(const Inputs& p) {
    const auto r=solve(p); const auto o=oracle(p);
    near(r.capacitorInputVolts,o[0],"oracle pre-cap");
    near(r.commonVolts,o[1],"oracle common");
    near(r.highTapVolts,o[2],"oracle high");
    near(r.lowTapVolts,o[3],"oracle low");
    near(r.jackVolts,p.mode==Mode::high?o[2]:o[3],"oracle selected tap");
    near(r.sourceAmps,r.highSeriesAmps+r.lowSeriesAmps,"common KCL");
    near(r.lowSeriesAmps,r.shuntAmps+(p.mode==Mode::low?r.loadAmps:Complex{}),"low KCL");
    near(r.highSeriesAmps,p.mode==Mode::high?r.loadAmps:Complex{},"high KCL");
}
} // namespace
namespace {
void ports(const Inputs& p) {
    const auto r=solve(p);
    near(p.sourceVolts,r.capacitorInputVolts+p.sourceOhms*r.sourceAmps,"source KVL");
    near(r.capacitorInputVolts,r.capacitorVolts+r.commonVolts,"capacitor KVL");
    near(r.sourceAmps,Complex(0,2*std::numbers::pi*p.frequencyHz*10e-6)*r.capacitorVolts,"capacitor current");
    near(r.commonVolts-r.highTapVolts,p.highSeriesOhms*r.highSeriesAmps,"high resistor");
    near(r.commonVolts-r.lowTapVolts,p.lowSeriesOhms*r.lowSeriesAmps,"low resistor");
    near(r.lowTapVolts,p.lowShuntOhms*r.shuntAmps,"shunt resistor");
    if(p.load.kind==LoadKind::finite)near(r.jackVolts,p.load.ohms*r.loadAmps,"finite load");
    if(p.load.kind==LoadKind::open)check(r.loadAmps==Complex{},"open current exact");
    if(p.load.kind==LoadKind::shorted)check(r.jackVolts==Complex{},"short voltage exact");
}
template<class F> void invalid(F change) {
    auto p=fixture();change(p);bool rejected=false;
    try { (void)solve(p); } catch(const std::invalid_argument&) {rejected=true;}
    check(rejected,"invalid boundary accepted");
}
template<class F> void numerical(F change) {
    auto p=fixture();change(p);bool rejected=false;
    try { (void)solve(p); } catch(const std::overflow_error&) {rejected=true;}
    check(rejected,"unresolvable numerical fixture accepted");
}
int runTests() {
    for (auto mode:{Mode::high,Mode::low}) for(auto kind:{LoadKind::open,LoadKind::shorted,LoadKind::finite})
        for(double f:{.01,1.,100.,10000.,1e6}) for(Complex zs:{Complex{},Complex{500,0},Complex{0,1000},Complex{220,-700}}) {
            auto p=fixture();p.mode=mode;p.load.kind=kind;p.frequencyHz=f;p.sourceOhms=zs;
            verify(p);ports(p);
        }
    std::mt19937 rng(0x8037);std::uniform_real_distribution<double> u(0,1);
    for(int k=0;k<600;++k) {
        auto p=fixture();p.mode=k%2?Mode::low:Mode::high;
        p.load.kind=k%3==0?LoadKind::open:k%3==1?LoadKind::shorted:LoadKind::finite;
        p.frequencyHz=std::pow(10.,-1+6*u(rng));
        p.highSeriesOhms=100+56000*u(rng);p.lowSeriesOhms=1000+9000*u(rng);
        p.lowShuntOhms=100+560000*u(rng);
        p.sourceVolts={4*u(rng)-2,4*u(rng)-2};p.sourceOhms={2000*u(rng),4000*u(rng)-2000};
        p.load.ohms={100+100000*u(rng),20000*u(rng)-10000};verify(p);ports(p);
    }
    for(auto mode:{Mode::high,Mode::low})for(auto kind:{LoadKind::open,LoadKind::shorted,LoadKind::finite}) {
        auto p=fixture();p.mode=mode;p.load.kind=kind;p.frequencyHz=0;const auto r=solve(p);
        check(r.capacitorInputVolts==p.sourceVolts && r.capacitorVolts==p.sourceVolts,"DC cap voltage");
        check(r.commonVolts==Complex{} && r.highTapVolts==Complex{} && r.lowTapVolts==Complex{}
            && r.jackVolts==Complex{} && r.sourceAmps==Complex{} && r.highSeriesAmps==Complex{}
            && r.lowSeriesAmps==Complex{} && r.shuntAmps==Complex{} && r.loadAmps==Complex{},"DC zero ports");
        ports(p);
    }
    for(double f:{.1,100.,1e5})for(Complex zs:{Complex{},Complex{10000,2000}}) {
        auto p=fixture();p.frequencyHz=f;p.sourceOhms=zs;p.load.kind=LoadKind::open;
        const auto h=solve(p);p.mode=Mode::low;const auto l=solve(p);
        near(l.jackVolts/h.jackVolts,{560./5260,0},"open ratio cancellation");
        near(l.sourceAmps,h.sourceAmps,"open source loading unchanged");
        p.highSeriesOhms=56000;near(solve(p).jackVolts,l.jackVolts,"unselected high series independent");
        p.mode=Mode::high;near(solve(p).jackVolts,h.jackVolts,"open high series independent");
    }
    {
        auto p=fixture();p.sourceOhms={0,0};p.frequencyHz=1e9;p.load.ohms={1000,0};
        const auto h=solve(p);p.mode=Mode::low;const auto l=solve(p);
        const double q=560.*1000/(560+1000);const double expected=(q/(4700+q))/(1000./1560);
        near(l.jackVolts/h.jackVolts,{expected,0},"finite load ratio",1e-7);
        check(std::abs(l.jackVolts/h.jackVolts-560./5260)>.001,"finite load changes ratio");
    }
    for(auto mode:{Mode::high,Mode::low}) {
        auto p=fixture();p.mode=mode;p.sourceVolts={0,0};const auto r=solve(p);
        check(r.jackVolts==Complex{} && r.sourceAmps==Complex{},"zero excitation");
        p=fixture();p.mode=mode;p.load.ohms={0,3000};verify(p);ports(p);
        p.load.ohms={0,-3000};verify(p);ports(p);
        p=fixture();p.mode=mode;const auto a=solve(p);p.sourceVolts*=Complex{-.3,.7};
        near(solve(p).jackVolts,a.jackVolts*Complex{-.3,.7},"complex linearity");
    }
    for(auto mode:{Mode::high,Mode::low}) {
        auto p=fixture();p.mode=mode;p.load.kind=LoadKind::open;const auto opened=solve(p);
        p.load={LoadKind::finite,{1e14,0}};near(solve(p).jackVolts,opened.jackVolts,"finite approaches open",1e-9);
        p.load.kind=LoadKind::shorted;const auto shorted=solve(p);
        p.load={LoadKind::finite,{1e-8,0}};
        near(solve(p).loadAmps,shorted.loadAmps,"finite approaches short",1e-9);
        p=fixture();p.mode=mode;p.frequencyHz=1e-4;
        p.sourceOhms={0,1/(2*std::numbers::pi*p.frequencyHz*10e-6)};verify(p);ports(p);
    }
    invalid([](Inputs& p){p=Inputs{};});
    invalid([](Inputs& p){p.mode=Mode::unknown;});
    invalid([](Inputs& p){p.mode=static_cast<Mode>(99);});
    invalid([](Inputs& p){p.load.kind=LoadKind::unknown;});
    invalid([](Inputs& p){p.load.kind=static_cast<LoadKind>(99);});
    for(double bad:{unknown,-1.,std::numeric_limits<double>::infinity()}) {
        invalid([&](Inputs& p){p.frequencyHz=bad;});
        invalid([&](Inputs& p){p.highSeriesOhms=bad;});
        invalid([&](Inputs& p){p.lowSeriesOhms=bad;});
        invalid([&](Inputs& p){p.lowShuntOhms=bad;});
        invalid([&](Inputs& p){p.sourceOhms={bad,0};});
        invalid([&](Inputs& p){p.load.ohms={bad,0};});
    }
    invalid([](Inputs& p){p.highSeriesOhms=0;});invalid([](Inputs& p){p.lowSeriesOhms=0;});
    invalid([](Inputs& p){p.lowShuntOhms=0;});invalid([](Inputs& p){p.load.ohms={0,0};});
    invalid([](Inputs& p){p.sourceVolts={unknown,0};});invalid([](Inputs& p){p.sourceVolts={0,unknown};});
    invalid([](Inputs& p){p.sourceOhms={0,unknown};});invalid([](Inputs& p){p.load.ohms={0,unknown};});
    numerical([](Inputs& p){p.frequencyHz=1e-14;p.sourceOhms={0,1/(2*std::numbers::pi*p.frequencyHz*10e-6)};});
    numerical([](Inputs& p){p.frequencyHz=std::numeric_limits<double>::denorm_min();p.sourceVolts={1e-100,0};});
    numerical([](Inputs& p){p.sourceVolts={1e308,1e308};p.frequencyHz=1e5;
        p.highSeriesOhms=1e-100;p.lowSeriesOhms=1e-100;p.lowShuntOhms=1e-100;
        p.sourceOhms={0,1/(2*std::numbers::pi*p.frequencyHz*10e-6)};});
    numerical([](Inputs& p){p.sourceVolts={1e-300,0};p.sourceOhms={1e100,0};});
    {
        auto p=fixture();p.frequencyHz=1e308;const auto r=solve(p);
        check(std::abs(r.capacitorVolts)>0,"high frequency retains capacitor drop");
    }
    // Scale-sensitive tiny-current regression: no cancellation of reconstructed nodes.
    {
        auto p=fixture();p.sourceOhms={1e30,0};p.sourceVolts={1,0};p.load.kind=LoadKind::open;
        const auto r=solve(p);check(std::abs(r.sourceAmps.real()/1e-30-1)<1e-12,"tiny source current");
        check(std::abs(r.lowSeriesAmps.real()/1e-30-1)<1e-12,"tiny low branch current");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
} // namespace
int main() {try{return runTests();}catch(const std::exception& e){std::cerr<<"Unexpected rejection: "<<e.what()<<'\n';return 1;}}

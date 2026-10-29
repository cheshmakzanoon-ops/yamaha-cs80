// Conditional DC arithmetic only; synthetic fixtures are not measured CS-80 data.
#include "reference/LevelControl.h"
#include <iostream>
#include <random>
using namespace cs80::reference::level;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { if (failures < 20) std::cerr << message << '\n'; ++failures; }
}
void near(double a, double b, const char* message, double tolerance = 2e-9) {
    check(std::abs(a - b) <= tolerance * (1 + std::abs(a) + std::abs(b)), message);
}
Inputs fixture() {
    Inputs p;
    for (int b = 0; b < 43; ++b) p.sources[b] = {1.0 + b * .7, 1000.0 + b * 3000};
    for (int i = 0; i < 16; ++i) p.receivers[i] = {1e-6 * (1 + i % 7), (i % 5 - 2) * 1e-7};
    return p;
}
// Full 59-node oracle: stamp each physical branch separately. Long double and
// direct bus KCL avoid sharing the production source-elimination equations.
std::array<long double, 59> oracle(const Inputs& p) {
    constexpr int n = 59;
    std::array<std::array<long double, n>, n> a {};
    std::array<long double, n> rhs {}, x {};
    auto resistor = [&](int i, int j, long double ohms) {
        const long double g = 1.0L / ohms;
        a[i][i] += g; a[j][j] += g; a[i][j] -= g; a[j][i] -= g;
    };
    for (int rank = 0; rank < 2; ++rank)
        for (int voice = 0; voice < 8; ++voice) {
            const int node = rank * 8 + voice;
            resistor(node, 16, 180000);
            resistor(node, 17 + rank, 560000);
            resistor(node, 19 + voice, 270000);
            resistor(node, 27 + node, 270000);
            resistor(node, 43 + node, 270000);
            a[node][node] += p.receivers[node].siemens;
            rhs[node] -= p.receivers[node].amps;
        }
    for (int b = 0; b < 43; ++b) {
        const int node = 16 + b;
        if (p.sources[b].ohms == 0) {
            a[node].fill(0); a[node][node] = 1; rhs[node] = p.sources[b].volts;
        } else {
            a[node][node] += 1.0L / p.sources[b].ohms;
            rhs[node] += static_cast<long double>(p.sources[b].volts) / p.sources[b].ohms;
        }
    }
    for (int row = 0; row < n; ++row) {
        long double scale = 0;
        for (auto v : a[row]) scale = std::max(scale, std::abs(v));
        for (auto& v : a[row]) v /= scale;
        rhs[row] /= scale;
    }
    for (int k = 0; k < n; ++k) {
        int pivot = k;
        for (int i = k + 1; i < n; ++i)
            if (std::abs(a[i][k]) > std::abs(a[pivot][k])) pivot = i;
        if (std::abs(a[pivot][k]) < 1e-20L) throw std::runtime_error("singular oracle fixture");
        std::swap(a[k], a[pivot]); std::swap(rhs[k], rhs[pivot]);
        for (int i = k + 1; i < n; ++i) {
            const auto factor = a[i][k] / a[k][k];
            for (int j = k; j < n; ++j) a[i][j] -= factor * a[k][j];
            rhs[i] -= factor * rhs[k];
        }
    }
    for (int i = n - 1; i >= 0; --i) {
        long double v = rhs[i];
        for (int j = i + 1; j < n; ++j) v -= a[i][j] * x[j];
        x[i] = v / a[i][i];
    }
    return x;
}
// Independent topology list: never call the header's attached/branch helpers.
std::array<int, 5> buses(int i) { return {0, 1+i/8, 3+i%8, 11+i, 27+i}; }
constexpr std::array<double, 5> resistances {180000, 560000, 270000, 270000, 270000};
void compare(const Inputs& p) {
    const auto result = solve(p);
    const auto reference = oracle(p);
    long double receiverTotal = 0, sourceTotal = 0;
    for (int i = 0; i < 16; ++i) {
        near(result.nodeVolts[i], static_cast<double>(reference[i]), "full59 node voltage");
        const double receiver = p.receivers[i].siemens * result.nodeVolts[i] + p.receivers[i].amps;
        near(receiver, result.receiverAmps[i], "receiver current", 1e-13);
        long double current = receiver;
        for (int k=0;k<5;++k) current += (result.nodeVolts[i]-result.busVolts[buses(i)[k]])/resistances[k];
        near(static_cast<double>(current),0,"node KCL",1e-13);
        receiverTotal += receiver;
    }
    for (int b = 0; b < 43; ++b) {
        near(result.busVolts[b], static_cast<double>(reference[16 + b]), "full59 bus voltage");
        long double current = 0;
        for (int i=0;i<16;++i) for(int k=0;k<5;++k) if(buses(i)[k]==b)
            current += (reference[16+b]-reference[i])/resistances[k];
        near(result.sourceAmps[b], static_cast<double>(current), "independent source current", 1e-13);
        sourceTotal += result.sourceAmps[b];
        if (p.sources[b].ohms > 0)
            near((p.sources[b].volts-result.busVolts[b])/p.sources[b].ohms,
                 result.sourceAmps[b],"source KCL",1e-13);
    }
    near(static_cast<double>(sourceTotal),static_cast<double>(receiverTotal),"total current budget",1e-13);
}
template<class F> void rejects(F run, const char* what) {
    bool threw = false;
    try { run(); } catch (const std::invalid_argument&) { threw = true; }
    catch (const std::overflow_error&) { threw = true; }
    check(threw, what);
}
}
int runTests() {
    auto p=fixture();
    for(auto& s:p.sources) s.ohms=0;
    for(auto& r:p.receivers) r={0,0};
    auto result=solve(p);
    for(int i=0;i<16;++i) {
        double drive=0,g=0;
        for(int k=0;k<5;++k) {drive+=p.sources[buses(i)[k]].volts/resistances[k];g+=1/resistances[k];}
        near(result.nodeVolts[i],drive/g,"ideal weighted divider");
    }
    compare(p);
    p.receivers[0]={1e-5,-2e-6}; result=solve(p);
    double drive=2e-6,g=1e-5;
    for(int k=0;k<5;++k) {drive+=p.sources[buses(0)[k]].volts/resistances[k];g+=1/resistances[k];}
    near(result.nodeVolts[0],drive/g,"loaded analytic divider"); compare(p);
    std::mt19937 rng(29371); std::uniform_real_distribution<double> unit(0,1);
    for(int trial=0;trial<250;++trial) {
        p=fixture();
        for(int b=0;b<43;++b) p.sources[b]={30*unit(rng)-15,(trial+b)%7==0?0:std::pow(10.,1+6*unit(rng))};
        for(auto& r:p.receivers) r={2e-5*unit(rng),1e-5*(unit(rng)-.5)};
        compare(p);
    }
    // Independent closed-form single-output source resistance: its branch and
    // Thevenin R are in series. All other sources are ideal in this fixture.
    for (int individual : {11, 27}) {
        p = fixture();
        for (auto& source : p.sources) source.ohms = 0;
        p.sources[individual].ohms = 7e6;
        result = solve(p);
        double numerator = -p.receivers[0].amps;
        double denominator = p.receivers[0].siemens;
        for (int k = 0; k < 5; ++k) {
            const int b = buses(0)[k];
            const double conductance = 1.0 / (resistances[k] + p.sources[b].ohms);
            numerator += conductance * p.sources[b].volts;
            denominator += conductance;
        }
        near(result.nodeVolts[0], numerator / denominator, "finite individual-source analytic divider");
        compare(p);
    }
    // Every source changes independently, including finite single-node TI/TA.
    for(int b=0;b<43;++b) {p=fixture();p.sources[b]={-7,7e6};compare(p);}
    for(int i=0;i<16;++i) {p=fixture();p.receivers[i]={3e-5,-3e-6};compare(p);}
    p=fixture();auto original=solve(p);p.receivers[0].siemens*=20;auto changed=solve(p);
    check(std::abs(changed.nodeVolts[9]-original.nodeVolts[9])>1e-5,"AM cross-rank cross-voice coupling");
    p=fixture();for(auto& s:p.sources)s.ohms=0;p.sources[3].ohms=1e5;
    original=solve(p);p.receivers[0].siemens*=20;changed=solve(p);
    check(std::abs(changed.nodeVolts[8]-original.nodeVolts[8])>1e-5,"KB same-voice cross-rank coupling");
    for(int i=1;i<16;++i)if(i!=8)near(changed.nodeVolts[i],original.nodeVolts[i],"isolated KB pair",1e-13);
    p.sources[3].ohms=0;original=solve(p);p.receivers[0].siemens*=2;changed=solve(p);
    near(changed.nodeVolts[8],original.nodeVolts[8],"ideal KB removes pair coupling",1e-13);
    p=fixture();for(auto& s:p.sources)s.ohms=0;p.sources[1].ohms=1e5;
    original=solve(p);p.receivers[0].siemens*=20;changed=solve(p);
    check(std::abs(changed.nodeVolts[1]-original.nodeVolts[1])>1e-5,"LV rank-local coupling");
    for(int i=8;i<16;++i)near(changed.nodeVolts[i],original.nodeVolts[i],"LV does not cross rank",1e-13);
    p=fixture();original=solve(p);auto q=p;
    std::swap(q.sources[1],q.sources[2]);
    for(int i=0;i<8;++i){std::swap(q.receivers[i],q.receivers[i+8]);std::swap(q.sources[11+i],q.sources[19+i]);std::swap(q.sources[27+i],q.sources[35+i]);}
    changed=solve(q);for(int i=0;i<16;++i)near(changed.nodeVolts[i],original.nodeVolts[(i+8)%16],"rank permutation");
    p=fixture();for(auto& s:p.sources)s={0,1e30};for(auto& r:p.receivers)r={1e-5,-1e-5};result=solve(p);
    for(int b=0;b<43;++b)check(std::abs(result.sourceAmps[b]/(-result.busVolts[b]/1e30)-1)<1e-12,"tiny source current retained");
    rejects([]{solve(Inputs{});},"NaN defaults fail closed");
    for(int b=0;b<43;++b)for(bool volts:{false,true}){q=fixture();if(volts)q.sources[b].volts=unknown;else q.sources[b].ohms=unknown;rejects([&]{solve(q);},"missing source member");}
    for(int i=0;i<16;++i)for(bool amps:{false,true}){q=fixture();if(amps)q.receivers[i].amps=unknown;else q.receivers[i].siemens=unknown;rejects([&]{solve(q);},"missing receiver member");}
    for(double bad:{-1.,std::numeric_limits<double>::infinity()}) {
        q=fixture();q.sources[0].ohms=bad;rejects([&]{solve(q);},"invalid source R");
        q=fixture();q.receivers[0].siemens=bad;rejects([&]{solve(q);},"invalid receiver G");
    }
    q=fixture();q.sources[0].volts=std::numeric_limits<double>::infinity();rejects([&]{solve(q);},"infinite voltage");
    q=fixture();q.receivers[0].amps=std::numeric_limits<double>::infinity();rejects([&]{solve(q);},"infinite current");
    for(double r:{1e17,1e300}){q=fixture();for(auto& s:q.sources)s={1,r};for(auto& v:q.receivers)v={0,0};rejects([&]{solve(q);},"near floating rejected");}
    q=fixture();for(auto& s:q.sources)s={1,1e9};for(auto& v:q.receivers)v={0,0};result=solve(q);
    for(double v:result.nodeVolts)check(std::abs(v-1)<1e-10,"accepted common mode");
    q=fixture();q.receivers[0].amps=std::numeric_limits<double>::max();rejects([&]{solve(q);},"unrepresentable voltage");
    q=fixture();for(auto& s:q.sources)s.volts=0;for(auto& v:q.receivers)v.amps=0;result=solve(q);
    for(double v:result.nodeVolts)check(v==0,"exact zero");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
int main(){try{return runTests();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

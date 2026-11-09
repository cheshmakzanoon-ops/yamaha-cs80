// Preregistered source-graph tests. Compile with a disposable boundary-injected
// BbdChorus.h; see run-oe2-topology-experiment.py. No audio-law qualification.
#include "core/BbdChorus.h"
#include "experimental/Oe2Topology.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace {
int checks=0, failures=0;
void check(bool ok, const char* label) { ++checks; if (!ok) { ++failures; std::printf("FAIL %s\n",label); } }
void near(double got,double want,double tol,const char* label) {
    check(std::isfinite(got)&&std::isfinite(want)&&std::abs(got-want)<=tol,label);
}
}
int main() {
    // Literal net ledger: IC7/O1 receives IC5; IC9/O2 receives IC6;
    // IC8/O3 receives IC6; IC10/O4 receives IC5. No implementation formula oracle.
    const std::array<std::array<double,2>,4> oneBranchExpected {{{2,0},{7,0},{0,7},{0,2}}};
    for (int i=0;i<4;++i) {
        std::array<double,4> g{};g[(std::size_t)i]=1;
        const auto got=cs80::experimental::oe2::sumBranches({2,7},g);
        near(got[0],oneBranchExpected[(std::size_t)i][0],1e-12,"one-branch left net");
        near(got[1],oneBranchExpected[(std::size_t)i][1],1e-12,"one-branch right net");
    }
    struct Phase {double x,y;std::array<double,4> expected;};
    const std::array<Phase,4> phases {{{1,0,{1,0,-1,0}},{0,1,{0,1,0,-1}},
                                       {-1,0,{-1,0,1,0}},{0,-1,{0,-1,0,1}}}};
    for(const auto& c:phases) {
        const auto got=cs80::experimental::oe2::relativeControls(c.x,c.y);
        for(int i=0;i<4;++i) near(got[(std::size_t)i],c.expected[(std::size_t)i],0,"inverter pair");
    }
    struct Case {const char* name;float upper,lower;double x,y;float depth;bool discriminating;};
    const std::array<Case,7> cases {{{"upper-only",1,0,0,0,0,true},
        {"lower-only",0,1,0,0,0,true},{"positive-LO1",1,1,.5,0,1,true},
        {"positive-LO2",1,1,0,.5,1,true},{"negative-LO1",1,1,-.5,0,1,true},
        {"negative-LO2",1,1,0,-.5,1,true},{"coherent-no-modulation",1,1,0,0,0,false}}};
    // Explicitly frozen from the prior software: NOT measured hardware gains.
    constexpr std::array<double,4> weights {.65,.58,.58,.65};
    constexpr double scale=.5/(.65+.58), polarity=-1, tolerance=2e-7;
    check(cs80::BbdChorus::kBranchVpp==weights,"retained O1..O4 weight order");
    check(cs80::BbdChorus::kWetGain==.5f&&cs80::BbdChorus::kWetPolarity==-1.f,"retained wet hypothesis");
    for(const auto& c:cases) {
        cs80::BbdChorus legacy,candidate; legacy.prepare(96000);candidate.prepare(96000);
        topology_probe::wet={c.upper,c.lower};topology_probe::lo1=c.x;topology_probe::lo2=c.y;
        const auto old=legacy.process(0,0,1,c.depth,true,true);
        const auto got=candidate.processExperimentalIndependent(0,0,1,c.depth,false,true,cs80::BbdChorus::ExperimentalTopology::sourceGraph);
        const std::array<double,4> gains {scale*weights[0]*(1+c.depth*c.x),scale*weights[1]*(1+c.depth*c.y),
                                          scale*weights[2]*(1-c.depth*c.x),scale*weights[3]*(1-c.depth*c.y)};
        const double left=polarity*(gains[0]*c.upper+gains[1]*c.lower);
        const double right=polarity*(gains[2]*c.lower+gains[3]*c.upper);
        near(got.l,left,tolerance,"actual candidate left route");near(got.r,right,tolerance,"actual candidate right route");
        const double oldLeft=polarity*static_cast<float>(scale*(weights[0]*(1+c.depth*c.x)+weights[1]*(1+c.depth*c.y)))*c.upper;
        const double oldRight=polarity*static_cast<float>(scale*(weights[2]*(1-c.depth*c.y)+weights[3]*(1-c.depth*c.x)))*c.lower;
        near(old.l,oldLeft,0,"legacy left unchanged");near(old.r,oldRight,0,"legacy right unchanged");
        const double oldError=std::max(std::abs(old.l-left),std::abs(old.r-right));
        check(std::isfinite(oldError)&&(c.discriminating?oldError>tolerance:oldError<=tolerance),"fixture discrimination");
        std::printf("case=%s candidate=(%.9g,%.9g) source=(%.9g,%.9g) legacy_source_error=%.9g\n",c.name,got.l,got.r,left,right,oldError);
    }
    std::printf("OE2 topology: %d checks, %d failures; arithmetic tolerance %.9g; no hardware-parity claim\n",checks,failures,tolerance);
    return failures?1:0;
}

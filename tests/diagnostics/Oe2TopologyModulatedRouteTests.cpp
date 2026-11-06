// Included in the clean-main preregistration after earlier independent review.
// Keeps all source-route thresholds unchanged. Distinct wet-node values
// and simultaneous unequal phase markers exercise routing during modulation.
#include "core/BbdChorus.h"
#include <array>
#include <cmath>
#include <cstdio>
int main() {
    struct Case {float u,v;double x,y;float depth;bool chorus;};
    const std::array<Case,4> cases {{{2,-.7f,.3,-.8,.37f,false},
        {-2,.7f,-.3,.8,.37f,false},{2,-.7f,.3,-.8,1,true},{-2,.7f,-.3,.8,1,true}}};
    constexpr double k=.5/(.65+.58),tol=2e-7;
    int checks=0,failures=0;
    for(const auto& c:cases) {
        cs80::BbdChorus b;b.prepare(96000);
        topology_probe::wet={c.u,c.v};topology_probe::lo1=c.x;topology_probe::lo2=c.y;
        const auto got=b.processExperimentalIndependent(.1f,-.2f,4,c.depth,c.chorus,true,cs80::BbdChorus::ExperimentalTopology::sourceGraph);
        // Literal graph ledger with the frozen affine hypothesis, independently
        // written here; no production helper is used to calculate this oracle.
        const double o1=k*.65*(1+c.depth*c.x)*c.u;
        const double o2=k*.58*(1+c.depth*c.y)*c.v;
        const double o3=k*.58*(1-c.depth*c.x)*c.v;
        const double o4=k*.65*(1-c.depth*c.y)*c.u;
        const std::array<double,2> expected{double(.1f)-o1-o2,double(-.2f)-o3-o4};
        const std::array<double,2> actual{got.l,got.r};
        for(int i=0;i<2;++i) {
            ++checks;
            if(!std::isfinite(actual[(std::size_t)i])||!std::isfinite(expected[(std::size_t)i])||
               std::abs(actual[(std::size_t)i]-expected[(std::size_t)i])>tol)++failures;
        }
    }
    std::printf("Additive modulated route regression: %d checks, %d failures; unchanged tolerance %.9g\n",checks,failures,tol);
    return failures?1:0;
}

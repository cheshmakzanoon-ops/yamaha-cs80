// Clean-main port: observations only, no oscillator or wet-node replacements.
// Verifies the software's independent logical paths, not physical panel switching.
#include "core/BbdChorus.h"
#include <array>
#include <cmath>
#include <cstdio>
int main() {
    int failures=0;unsigned long long checks=0,bothSamples=0;
    double clockError=0,phaseError=0,outputError=0;
    constexpr double pi=3.14159265358979323846,nominal=106904.5,k=.5/(.65+.58);
    constexpr double scalarTolerance=1e-10,outputTolerance=2e-7;
    for(float rate:{88200.f,96000.f,192000.f}) for(bool source:{false,true}) {
        cs80::BbdChorus b;b.prepare(rate);
        for(int n=0;n<24000;++n) {
            const int mode=(n/3000)%4;const bool chorus=(mode&1)!=0,tremolo=(mode&2)!=0;
            const float inL=float(.2*std::sin(.018*n)),inR=float(.1*std::cos(.031*n));
            const auto got=b.processExperimentalIndependent(inL,inR,4,.75f,chorus,tremolo,
                source?cs80::BbdChorus::ExperimentalTopology::sourceGraph:cs80::BbdChorus::ExperimentalTopology::historicalGrouping);
            const double x=std::sin(2*pi*4*(n+1)/rate),y=std::sin(2*pi*4*(.056/.55)*(n+1)/rate);
            const double expectedClock=chorus?std::exp2(.75*.5*.5*(x+y)):1;
            const double ce=std::abs(topology_observe::clockHz/nominal-expectedClock);
            const double pe=std::max(std::abs(topology_observe::phase1-x),std::abs(topology_observe::phase2-y));
            const double m=tremolo?.75:0,u=topology_observe::wet[0],v=topology_observe::wet[1];
            double left=inL,right=inR;
            if(chorus||tremolo) {
                if(source) {
                    left-=k*(.65*(1+m*x)*u+.58*(1+m*y)*v);
                    right-=k*(.58*(1-m*x)*v+.65*(1-m*y)*u);
                } else {
                    left-=k*(.65*(1+m*x)+.58*(1+m*y))*u;
                    right-=k*(.58*(1-m*y)+.65*(1-m*x))*v;
                }
            }
            const double oe=std::max(std::abs(got.l-left),std::abs(got.r-right));
            const bool finite=std::isfinite(got.l)&&std::isfinite(got.r)&&std::isfinite(left)&&std::isfinite(right)
                &&std::isfinite(ce)&&std::isfinite(pe)&&std::isfinite(oe)
                &&std::isfinite(topology_observe::phase1)&&std::isfinite(topology_observe::phase2)
                &&std::isfinite(topology_observe::clockHz)&&std::isfinite(u)&&std::isfinite(v);
            if(!finite||ce>scalarTolerance||pe>scalarTolerance||oe>outputTolerance)++failures;
            ++checks;if(chorus&&tremolo)++bothSamples;
            if(finite){clockError=std::max(clockError,ce);phaseError=std::max(phaseError,pe);outputError=std::max(outputError,oe);}
        }
    }
    std::printf("Independent paths: %llu samples (%llu BOTH), failures=%d, normalized_clock_error=%.12g phase_error=%.12g output_error=%.12g\n",checks,bothSamples,failures,clockError,phaseError,outputError);
    return failures?1:0;
}

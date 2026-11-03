// Compare the new default paths with the SHA-pinned, unmodified main header.
// The runner changes only that frozen header's namespace to permit side-by-side use.
#include "core/BbdChorus.h"
#include "FrozenBbdChorus.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>

namespace {
std::uint64_t checks=0,mismatches=0,nonfinite=0;
void same(float a,float b) {
    ++checks;
    if(!std::isfinite(a)||!std::isfinite(b)) ++nonfinite;
    if(std::bit_cast<std::uint32_t>(a)!=std::bit_cast<std::uint32_t>(b)) ++mismatches;
}
template<class A,class B> void pair(A a,B b){same(a.l,b.l);same(a.r,b.r);}
}
int main() {
    if(sizeof(cs80::BbdChorus)!=sizeof(cs80_oe2_frozen::BbdChorus)) {
        std::printf("FAIL object size changed\n");return 1;
    }
    for(float rate:{88200.f,96000.f,192000.f}) for(int scenario=0;scenario<12;++scenario) {
        cs80::BbdChorus now,nowTwoState;
        cs80_oe2_frozen::BbdChorus old,oldTwoState;
        now.prepare(rate);old.prepare(rate);nowTwoState.prepare(rate);oldTwoState.prepare(rate);
        for(int n=0;n<16384;++n) {
            // Includes exact idle history, asymmetric stereo, impulses and changing controls.
            const bool idle=n<1024||(n>=8192&&n<12288);
            const float l=idle?0.f:float(.24*std::sin(.011*n)+.07*std::cos(.037*n)+(n==4096?.3:0));
            const float r=idle?0.f:float(.17*std::sin(.019*n)-.05*std::cos(.029*n));
            const int segment=(n/1024+scenario)%4;
            const bool chorus=(segment&1)!=0,tremolo=(segment&2)!=0;
            const std::array<float,4> speeds{.1f,1.7f,8.f,float(1/.056)};
            const std::array<float,4> depths{0.f,1.f,.37f,.8f};
            const float speed=speeds[std::size_t((n/512+scenario)%4)];
            const float depth=depths[std::size_t((n/731+scenario)%4)];
            pair(now.process(l,r,speed,depth,chorus,tremolo),old.process(l,r,speed,depth,chorus,tremolo));
            pair(nowTwoState.processExperimentalIndependent(l,r,speed,depth,chorus&&!tremolo,chorus&&tremolo,cs80::BbdChorus::ExperimentalTopology::historicalGrouping),oldTwoState.process(l,r,speed,depth,chorus,tremolo));
            if(n==6000) {
                auto copiedNow=now;auto copiedOld=old;
                for(int j=0;j<512;++j) pair(copiedNow.process(l,r,speed,depth,true,true),copiedOld.process(l,r,speed,depth,true,true));
                now=std::move(copiedNow);old=std::move(copiedOld);
            }
            if(n==10000) {now.reset();old.reset();nowTwoState.reset();oldTwoState.reset();}
        }
    }
    // Experimental output selection must not alter oscillator, delay, filter or RNG state.
    for(float rate:{88200.f,96000.f,192000.f}) {
        cs80::BbdChorus selected;cs80_oe2_frozen::BbdChorus old;
        selected.prepare(rate);old.prepare(rate);
        for(int n=0;n<24000;++n) {
            const float x=float(.2*std::sin(.03*n));
            const bool selectExperiment=n<12000;
            const auto expected=old.process(x,-.3f*x,3.2f,.9f,true,false);
            const auto actual=selectExperiment?selected.processExperimentalIndependent(x,-.3f*x,3.2f,.9f,true,true,cs80::BbdChorus::ExperimentalTopology::sourceGraph):selected.process(x,-.3f*x,3.2f,.9f,true,false);
            if(!selectExperiment) pair(actual,expected);
        }
    }
    std::printf("Frozen legacy: %llu sample-channel checks; %llu bit mismatches; %llu nonfinite; sizeof unchanged\n",
                (unsigned long long)checks,(unsigned long long)mismatches,(unsigned long long)nonfinite);
    return mismatches||nonfinite?1:0;
}

#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cs80;
struct Snapshot { double env[3]{}, hz[3]{}; };
int main() {
    constexpr int sr=48000, secondOn=28800, firstOff=26400, total=48000;
    std::printf("program,frame,continuous_envelope,overlap_envelope,detached_envelope,continuous_hz,overlap_hz,detached_hz\n");
    int failures=0;
    for (const char* name : {"UFO Landing", "Gong", "Temple Bell", "Ring Sweep Bell"}) {
        const auto& bank=curatedLibrary();
        auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==name;});
        if(it==bank.end()) throw std::runtime_error(name);
        std::array<std::unique_ptr<Engine>,3> engines;
        for(auto& e:engines) { e=std::make_unique<Engine>(); e->prepare(sr); e->setParams(it->values); e->noteOn(60,.8f,1); }
        double maxOverlapEnvError=0,maxOverlapHzError=0,maxDetachedDifference=0;
        bool finite=true;
        for(int frame=0;frame<total;frame++) {
            if(frame==firstOff) engines[2]->noteOff(60,1);
            if(frame==secondOn) { engines[1]->noteOn(67,.8f,1); engines[2]->noteOn(67,.8f,1); }
            float a[2];
            for(auto& e:engines) { e->render(a,1);finite=finite&&std::isfinite(a[0])&&std::isfinite(a[1]); }
            if(frame>=secondOn) {
                maxOverlapEnvError=std::max(maxOverlapEnvError,std::abs(double(engines[0]->getRingEnvelope())-engines[1]->getRingEnvelope()));
                maxOverlapHzError=std::max(maxOverlapHzError,std::abs(double(engines[0]->getRingFrequency())-engines[1]->getRingFrequency()));
                maxDetachedDifference=std::max(maxDetachedDifference,std::abs(double(engines[0]->getRingEnvelope())-engines[2]->getRingEnvelope()));
            }
            if(frame==secondOn-1||frame==secondOn+479||frame==secondOn+4799||frame==total-1) {
                std::printf("%s,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g\n",name,frame,
                    engines[0]->getRingEnvelope(),engines[1]->getRingEnvelope(),engines[2]->getRingEnvelope(),
                    engines[0]->getRingFrequency(),engines[1]->getRingFrequency(),engines[2]->getRingFrequency());
            }
        }
        std::fprintf(stderr,"%s: overlap max envelope error %.12g; carrier error %.12g Hz; detached max envelope difference %.12g; finite %d\n",name,maxOverlapEnvError,maxOverlapHzError,maxDetachedDifference,int(finite));
        if(maxOverlapEnvError!=0||maxOverlapHzError!=0||maxDetachedDifference<.1||!finite) ++failures;
    }
    return failures?1:0;
}

// Counts this fixture's work for real four-stage calls. There is no timer.
// Usage: stage-work SCENE FIRST_BLOCK LAST_BLOCK (256 calls per block).
#include "core/Ig00156Batch.h"
#include <valgrind/callgrind.h>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

int main(int argc,char** argv)
{
    using Stage=cs80::Ig00156Stage; using Batch=cs80::Ig00156Batch;
    if(argc!=4) return 2;
    const int scene=std::atoi(argv[1]), first=std::atoi(argv[2]), last=std::atoi(argv[3]);
    if(scene<0 || scene>4 || first<1 || last<first || last>2) return 2;
    const auto kernel=Batch::selectKernel();
    if(!kernel) return 3;

    std::array<Stage,4> stages;
    for(int i=0;i<4;++i)
    {
        auto net=(scene==0 || (scene==2 && i%2==0)) ? cs80::kIg00156HighPassNetwork
                                                     :cs80::kIg00156LowPassNetwork;
        float fc=340.f+580.f*i, q=.65f+.65f*i;
        if(scene==3 && i==0) net.inputFarads=1.e-12; // legal nondefault coupling
        if(scene==4 && i==0) // deliberately drive the ordered 1e6 equality edge
        {
            net.sourceOhms=65536.; fc=300.f; q=.6f;
            Stage reference(net); reference.set(fc,q,96000.f);
            net.inputFarads=(reference.halfSteps().couplings/net.sourceOhms)/1000000.;
        }
        stages[i]=Stage(net);
        stages[i].set(fc,q,96000.f);
        if(scene==4 && i==0 && (stages[i].halfSteps().couplings/net.inputFarads)/net.sourceOhms!=1000000.) return 4;
    }

    Batch::Stages p{&stages[0],&stages[1],&stages[2],&stages[3]};
    Batch::Inputs x{}; Batch::Outputs y{};
    unsigned hits=0;
    std::uint64_t hash=14695981039346656037ull;
    for(int block=0;block<=last;++block)
    {
        if(block==first) { CALLGRIND_ZERO_STATS; CALLGRIND_TOGGLE_COLLECT; }
        for(int t=0;t<256;++t)
        {
            for(int i=0;i<4;++i) x[i]=scene==4 ? 0.f : float(((block*256+t)*31+i*149)%2048-1023)*.0002f;
            hits+=kernel(p,x,y);
            for(auto out:y) for(float value:{out.lp,out.hp,out.bp})
            {
                hash^=std::bit_cast<std::uint32_t>(value);
                hash*=1099511628211ull;
            }
        }
    }
    CALLGRIND_TOGGLE_COLLECT;
    std::printf("groups=%u checksum=%016llx\n",hits,(unsigned long long)hash);
    return hits==unsigned((last+1)*256)?0:5;
}

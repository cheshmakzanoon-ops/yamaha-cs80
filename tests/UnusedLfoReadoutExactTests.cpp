#include "core/ModulationLfo.h"
#include "fixtures/unused_lfo/ModulationLfoBefore.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Frozen software continuation plus independent routing identities, preregistered
// before evaluation in unused-lfo-readout-exact.json. No hardware claim or tolerance.
namespace {
int checks=0,failures=0;
std::uint64_t samples=0;
void check(bool ok,const char* what,float rate=0,int variant=-1)
{ ++checks; if(!ok){++failures;std::printf("FAIL: %s rate=%.0f variant=%d\n",what,rate,variant);} }
bool same(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool same(double a,double b){return std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b);}
constexpr std::array<float,6> sampleRates {88200,96000,176400,192000,352800,384000};

// Before the implementation the baseline has the three-argument API. Evaluating
// that baseline is useful too; after implementation this selects the emit flag.
// Removing the new API would still fail the production Engine four-argument call.
template<class Lfo> float tick(Lfo& lfo,float rate,float sampleRate,int wave,bool emit)
{
    if constexpr(requires {lfo.tick(rate,sampleRate,wave,emit);})
        return lfo.tick(rate,sampleRate,wave,emit);
    else return lfo.tick(rate,sampleRate,wave);
}
struct Pair {
    cs80::ModulationLfo actual {0xdecaf123u};
    cs80::unused_lfo_before::ModulationLfo before {0xdecaf123u};
    bool phase=true,output=true,defaultOutput=true;
    void reset(float p){actual.reset(p);before.reset(p);}
    void advance(float rate,float sr,int wave,bool emit,bool useDefault=false)
    {
        ++samples;
        const float a=useDefault?actual.tick(rate,sr,wave):tick(actual,rate,sr,wave,emit);
        const float b=before.tick(rate,sr,wave);
        phase&=same(actual.getPhase(),before.getPhase())&&same(actual.getShapePhase(),before.getShapePhase());
        if(emit) output&=same(a,b);
        if(useDefault) defaultOutput&=same(a,b);
    }
};

void classContinuation(float sr)
{
    for(int wave=0;wave<cs80::kLfoWaveCount;++wave)
    {
        Pair p;p.reset(-0.0f);
        // A half second without readouts spans more than fifteen random updates.
        for(int n=0;n<(int)(0.75*sr);++n)
            p.advance(31.25f,sr,wave,n>=(int)(0.5*sr));
        Pair copied=p;
        for(int n=0;n<4096;++n) {
            const bool emit=n>=3072;
            p.advance(7.3f,sr,wave,emit);
            copied.advance(7.3f,sr,wave,emit);
        }
        check(p.phase&&copied.phase,"both LFO phases stay exact during disabled/readout/copy intervals",sr,wave);
        check(p.output&&copied.output,"enabled waveform and random continuation stays bit-identical",sr,wave);
        p.reset(0.875f);
        for(int n=0;n<8192;++n)p.advance(13.7f,sr,wave,true,true);
        check(p.defaultOutput,"default three-argument LFO output is unchanged after reset",sr,wave);
    }
    Pair p;p.reset(0.25f);
    constexpr std::array<float,7> rates {0.0f,0.05f,0.28f,1.1f,7.3f,20.0f,60000.0f};
    for(int n=0;n<(int)(0.75*sr);++n)
    {
        if(n==(int)(0.2*sr))p.reset(-0.0f);
        if(n==(int)(0.6*sr))p.reset(0.999f);
        const int wave=(n/257)%cs80::kLfoWaveCount;
        const float rate=rates[(n/139)%rates.size()];
        const bool emit=(n/4096)%3==2;
        p.advance(rate,sr,wave,emit);
    }
    for(int n=0;n<8192;++n)p.advance(17.0f,sr,(n/113)%cs80::kLfoWaveCount,true);
    check(p.phase,"rate/wave/reset changes retain both disabled phases",sr);
    check(p.output,"random-shape transitions while disabled retain later enabled outputs",sr);
}

}
int main()
{
    for(float sr:sampleRates)classContinuation(sr);
    std::printf("unused LFO class exact: %d checks, %d failures; %llu paired samples\n",checks,failures,(unsigned long long)samples);
    return failures==0?0:1;
}

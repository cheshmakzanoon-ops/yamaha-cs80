// Additional source-time stress evidence, not a factory or acoustic-fit test.
#include "core/InstrumentBody.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>

int main()
{
    using namespace cs80::body;
    int cases=0, failures=0;
    double peak=0.0, seconds=0.0;
    for(double rate : {44100.,48000.,96000.})
        for(int note : {12,36,60,84,108,127})
            for(float size : {.5f,2.5f})
                for(float decay : {.25f,4.f})
                    for(std::uint32_t seed : {1u,152947u,0x7fffffffu})
                    {
                        InstrumentBody b;
                        b.prepare(rate*2,rate);
                        b.noteOn(bell,float(note),1,1,decay,seed,size,standard,gong);
                        const float hz=float(440*std::exp2((note-69)/12.));
                        const int count=int(rate*2*1.6);
                        bool valid=true;
                        for(int i=0;i<count;++i)
                        {
                            // Changes cover the complete colour interval while
                            // delayed plate modes enter; no output normalization.
                            const float colour=((i/int(rate*.08))&1)?0.f:1.f;
                            const float x=b.tick(hz,colour);
                            peak=std::max(peak,std::abs(double(x)));
                            valid=valid&&std::isfinite(x)&&std::abs(x)<.8341f;
                        }
                        ++cases;failures+=!valid;seconds+=1.6;
                    }
    std::printf("Gong extended probe: %d cases, %d failures, peak %.9f, %.1f generator seconds\n",cases,failures,peak,seconds);
    return failures?1:0;
}

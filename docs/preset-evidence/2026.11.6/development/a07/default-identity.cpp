#include "core/InstrumentBody.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cmath>
int main(){for(int model=0;model<=19;++model){if(model==1)continue;for(int variant=0;variant<=4;++variant)for(int note:{48,60,84})for(double rate:{44100.,48000.,96000.}){cs80::body::InstrumentBody b;b.prepare(rate*2,rate);b.noteOn(model,float(note),.63f,.58f,1.3f,0x763918u,1.15f,variant);std::uint64_t hash=14695981039346656037ull;const float f=float(440*std::exp2((note-69)/12.));for(int i=0;i<12000;++i){const auto bits=std::bit_cast<std::uint32_t>(b.tick(f,i<6000?.58f:.82f,.2f));for(int j=0;j<4;++j){hash^=(bits>>(8*j))&255;hash*=1099511628211ull;}}std::printf("%d,%d,%d,%.0f,%016llx\n",model,variant,note,rate,(unsigned long long)hash);}}}

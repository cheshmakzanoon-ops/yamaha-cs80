#include "core/InstrumentBody.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
int main(){
 for(double rate:{44100.,48000.,96000.})for(int model:{cs80::body::nylon,cs80::body::steel,cs80::body::bass})
 for(float note:{36.f,60.f,84.f})for(float velocity:{.25f,.9f})for(float size:{.65f,1.75f}){
  cs80::body::InstrumentBody b;b.prepare(rate*2,rate);b.noteOn(model,note,velocity,.65f,1.25f,16714,size);
  std::uint64_t hash=14695981039346656037ull;
  for(int i=0;i<(int)(rate*.5);++i){auto bits=std::bit_cast<std::uint32_t>(b.tick((float)(440.*std::exp2((note-69.)/12.)),.65f));for(int j=0;j<4;++j){hash^=(bits>>(j*8))&255;hash*=1099511628211ull;}}
  std::printf("%.0f %d %.0f %.2f %.2f %016llx\n",rate,model,note,velocity,size,(unsigned long long)hash);
 }
}

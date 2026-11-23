#include "BaselineInstrumentBody.h"
#include "core/InstrumentBody.h"
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cmath>
int main() {
  unsigned long long samples=0, differences=0;
  for (int model=1; model<=19; ++model)
  for (double rate : {44100.,48000.,96000.})
  for (int note : {24,36,60,84,96})
  for (float colour : {0.f,.65f,1.f})
  for (float velocity : {.2f,.8f,1.f}) {
    if(model==cs80::body::tine && velocity!=1.f) continue;
    oldcs80::body::InstrumentBody oldBody;
    cs80::body::InstrumentBody newBody;
    oldBody.prepare(rate*2,rate); newBody.prepare(rate*2,rate);
    oldBody.noteOn(model,note,velocity,colour,1.f,0x7812a3u);
    newBody.noteOn(model,note,velocity,colour,1.f,0x7812a3u);
    float f=440.f*std::exp2((note-69.f)/12.f);
    for (int i=0;i<4096;++i) {
      float a=oldBody.tick(f,colour), b=newBody.tick(f,colour);
      differences += std::bit_cast<std::uint32_t>(a)!=std::bit_cast<std::uint32_t>(b);
      ++samples;
    }
  }
  std::printf("Exact full-strike tine / all other models parity: %llu samples, %llu differences\n",samples,differences);
  return differences ? 1:0;
}

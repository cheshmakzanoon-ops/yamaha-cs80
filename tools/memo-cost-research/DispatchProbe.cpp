#include "core/Engine.h"
#include <xmmintrin.h>
#include <cfenv>
#include <array>
#include <cstdio>
#include <cmath>
int main(int argc,char** argv) {
 if(argc!=2)return 2;auto*f=std::fopen(argv[1],"wb");if(!f)return 3;
 const auto original=_mm_getcsr();const auto rounding=std::fegetround();unsigned cases=0;
 for(int round:{FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO})
 for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u})for(double rate:{44100.,48000.,96000.})
 for(int policy:{0,1,2})for(int quality:{0,1,2}) {
  std::fesetround(round);_mm_setcsr((_mm_getcsr()&~0x807fu)|denorm|0x1f80u);
  cs80::Engine e;e.prepare(rate);cs80::ControlFrame frame;
  frame.hardwareSources={0,0};frame.effectiveValues[cs80::P_sourcePerformance]=policy==1?1.f:0.f;
  frame.effectiveValues[cs80::P_parityMode]=policy==2?1.f:0.f;
  frame.effectiveValues[cs80::P_oversampling]=(float)quality;
  if(!e.setControlFrame(frame))return 4;
  for(int n=0;n<8;++n)e.noteOn(48+n,.75f,1);
  std::array<float,256> audio;
  for(int block=0;block<3;++block) {
   if(block==1)e.controlChange(1,.5f,1);if(block==2)e.allNotesOff();
   e.render(audio.data(),128);for(float x:audio)if(!std::isfinite(x))return 5;
   auto status=_mm_getcsr();if(std::fwrite(audio.data(),sizeof(float),audio.size(),f)!=audio.size())return 6;
   if(std::fwrite(&status,sizeof(status),1,f)!=1)return 7;
  }++cases;
 }
 std::fesetround(rounding);_mm_setcsr(original);if(std::fclose(f))return 8;
 std::printf("PASS dispatch cases=%u\n",cases);return 0;
}

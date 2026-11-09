#include "core/Engine.h"
#include <cmath>
#include <cstdio>
#include <array>
__attribute__((noinline)) float fused(float x){return std::fma(7.f-x,.0015f,x);}
__attribute__((noinline)) float split(float x){volatile float product=(7.f-x)*.0015f;return x+product;}
int main(){
 for(int wave=0;wave<6;++wave){
  cs80::Engine e;e.prepare(48000);cs80::ParamValues p;p[cs80::P_subOriginalMode]=0;p[cs80::P_subWave]=float(wave);p[cs80::P_subSpeed]=7;
  p[cs80::P_subVco]=.2f;p[cs80::P_subVcf]=.2f;p[cs80::P_subVca]=.2f;e.setParams(p);
  float a=4.2f,b=4.2f;int fa=-1,fb=-1;std::array<float,2> audio{};
  for(int frame=0;frame<512;++frame){for(int os=0;os<2;++os){a=fused(a);b=split(b);}e.render(audio.data(),1);
   if(fa<0&&a!=e.getSubOscillatorRate())fa=frame;if(fb<0&&b!=e.getSubOscillatorRate())fb=frame;
   if(frame==398)std::printf("wave=%d frame398 fused=%a split=%a actual=%a\n",wave,double(a),double(b),double(e.getSubOscillatorRate()));
  }std::printf("wave=%d first_fused=%d first_split=%d\n",wave,fa,fb);
 }
}

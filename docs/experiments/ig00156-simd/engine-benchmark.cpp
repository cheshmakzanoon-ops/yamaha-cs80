// Offline one-case comparison driver. No driver/DAW/deadline claim.
#include "core/Engine.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
using namespace cs80;
int main(int argc,char**argv){
 if(argc!=7){std::fprintf(stderr,"policy voices bypass moving accelerated blocks\n");return 2;}
 const int policy=std::atoi(argv[1]),voices=std::atoi(argv[2]),bypass=std::atoi(argv[3]),moving=std::atoi(argv[4]),accelerated=std::atoi(argv[5]),blocks=std::atoi(argv[6]);
 Engine e;e.prepare(48000.);
#ifndef CS80_BATCH_BASELINE
 e.setFilterBatchingEnabled(accelerated!=0);
#else
 (void)accelerated;
#endif
 ControlFrame f;auto&p=f.effectiveValues;p[P_parityMode]=policy==2;p[P_sourcePerformance]=policy==1;p[P_oversampling]=0;
 p[P_volume]=.13f;p[P_chorusOn]=p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=p[P_ringAmount]=0;
 p[P_mpeOn]=1.f;p[P_subOriginalMode]=1.f;p[P_subVcf]=.12f;p[P_subVca]=.13f;f.hardwareSources={13,13};
 for(int r=0;r<2;++r){p[chParam(r,CP_hpfOn)]=float((bypass>>(r*2))&1);p[chParam(r,CP_lpfOn)]=float((bypass>>(r*2+1))&1);p[chParam(r,CP_sine)]=.15f;}
 if(!e.setControlFrame(f))return 3;
 for(int i=0;i<voices;++i)e.noteOn(36+i*5,.8f,i+2);
 std::array<float,256>audio;for(int i=0;i<80;++i)e.render(audio.data(),128);
 const int hp=int(e.getRankInfo(0,0).hpfEnabled)+int(e.getRankInfo(0,1).hpfEnabled),lp=int(e.getRankInfo(0,0).lpfEnabled)+int(e.getRankInfo(0,1).lpfEnabled);
 double sum=0.;const auto cpuStart=std::clock();const auto start=std::chrono::steady_clock::now();
 for(int b=0;b<blocks;++b){
  if(moving){e.controlChange(1,float(b%31)/31.f);e.channelPressure(2,float(b%17)/17.f);p[P_brilliance]=float(b%19)*.05f;if(!e.setControlFrame(f))return 3;}
  e.render(audio.data(),128);for(float x:audio)sum+=x;
 }
 const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const double cpu=double(std::clock()-cpuStart)/CLOCKS_PER_SEC;
 std::printf("%d,%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.17g\n",policy,voices,bypass,hp,lp,moving,accelerated,blocks,wall,cpu,sum);
}

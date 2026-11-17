#include "core/Engine.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <cmath>
using namespace cs80;
int main(int argc,char**argv){if(argc!=3)return 2;std::string mode=argv[1],dir=argv[2];for(int rate:{44100,48000,88200,96000})for(int scene=0;scene<6;++scene){auto e=std::make_unique<Engine>();
#ifdef DRAFT_CANDIDATE
 if(!e->setResearchDraft1xBeforePrepare(mode=="draft"))return 3;
#endif
 ParamValues p;p[P_sourcePerformance]=1;p[P_voiceDispersion]=0;p[P_drift]=0;p[P_analogNoise]=0;if(scene==4)p[P_resonance]=1;if(scene==5){p[P_ringAmount]=1;p[P_ringSpeed]=200;}e->setParams(p);e->setNonRealtime(mode=="hq");e->prepare(rate);int note=scene<4?36+24*scene:84;int held=rate/2,total=held+rate/4;std::vector<float>a(total*2);e->noteOn(note,0.8f);for(int n=0;n<total;){if(n==held)e->noteOff(note);int count=std::min(256,total-n);if(n<held)count=std::min(count,held-n);e->render(a.data()+n*2,count);n+=count;}for(float x:a)if(!std::isfinite(x))return 4;std::string path=dir+"/"+std::to_string(rate)+"-"+std::to_string(scene)+".f32";FILE*f=fopen(path.c_str(),"wb");if(!f)return 5;if(fwrite(a.data(),sizeof(float),a.size(),f)!=a.size())return 6;fclose(f);printf("rate=%d scene=%d factor=%d latency=%d frames=%d finite=1\n",rate,scene,e->getOversamplingFactor(),e->getLatencySamples(),total);fflush(stdout);} }

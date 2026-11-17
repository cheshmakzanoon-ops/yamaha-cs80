#include "core/Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
using namespace cs80;
int main(int argc,char**argv){
 if(argc==1){for(int n:{1,2,4,8}){OversamplingDecimator d;d.prepare(n);ExternalModulationInterpolator ext;ext.prepare(48000,n);ext.setConnected(true);float peakD=-1,peakE=-1;int indexD=-1,indexE=-1;for(int h=0;h<256;++h){auto a=ext.tick(h==0?1:0);for(int os=0;os<n;++os){d.push(h==0&&os==0?1:0);float v=std::abs(a[os]);if(v>peakE){peakE=v;indexE=h*n+os;}}float v=std::abs(d.output());if(v>peakD){peakD=v;indexD=h;}}if(indexD!=(n==1?0:63)||indexE!=OversamplingDecimator::centerFor(n))return 1;printf("factor=%d decimator_peak_hostsample=%d EXT_peak_internalsample=%d EXT_hostdelay=%.3f\n",n,indexD,indexE,ext.getHostDelay());}return 0;}
 if(argc!=3)return 2;int rate=std::atoi(argv[1]),note=std::atoi(argv[2]);auto e=std::make_unique<Engine>();if(!e->setResearchDraft1xBeforePrepare(true))return 3;ParamValues p;p[P_sourcePerformance]=1;p[P_voiceDispersion]=0;p[P_drift]=0;p[P_analogNoise]=0;p[P_resonance]=1;for(int ch=0;ch<2;++ch){int b=ch==0?P_ch1Start:P_ch2Start;p[b+CP_lpfRes]=paramDef(b+CP_lpfRes).max;p[b+CP_lpfCutoff]=12000;}e->setParams(p);e->prepare(rate);if(e->getOversamplingFactor()!=1)return 4;e->noteOn(note,1);float block[512];double max=0,sum=0;int samples=0;for(int n=0;n<rate*6;){if(n==rate*2)e->noteOff(note);int next=std::min(rate*6,((n/rate)+1)*rate);int count=std::min(256,next-n);e->render(block,count);for(int i=0;i<count*2;++i){double x=block[i];if(!std::isfinite(x))return 5;max=std::max(max,std::abs(x));sum+=x*x;++samples;}n+=count;if(n%rate==0){printf("rate=%d note=%d second=%d rms=%.12g cumulative_peak=%.12g finite=1\n",rate,note,n/rate,std::sqrt(sum/samples),max);sum=0;samples=0;fflush(stdout);}}return 0;
}

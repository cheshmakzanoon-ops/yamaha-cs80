#include "core/Engine.h"
#include <bit>
#include <cstdio>
#include <limits>
#include <memory>
#include <cmath>
using namespace cs80;
int checks=0;void check(bool x){++checks;if(!x){std::fprintf(stderr,"FAIL %d\n",checks);std::exit(1);}}
int main(){
 OversamplingDecimator d;d.prepare(1);check(d.getFactor()==1&&d.getTaps()==1&&d.getCenter()==0&&d.getHostLatency()==0&&d.getRetainedPhase()==0);
 for(float f:{0.f,-0.f,1.f,-1.f,std::numeric_limits<float>::max(),std::numeric_limits<float>::denorm_min()})check(std::bit_cast<unsigned>(d.process(f))==std::bit_cast<unsigned>(f));
 d.reset();check(d.output()==0);
 ExternalModulationInterpolator ext;ext.prepare(48000,1);check(ext.getFactor()==1&&ext.getHostDelay()==0);check(ext.tick(1)[0]==0);ext.setConnected(true);
 for(float f:{0.f,-0.f,1.f,-1.f,2.f,-2.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){auto a=ext.tick(f);check(std::bit_cast<unsigned>(a[0])==std::bit_cast<unsigned>(ext.sanitise(f)));for(int i=1;i<8;++i)check(a[i]==0);}
 ext.setConnected(false);check(ext.tick(1)[0]==0);ext.reset();check(ext.tick(1)[0]==0);
 for(double rate:{8000.,16000.,32000.,44100.,48000.,88200.,96000.,192000.})for(int policy=0;policy<3;++policy)for(int mode=0;mode<3;++mode)for(bool offline:{false,true})for(bool enabled:{false,true}){
  auto e=std::make_unique<Engine>();check(e->setResearchDraft1xBeforePrepare(enabled));ParamValues p;p[P_sourcePerformance]=policy==2;p[P_parityMode]=policy==1;p[P_oversampling]=mode;e->setParams(p);e->setNonRealtime(offline);e->prepare(rate);
  bool draft=enabled&&policy!=0&&!offline&&mode!=2&&(rate==44100||rate==48000||rate==88200||rate==96000);int n=draft?1:oversamplingFactor(oversamplingMode(mode),policy!=0,offline,rate);
  check(e->getOversamplingFactor()==n);check(e->getInternalSampleRate()==rate*n);check(e->getLatencySamples()==(draft?8:63+(policy!=0?8/n:0)));check(!e->setResearchDraft1xBeforePrepare(!enabled));check(e->getOversamplingFactor()==n);
 }
 for(int n:{1,2,4,8,1,8,1,4,1,2,1}){
  d.prepare(n);check(d.getFactor()==n);check(d.output()==0);d.process(0.75f);d.reset();check(d.output()==0);
  ext.prepare(48000,n);check(ext.getFactor()==n);check(ext.tick(1)[0]==0);ext.setConnected(true);auto a=ext.tick(1);check(std::isfinite(a[0]));ext.reset();check(ext.tick(1)[0]==0);
 }
 for(double rate:{44100.,48000.,88200.,96000.}){
  auto e=std::make_unique<Engine>();check(e->setResearchDraft1xBeforePrepare(true));ParamValues p;p[P_sourcePerformance]=1;e->setParams(p);e->prepare(rate);float buf[512]{};
  e->noteOn(60,0.8f);e->render(buf,256);check(e->activeVoices()>0);for(float x:buf)check(std::isfinite(x));
  e->setNonRealtime(true);check(e->activeVoices()==0);check(e->getOversamplingFactor()==oversamplingFactor(OversamplingMode::standard,true,true,rate));
  e->setNonRealtime(false);check(e->activeVoices()==0&&e->getOversamplingFactor()==1);
  e->noteOn(60,0.8f);e->render(buf,256);p[P_sourcePerformance]=0;e->setParams(p);check(e->activeVoices()==0&&e->getOversamplingFactor()==2);
  p[P_sourcePerformance]=1;p[P_oversampling]=2;e->setParams(p);check(e->getOversamplingFactor()>1);p[P_oversampling]=1;e->setParams(p);check(e->getOversamplingFactor()==1&&e->activeVoices()==0);
  e->prepare(rate);check(e->activeVoices()==0&&e->getOversamplingFactor()==1);
 }
 std::printf("PASS %d structural checks; bounded render, no cost measurement\n",checks);
}

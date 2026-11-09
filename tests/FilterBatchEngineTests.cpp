// Bitwise scheduling/continuation checks, not hardware-model fidelity claims.
#include "core/Engine.h"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string_view>
#include <vector>
using namespace cs80;
namespace {
std::uint64_t checks=0,framesCompared=0;int failures=0,currentCase=0;
std::FILE* corpus=nullptr;
void enable(Engine& e,bool value){
#ifndef CS80_BATCH_BASELINE
 e.setFilterBatchingEnabled(value);
#else
 (void)e;(void)value;
#endif
}
bool enabled(const Engine& e){
#ifndef CS80_BATCH_BASELINE
 return e.hasFilterBatchingKernel();
#else
 (void)e;return false;
#endif
}
void check(bool ok,const char* message){++checks;if(!ok){if(failures<16)std::fprintf(stderr,"FAIL case %d: %s\n",currentCase,message);++failures;}}
template<class T>void same(T a,T b,const char* label){
 if(corpus&&std::fwrite(&a,sizeof(a),1,corpus)!=1){std::perror("corpus write");std::exit(2);}
 if constexpr(sizeof(T)==4)check(std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b),label);
 else check(std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b),label);
}
void state(Ig00156Stage::State a,Ig00156Stage::State b){same(a.input,b.input,"input history");same(a.feedback,b.feedback,"feedback history");same(a.bp,b.bp,"BP history");same(a.lp,b.lp,"LP history");}
void observe(const Engine&a,const Engine&b){
 check(a.activeVoices()==b.activeVoices(),"active voice count");check(a.getLatencySamples()==b.getLatencySamples(),"latency");
 same(a.getRingFrequency(),b.getRingFrequency(),"ring frequency");same(a.getRingEnvelope(),b.getRingEnvelope(),"ring envelope");
 const auto x=a.getListeningSample(),y=b.getListeningSample();same(x.left,y.left,"internal output L");same(x.right,y.right,"internal output R");
 for(int v=0;v<8;++v){
  const auto x=a.getVoiceInfo(v),y=b.getVoiceInfo(v);
  check(x.active==y.active&&x.held==y.held&&x.pedalHeld==y.pedalHeld&&x.note==y.note&&x.channel==y.channel,"voice ownership");
  same(x.velocity,y.velocity,"velocity");same(x.pressure,y.pressure,"pressure");same(x.bend,y.bend,"bend");same(x.ribbonRatio,y.ribbonRatio,"ribbon");same(x.touchA,y.touchA,"touch A");same(x.touchB,y.touchB,"touch B");same(x.touchC,y.touchC,"touch C");
  for(int r=0;r<2;++r){
   const auto x=a.getRankAudioState(v,r),y=b.getRankAudioState(v,r);same(x.phase,y.phase,"osc phase");state(x.hpCircuit,y.hpCircuit);state(x.lpCircuit,y.lpCircuit);
   same(x.hpf.ic1,y.hpf.ic1,"HP ic1");same(x.hpf.ic2,y.hpf.ic2,"HP ic2");same(x.lpf.ic1,y.lpf.ic1,"LP ic1");same(x.lpf.ic2,y.lpf.ic2,"LP ic2");
   const auto p=a.getRankInfo(v,r),q=b.getRankInfo(v,r);
   const float pf[]{p.hpfHz,p.lpfHz,p.filterEnvelope,p.ampEnvelope,p.oscillatorHz,p.keyVolts,p.hpfQ,p.lpfQ};
   const float qf[]{q.hpfHz,q.lpfHz,q.filterEnvelope,q.ampEnvelope,q.oscillatorHz,q.keyVolts,q.hpfQ,q.lpfQ};for(int j=0;j<8;++j)same(pf[j],qf[j],"rank info");
   check(p.hpfEnabled==q.hpfEnabled&&p.lpfEnabled==q.lpfEnabled,"bypass flags");
   const auto e=a.getRankEnvelopeState(v,r),f=b.getRankEnvelopeState(v,r);
   check(e.amplitude.stage==f.amplitude.stage&&e.filter.stage==f.filter.stage,"envelope stages");
   same(e.amplitude.value,f.amplitude.value,"amp value");same(e.amplitude.elapsed,f.amplitude.elapsed,"amp elapsed");same(e.filter.value,f.filter.value,"filter value");same(e.filter.elapsed,f.filter.elapsed,"filter elapsed");
   const auto& o=a.getRankOscillatorState(v,r);const auto& z=b.getRankOscillatorState(v,r);
   const double of[]{o.charge,o.resetRemainingSeconds,o.inputBaselineVolts,o.triangleBaselineVolts,o.heldFrequencyHz,o.heldThresholdVolts,o.elapsedSeconds};
   const double zf[]{z.charge,z.resetRemainingSeconds,z.inputBaselineVolts,z.triangleBaselineVolts,z.heldFrequencyHz,z.heldThresholdVolts,z.elapsedSeconds};for(int j=0;j<7;++j)same(of[j],zf[j],"WSC state");
   check(o.residualCursor==z.residualCursor&&o.resetCount==z.resetCount&&o.sawEnabled==z.sawEnabled&&o.pulseEnabled==z.pulseEnabled,"WSC metadata");
   for(size_t row=0;row<o.residual.size();++row)for(size_t i=0;i<o.residual[row].size();++i)same(o.residual[row][i],z.residual[row][i],"WSC residual");
  }
 }
}
void renderPair(Engine&a,Engine&b,int n,int block=0){
 std::vector<float>x(size_t(n)*2),y(size_t(n)*2),ext(size_t(n),0.f);
 for(int i=0;i<n;++i)ext[size_t(i)]=0.25f*std::sin(float(i+block*127)*.13f);
 a.render(x.data(),n,ext.data());b.render(y.data(),n,ext.data());
 for(size_t i=0;i<x.size();++i){same(x[i],y[i],"host output");check(std::isfinite(x[i]),"finite output");}
 framesCompared+=std::uint64_t(n);observe(a,b);
}
ControlFrame fixture(int policy,int quality,int bypass,int selector){
 ControlFrame f;auto&p=f.effectiveValues;p[P_parityMode]=policy==2;p[P_sourcePerformance]=policy==1;p[P_oversampling]=float(quality);
 p[P_chorusOn]=0.f;p[P_drift]=.18f;p[P_voiceDispersion]=.32f;p[P_analogNoise]=.03f;p[P_volume]=.13f;
 p[P_mpeOn]=1.f;p[P_subOriginalMode]=1.f;p[P_subVcf]=.12f;p[P_subVca]=.13f;p[P_subExtLevel]=.1f;p[P_ringAmount]=.17f;
 f.hardwareSources={selector,(selector+5)%14};
 for(int r=0;r<2;++r){p[chParam(r,CP_hpfOn)]=float((bypass>>(r*2))&1);p[chParam(r,CP_lpfOn)]=float((bypass>>(r*2+1))&1);p[chParam(r,CP_sine)]=.15f;}
 return f;
}
void scenario(double rate,int policy,int quality,int bypass,int selector){
 ++currentCase;Engine a,b;a.prepare(rate);b.prepare(rate);enable(a,true);enable(b,false);check(!enabled(b),"forced scalar setup");
 auto f=fixture(policy,quality,bypass,selector);check(a.setControlFrame(f)&&b.setControlFrame(f),"frame accepted");
 auto each=[&](auto fn){fn(a);fn(b);};
 renderPair(a,b,257); // cold idle snapshots/replay before first note
 for(int block=0;block<60;++block){
  if(block<8)each([&](Engine&e){e.noteOn(43+block*4,.5f+.05f*float(block),block+2);});
  if(block==10)each([](Engine&e){e.polyPressure(43,.8f,2);e.pitchBend(3,.31f);e.setRibbon(.1f);e.controlChange(1,.65f);});
  if(block==15){f.effectiveValues[P_brilliance]=.6f;f.effectiveValues[P_resonance]=.65f;f.effectiveValues[P_voiceDispersion]=.7f;check(a.setControlFrame(f)&&b.setControlFrame(f),"moving frame");}
  if(block==20)each([](Engine&e){e.controlChange(64,1.f);for(int i=0;i<8;++i)e.noteOff(43+i*4,i+2);});
  if(block==23)each([](Engine&e){e.controlChange(64,0.f);});
  if(block==25)each([](Engine&e){e.allNotesOff(false);});
  if(block==29)for(int i=0;i<7;++i)each([&](Engine&e){e.noteOn(52+i*3,.8f,i+2);});
  if(block==33){enable(a,false);} // live switch preserves state
  if(block==35){enable(a,true);}
  if(block==38){f.hardwareSources={13,13};f.effectiveValues[P_kasScanTiming]=1.f;check(a.setControlFrame(f)&&b.setControlFrame(f),"panel and scan transition");}
  if(block==40)each([](Engine&e){e.allNotesOff(false);e.noteOnFromKeyboard(61,.7f,2);e.noteOnFromKeyboard(66,.8f,3);});
  if(block==44)each([](Engine&e){e.setNonRealtime(true);e.noteOn(65,.8f,2);e.noteOn(69,.7f,3);});
  if(block==48)each([](Engine&e){e.setNonRealtime(false);e.reset();e.noteOn(58,.6f,2);e.noteOn(62,.9f,3);});
  if(block==53){f.effectiveValues[P_oversampling]=float((quality+1)%3);check(a.setControlFrame(f)&&b.setControlFrame(f),"quality switch");each([](Engine&e){for(int i=0;i<5;++i)e.noteOn(40+i*7,.7f,i+2);});}
  if(block==57){each([&](Engine&e){e.prepare(rate==48000.?44100.:48000.);e.noteOn(60,.7f,2);e.noteOn(72,.8f,3);});}
  renderPair(a,b,std::array{1,7,64,129}[size_t(block)%4],block);
 }
 each([](Engine&e){e.allNotesOff(false);});renderPair(a,b,4096);each([](Engine&e){e.noteOn(60,.8f,2);e.noteOn(67,.6f,3);e.noteOn(74,.7f,4);});renderPair(a,b,1024);
}
void benchmark(){
 std::puts("policy,voices,bypass,effective_hp_ranks,effective_lp_ranks,moving,repeat,batch,wall_seconds,cpu_seconds,checksum");
 for(int policy:{0,1,2})for(int voices:{0,1,2,4,8})for(int bypass:{0,5,10,15})for(int moving:{0,1})for(int repeat=0;repeat<5;++repeat)for(int order=0;order<2;++order){
  const bool batch=(repeat%2)==order;Engine e;e.prepare(48000);enable(e,batch);auto f=fixture(policy,0,bypass,13);f.effectiveValues[P_voiceDispersion]=f.effectiveValues[P_drift]=f.effectiveValues[P_analogNoise]=0;f.effectiveValues[P_ringAmount]=0;check(e.setControlFrame(f),"benchmark frame");
  for(int i=0;i<voices;++i)e.noteOn(36+i*5,.8f,i+2);
  std::array<float,256>audio;for(int i=0;i<80;++i)e.render(audio.data(),128);
  const int hpRanks=int(e.getRankInfo(0,0).hpfEnabled)+int(e.getRankInfo(0,1).hpfEnabled),lpRanks=int(e.getRankInfo(0,0).lpfEnabled)+int(e.getRankInfo(0,1).lpfEnabled);
  double sum=0.;const auto cpuStart=std::clock();auto start=std::chrono::steady_clock::now();
  for(int block=0;block<150;++block){if(moving){e.controlChange(1,float(block%31)/31.f);e.channelPressure(2,float(block%17)/17.f);f.effectiveValues[P_brilliance]=float(block%19)*.05f;e.setControlFrame(f);}e.render(audio.data(),128);for(float x:audio)sum+=x;}
  const double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();const double cpu=double(std::clock()-cpuStart)/CLOCKS_PER_SEC;
  std::printf("%d,%d,%d,%d,%d,%d,%d,%d,%.9f,%.9f,%.17g\n",policy,voices,bypass,hpRanks,lpRanks,moving,repeat,batch,sec,cpu,sum);
 }
}
}
int main(int argc,char**argv){
 if(argc>1&&std::string_view(argv[1])=="--benchmark"){benchmark();return failures?1:0;}
 if(argc==3&&std::string_view(argv[1])=="--write-corpus"){corpus=std::fopen(argv[2],"wb");if(!corpus){std::perror("corpus open");return 2;}}
 int i=0;for(double rate:{8000.,44100.,48000.,96000.,192000.,768000.})for(int policy:{0,1,2}){scenario(rate,policy,i%3,i%16,i%14);++i;}
 for(int bypass=0;bypass<16;++bypass)scenario(48000.,1,0,bypass,13);
 if(corpus&&std::fclose(corpus)!=0){std::perror("corpus close");return 2;}
 std::printf("FilterBatchEngine: %llu checks, %llu stereo frames, %d failures\n",(unsigned long long)checks,(unsigned long long)framesCompared,failures);return failures?1:0;
}

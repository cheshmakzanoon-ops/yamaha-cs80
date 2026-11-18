// Independent scheduling/partition corpus; not a hardware-fidelity test.
#include "core/Engine.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
using namespace cs80;
namespace {
std::uint64_t checks=0,frames=0;int failures=0;bool routed=false;
void check(bool ok,const char* label){++checks;if(!ok){if(failures<16)std::fprintf(stderr,"FAIL %s\n",label);++failures;}}
template<class T>void same(T a,T b,const char* label){
 if constexpr(sizeof(T)==4)check(std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b),label);
 else check(std::bit_cast<std::uint64_t>(a)==std::bit_cast<std::uint64_t>(b),label);
}
void observe(const Engine& a,const Engine& b){
 check(a.activeVoices()==b.activeVoices(),"active voice count");
 for(int v=0;v<8;++v){
  const auto x=a.getVoiceInfo(v),y=b.getVoiceInfo(v);
  check(x.active==y.active&&x.held==y.held&&x.note==y.note,"voice allocation/held state");
  for(int r=0;r<2;++r){
   const auto x=a.getRankAudioState(v,r),y=b.getRankAudioState(v,r);
   same(x.phase,y.phase,"oscillator phase");
   for(auto pair:{std::pair{x.hpCircuit,y.hpCircuit},std::pair{x.lpCircuit,y.lpCircuit}}){
    same(pair.first.input,pair.second.input,"input capacitor");
    same(pair.first.feedback,pair.second.feedback,"feedback capacitor");
    same(pair.first.bp,pair.second.bp,"BP capacitor");same(pair.first.lp,pair.second.lp,"LP capacitor");
   }
   const auto e=a.getRankEnvelopeState(v,r),f=b.getRankEnvelopeState(v,r);
   check(e.amplitude.stage==f.amplitude.stage&&e.filter.stage==f.filter.stage,"envelope stage");
   same(e.amplitude.value,f.amplitude.value,"amp envelope value");same(e.filter.value,f.filter.value,"filter envelope value");
   same(e.amplitude.elapsed,f.amplitude.elapsed,"amp envelope elapsed");same(e.filter.elapsed,f.filter.elapsed,"filter envelope elapsed");
   const auto m=a.getModernRankOffsets(v,r),n=b.getModernRankOffsets(v,r);
   const float p[]{m.pitch,m.pulseWidth,m.hpCutoff,m.lpCutoff,m.hpQ,m.lpQ,m.level,m.pan};
   const float q[]{n.pitch,n.pulseWidth,n.hpCutoff,n.lpCutoff,n.hpQ,n.lpQ,n.level,n.pan};
   for(int k=0;k<8;++k)same(p[k],q[k],"modern rank routing");
   routed |= m.hpCutoff!=0 || m.lpCutoff!=0 || m.level!=0;
  }
  for(int i=0;i<3;++i)same(a.getModernMsegValue(v,i),b.getModernMsegValue(v,i),"MSEG runner");
 }
 const auto g=a.getModernGlobalOffsets(),h=b.getModernGlobalOffsets();
 same(g.subSpeedRatio,h.subSpeedRatio,"global SUB speed route");
 same(g.subDepthGain,h.subDepthGain,"global SUB depth route");
 for(int i=0;i<3;++i){same(g.fxMix[i],h.fxMix[i],"global FX route");same(a.getModernMsegDisplayPosition(i),b.getModernMsegDisplayPosition(i),"MSEG display clock");}
}
ControlFrame frame(){
 ControlFrame f;auto&p=f.effectiveValues;
 p[P_sourcePerformance]=1;p[P_parityMode]=0;p[P_oversampling]=0;p[P_chorusOn]=0;p[P_volume]=.1f;
 p[P_drift]=.2f;p[P_voiceDispersion]=.4f;p[P_analogNoise]=.03f;p[P_keyRelease]=0;p[P_masterSustain]=0;
 p[P_ringAmount]=.15f;p[P_subVcf]=.12f;p[P_subVca]=.1f;
 f.hardwareSources={13,13};
 for(int r=0;r<2;++r){p[chParam(r,CP_vA)]=.003f;p[chParam(r,CP_vD)]=.01f;p[chParam(r,CP_vS)]=.4f;p[chParam(r,CP_vR)]=.008f;p[chParam(r,CP_fR)]=.008f;p[chParam(r,CP_noise)]=.05f;}
 for(int i=0;i<6;++i)p[static_cast<size_t>(P_mod1Amount+i)]=(i%2?-.13f:.17f);
 p[P_mseg1Rate]=.5f;p[P_mseg2Rate]=-.3f;return f;
}
modern::ModernConfig config(){
 using namespace modern;ModernConfig c;
 c.slots[0]={ModSource::ampEnvelope,ModSource::none,ModDestination::lpCutoff,ModTarget::rankII};
 c.slots[1]={ModSource::filterEnvelope,ModSource::ampEnvelope,ModDestination::mseg1Rate,ModTarget::rankI};
 c.slots[2]={ModSource::mseg1,ModSource::none,ModDestination::hpCutoff,ModTarget::rankI};
 c.slots[3]={ModSource::mseg2,ModSource::filterEnvelope,ModDestination::level,ModTarget::rankII};
 c.slots[4]={ModSource::ampEnvelope,ModSource::none,ModDestination::subDepth,ModTarget::both};
 c.slots[5]={ModSource::filterEnvelope,ModSource::none,ModDestination::pan,ModTarget::both};
 for(int i=0;i<3;++i){auto&s=c.msegs[i];s.count=3;s.points[0]={0,0,.2f};s.points[1]={.02f,1,-.4f};s.points[2]={.07f,.1f,0};s.loopStart=0;s.loopEnd=2;s.loop=MsegLoop::always;s.trigger=static_cast<MsegTrigger>(i);}
 return c;
}
void scenario(double rate){
 std::array<std::unique_ptr<Engine>,4> e;auto f=frame();
 for(int i=0;i<4;++i){e[i]=std::make_unique<Engine>();e[i]->prepare(rate);e[i]->setFilterBatchingEnabled(i%2==1);e[i]->setModernConfig(config());check(e[i]->setControlFrame(f),"frame accepted");}
 int timeline=0;
 auto each=[&](auto fn){for(auto&x:e)fn(*x);};
 auto render=[&](int count){
  std::array<std::vector<float>,4> audio;std::vector<float> ext(count);
  for(int n=0;n<count;++n)ext[n]=.15f*std::sin(float(timeline+n)*.019f);
  for(int i=0;i<4;++i){
   audio[i].resize(static_cast<size_t>(count)*2);
   if(i<2)e[i]->render(audio[i].data(),count,ext.data());
   else{int done=0,k=0;while(done<count){const int n=std::min(count-done,std::array{1,7,64,997}[size_t(k++)%4]);e[i]->render(audio[i].data()+2*done,n,ext.data()+done);done+=n;}}
  }
  for(int i=1;i<4;++i){for(size_t n=0;n<audio[0].size();++n)same(audio[0][n],audio[i][n],"scalar/SIMD/block-partition host output");observe(*e[0],*e[i]);}
  frames+=count;timeline+=count;
 };
 render(1024);each([](Engine&x){x.noteOn(60,.8f);});render(317);
 check(e[0]->activeVoices()==1,"one active voice");
 each([](Engine&x){x.noteOn(67,.6f);});render(991);
 check(e[0]->activeVoices()==2,"two active voices trigger batching boundary");
 each([](Engine&x){x.noteOff(67);});render(8192);
 check(e[0]->activeVoices()==1,"natural release returns to one voice");
 for(int note:{48,55,62,69,76})each([&](Engine&x){x.noteOn(note,.7f);});
 render(777);check(e[0]->isModernLayerActive(),"matrix/MSEG actually enabled");
 for(int bypass=0;bypass<16;++bypass){
  for(int r=0;r<2;++r){f.effectiveValues[chParam(r,CP_hpfOn)]=float((bypass>>(2*r))&1);f.effectiveValues[chParam(r,CP_lpfOn)]=float((bypass>>(2*r+1))&1);}
  each([&](Engine&x){check(x.setControlFrame(f),"bypass change");x.controlChange(1,float(bypass)/16);});
  render(257+bypass);
 }
 for(int note:{48,55,60,62,67,69,76})each([&](Engine&x){x.noteOff(note);});
 render(static_cast<int>(rate));check(e[0]->activeVoices()==0,"natural death to idle without panic");
 render(4096);each([](Engine&x){x.noteOn(61,.8f);x.noteOn(73,.6f);});render(2048);
 check(e[0]->activeVoices()==2,"wake/replay returns to two voices");
}
}
int main(){scenario(44100);scenario(48000);check(routed,"nonidentity envelope/MSEG routes exercised");
 std::printf("FilterBatchScheduling: %llu checks, %d failures, %llu timeline frames (four engine variants)\n",
 static_cast<unsigned long long>(checks),failures,static_cast<unsigned long long>(frames));return failures?1:0;}

#include <bits/stdc++.h>
#include <xmmintrin.h>
#define private public
#include "core/Engine.h"
#undef private
#include "core/FactoryPresets.h"
using namespace cs80;
struct Case{bool isolated=false,sourcePerformance=true,noise=false,moving=false;int performanceProfile=0;};
#include "frame.inc"
std::uint64_t counts[3]{};
extern "C" void shared_cache_observe(unsigned n){if(n<3)++counts[n];else std::abort();}
struct Out{FILE*f;template<class T>void pod(const T&x){static_assert(std::is_trivially_copyable_v<T>);if constexpr(std::is_array_v<T>){for(const auto&part:x)pod(part);}else{T copy=x;__builtin_clear_padding(&copy);if(std::fwrite(&copy,sizeof(copy),1,f)!=1)throw std::runtime_error("write");}}template<class...T>void all(const T&...x){(pod(x),...);}};
void fp(Out&o){unsigned csr;unsigned short cw,sw;asm volatile("stmxcsr %0":"=m"(csr)::"memory");asm volatile("fnstcw %0":"=m"(cw));asm volatile("fnstsw %0":"=am"(sw));const int err=errno;o.all(csr,cw,sw,err);}
void state(Out&o,const Engine&e){
 o.all(e.params,e.global,e.rankSettings,e.hardwareSources,e.sourceRoutes,e.sourceActive,e.sourceBoundaries,e.sourceCutoff,e.sourceShared,e.sourceFilterValues,e.sourceCompileCount,e.controlFrameStatus);
 o.all(e.voiceCounter,e.noteIngressOrder,e.nextVoice,e.lastNote,e.globalBend,e.globalPitchWheel,e.modWheel,e.lastChannelPressure,e.expression,e.ribbon,e.hostRate,e.internalRate,e.initialPitchDecay,e.oversample,e.idleStepSamples,e.idleTransientDecay,e.idleNoise,e.touchCoefficients,e.trg5BusVolts,e.trg5FullScaleVolts,e.toleranceScaleApplied,e.refreshedContourLevers,e.prepared,e.nonRealtime,e.settingsSmoothing,e.controllerSmoothing,e.controlSmoothing,e.subSpeedSmoothing);
 o.all(e.pwmPhase,e.subLfo,e.originalSub,e.originalSubSpeedSmoothed,e.originalSubSmoothing,e.modLfos,e.subValue,e.lfoValues,e.lfoPitch,e.lfoFilter,e.lfoAmp,e.lfoPwm,e.lfoPan,e.subSpeedSmoothed,e.glideExp);
 o.all(e.ringPhase,e.ringEnv,e.ringFrequency,e.ringGate,e.anyVoiceWasActive,e.audioRng,e.modRng,e.dcL,e.dcR,e.dcInL,e.dcInR,e.dcCoefficient,e.smoothedVolume,e.ringAdsr,e.ringKeyDown,e.heldCount,e.sustainPedals,e.driftPhase,e.voiceDrift,e.heldKeys,e.lastPitch,e.mpeZones,e.memberExpression,e.managerPressure,e.managerTimbre,e.managerBend,e.upperZoneBend,e.scanner,e.unassignedKeys,e.unassignedCount,e.heldKeyCount,e.keyOrder,e.arpSequence,e.arpSequenceLength,e.arpIndex,e.arpDirection,e.arpVoice,e.arpCurrentNote,e.arpSourceOrder,e.arpGateOpen,e.arpWasOn,e.arpClock,e.arpGateTime);
 for(const auto&v:e.voices){
  o.all(v.active,v.held,v.sustained,v.pedalHeld,v.note,v.midiChannel,v.velocity,v.pressure,v.notePressureRaw,v.noteTimbreRaw,v.timbre,v.mpeBend,v.glidePitch,v.targetPitch,v.initialPitchTransient,v.ribbonRatio,v.age,v.releaseGroup,v.coordinatedReleaseOrder,v.keyboardOrigin,v.contour,v.idleClock,v.idleSteps,v.envelopeReset,v.keyNode,v.kbcKey,v.kbcCv,v.kbcLevelGain,v.touch,v.touchA,v.touchB,v.touchC,v.upperZone,v.unisonOffset);
  for(const auto&r:v.ranks){
   o.all(r.osc,r.hpf,r.lpf,r.hpStage,r.lpStage,r.vca,r.vcaCold,r.fenv,r.aenv,r.trimTune,r.trimCutoff,r.trimLevel,r.trimEnv,r.trimPan,r.info,r.hpCutoffExp,r.lpCutoffExp,r.keyVoltsExp,r.idle,r.idleRefs,r.idleHistory,r.idleLead,r.idleFirstSamples,r.tolDraws,r.tol,r.thermal,r.driftCents);
   const auto&w=r.wsc;o.all(w.endNodes,w.endNodesValid,w.negligibleCornerScale,w.profile,w.state,w.diagnostics,w.rate,w.dt,w.converterGain,w.rampHalf);
  }
 }
}
void mode(unsigned n){unsigned short cw=static_cast<unsigned short>(0x37fu|(((n/4)%4)<<10));asm volatile("fnclex\n\tfldcw %0"::"m"(cw):"memory");_mm_setcsr(0x1f80u|(((n/4)%4)<<13)|((n&1)?0x8000u:0u)|((n&2)?0x40u:0u)|(n%3?0x20u:0));errno=91;}
int main(int argc,char**argv){if(argc!=2)return 2;const char*names[]={"quiet","held","moving","release_wake","panel","pure","factory4","factory7","factory10","mixed","transitions","resonance","one_sample","legacy"};const int notes[]={36,43,48,55,60,64,67,72};
 for(int scene=0;scene<14;scene++){
  mode(3);Case c;c.isolated=scene==5;c.sourcePerformance=scene!=5&&scene!=13;c.performanceProfile=scene==2?1:0;int selector=scene==4?13:scene==6?4:scene==7?7:scene==8?10:0;auto frame=makeFrame(c,selector);if(scene==9)frame.hardwareSources[1]=13;
  frame.effectiveValues[P_voiceDispersion]=.18f;frame.effectiveValues[P_drift]=.37f;
  Engine e;e.prepare(48000);if(!e.setControlFrame(frame))throw std::runtime_error("frame");e.reset();if(scene!=0)for(int i=0;i<8;i++)e.noteOn(notes[i],.8f,scene==2?i+2:1);
  std::string path=std::string(argv[1])+"-"+names[scene]+".bin";Out o{std::fopen(path.c_str(),"wb")};if(!o.f)throw std::runtime_error("open");
  const int block=scene==12?1:64,blocks=scene==12?1024:256;std::array<float,128>pcm{};std::fill(std::begin(counts),std::end(counts),0);
  for(int b=-64;b<blocks;b++){
   if(b==0)std::fill(std::begin(counts),std::end(counts),0);
   mode(scene==10?unsigned(std::max(0,b)/16)%16:3);
   if(b==64||b==192){if(!e.setControlFrame(frame))throw std::runtime_error("repeat");}
   if(b==97){auto bad=frame;bad.modelVersion++;if(e.setControlFrame(bad))throw std::runtime_error("bad accepted");}
   if(scene==2&&b>=0&&b%4==0){int i=(b/4)%8;float t=float((b*13)%97)/96.f;e.channelPressure(i+2,t);e.pitchBend(i+2,t*.1f);e.controlChange(74,t,i+2);}
   if(scene==3&&b==24)for(int n:notes)e.noteOff(n);
   if(scene==3&&b==180)for(int n:notes)e.noteOn(n,.72f);
   if(scene==11&&b>=0&&b%4==0){frame.effectiveValues[P_resonance]=float(b%101)/100.f;if(!e.setControlFrame(frame))throw std::runtime_error("resonance");}
   if(scene==10){
    if(b==20)e.allNotesOff(false);
    if(b==32)e.reset();
    if(b==48){frame.effectiveValues[P_parityMode]=1;if(!e.setControlFrame(frame))throw std::runtime_error("Pure");}
    if(b==80){frame.effectiveValues[P_parityMode]=0;frame.effectiveValues[P_sourcePerformance]=0;if(!e.setControlFrame(frame))throw std::runtime_error("Legacy");}
    if(b==112){frame.effectiveValues[P_sourcePerformance]=1;if(!e.setControlFrame(frame))throw std::runtime_error("Source");for(int n:notes)e.noteOn(n,.62f);}
    if(b==128){e.prepare(44100);if(!e.setControlFrame(frame))throw std::runtime_error("rate");for(int n:notes)e.noteOn(n,.69f);}
    if(b==160){Engine copy=e;e=copy;}
   }
   const bool snapshot=(b%16==0||b==20||b==24||b==48||b==80||b==97||b==112||b==128||b==160||b==180||b==192);
   if(b>=0){o.pod(b);fp(o);o.pod(e.sourceCutoff);o.pod(e.sourceShared);o.pod(snapshot);if(snapshot)state(o,e);} // zero-frame event-visible state
   mode(scene==10?unsigned(std::max(0,b)/16)%16:3);e.render(pcm.data(),block);
   if(b>=0){fp(o);for(int i=0;i<block*2;i++){if(!std::isfinite(pcm[i]))throw std::runtime_error("nonfinite");o.pod(pcm[i]);}o.pod(e.sourceCutoff);o.pod(e.sourceShared);if(snapshot)state(o,e);}
  }
  if(std::fclose(o.f))throw std::runtime_error("close");
  std::printf("%s,%llu,%llu,%llu\n",names[scene],(unsigned long long)counts[0],(unsigned long long)counts[1],(unsigned long long)counts[2]);
 }
}

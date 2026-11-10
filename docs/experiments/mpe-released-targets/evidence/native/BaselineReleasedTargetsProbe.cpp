#include "plugin/PluginProcessor.h"
#include <cmath>
#include <cstdio>
using namespace cs80;
namespace {
int checks=0, failures=0, fixtureFailures=0, retentionFailures=0;
void check(bool condition, const char* label, bool retention=false) {
 ++checks;
 if (!condition) { ++failures; retention ? ++retentionFailures : ++fixtureFailures; }
 std::printf("  %s: %s\n", condition ? "PASS" : "FAIL", label);
}
void zone(juce::MidiBuffer& m,int manager,int members) {
 m.addEvent(juce::MidiMessage::controllerEvent(manager,101,0),0);
 m.addEvent(juce::MidiMessage::controllerEvent(manager,100,6),0);
 m.addEvent(juce::MidiMessage::controllerEvent(manager,6,members),0);
}
void expression(juce::MidiBuffer& m,int member,bool later) {
 m.addEvent(juce::MidiMessage::pitchWheel(member,later?6144:10240),0);
 m.addEvent(juce::MidiMessage::channelPressureChange(member,later?23:89),0);
 m.addEvent(juce::MidiMessage::controllerEvent(member,74,later?17:101),0);
}
}
int main() {
 juce::ScopedJuceInitialiser_GUI init;
 juce::TemporaryFile fixture("-mpe-released-target-baseline");
 int scenarios=0;
 for(bool circuit:{false,true}) for(int eventFrames:{0,32}) for(int member:{2,15}) for(bool pedal:{false,true}) {
  ++scenarios;
  std::printf("SCENARIO %d: %s callback=%d member=%d pedal=%d\n",scenarios,circuit?"Circuit":"Legacy",eventFrames,member,(int)pedal);
  Cs80AudioProcessor p(fixture.getFile());
  p.setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
  p.setParamValue(P_parityMode,0); p.setParamValue(P_voiceMode,0); p.setParamValue(P_mpeOn,1);
  p.setParamValue(P_kasScanTiming,0); p.setParamValue(P_arpOn,0);
  p.setParamValue(P_analogNoise,0); p.setParamValue(P_voiceDispersion,0); p.setParamValue(P_drift,0);
  p.setParamValue(P_pressureCurve,0); p.setParamValue(P_pressureThreshold,0);
  p.setParamValue(P_mpeBendRange,48); p.setParamValue(P_keyRelease,2);
  p.setParamValue(P_mod1Amount,1);
  p.getModernLayer().setSlot(0,{modern::ModSource::timbre,modern::ModSource::none,modern::ModDestination::pulseWidth,modern::ModTarget::both});
  p.prepareToPlay(48000,64);
  juce::AudioBuffer<float> eventAudio(2,eventFrames), rendered(2,64);
  juce::MidiBuffer midi;
  auto event=[&] { p.processBlock(eventAudio,midi); midi.clear(); };
  auto render=[&] {
   bool finite=true,sounded=false;
   for(int i=0;i<4;++i) { p.processBlock(rendered,midi); midi.clear();
    for(int c=0;c<2;++c) for(int f=0;f<64;++f) {const auto x=rendered.getSample(c,f);finite &= std::isfinite(x); sounded |= x!=0.0f;}
   }
   return finite && sounded;
  };
  event(); zone(midi,1,0); zone(midi,16,0); zone(midi,1,2); zone(midi,16,2); event();
  const auto role=p.getEngine().getMpeZones().role(member);
  check(role==(member==2?MpeZoneLayout::Role::lowerMember:MpeZoneLayout::Role::upperMember),"negotiated current member role");
  midi.addEvent(juce::MidiMessage::noteOn(member,60,(juce::uint8)100),0); expression(midi,member,false); event();
  check(render(),"initial continuation is finite and non-silent");
  int vi=-1;
  for(int i=0;i<8;++i) {auto v=p.getEngine().getVoiceInfo(i); if(v.active&&v.held&&v.note==60&&v.channel==member)vi=i;}
  check(vi>=0,"initial voice active, physically held, correct note and member");
  if(vi<0) continue;
  const auto initial=p.getEngine().getVoiceInfo(vi);
  check(initial.bend==12.0f,"initial bend is 12 semitones");
  check(initial.pressure==89.0f/127.0f,"initial pressure is 89/127");
  const auto initialTimbre0=p.getEngine().getModernRankOffsets(vi,0).pulseWidth;
  const auto initialTimbre1=p.getEngine().getModernRankOffsets(vi,1).pulseWidth;
  check(initialTimbre0==(101.0f/127.0f)*modern::kPulseWidthRange,"initial rank 0 voice timbre-to-pulseWidth");
  check(initialTimbre1==(101.0f/127.0f)*modern::kPulseWidthRange,"initial rank 1 voice timbre-to-pulseWidth");
  if(pedal) midi.addEvent(juce::MidiMessage::controllerEvent(member,64,127),0);
  midi.addEvent(juce::MidiMessage::noteOff(member,60),0); event();
  const auto released=p.getEngine().getVoiceInfo(vi);
  check(released.active&&!released.held&&released.note==60&&released.channel==member,"accepted release leaves active, unheld voice on original note/channel");
  check(released.pedalHeld==pedal,"accepted release has intended pedal-held state");
  expression(midi,member,true); event();
  check(render(),"post-release continuation is finite and non-silent");
  const auto after=p.getEngine().getVoiceInfo(vi);
  check(after.active&&!after.held&&after.note==60&&after.channel==member,"retention witness remains active, unheld and correctly attributed");
  check(after.pedalHeld==pedal,"retention witness preserves intended pedal-held state");
  check(after.bend==initial.bend,"released bend target retains old value",true);
  check(after.pressure==initial.pressure,"released pressure target retains old value",true);
  const auto timbre0=p.getEngine().getModernRankOffsets(vi,0).pulseWidth;
  const auto timbre1=p.getEngine().getModernRankOffsets(vi,1).pulseWidth;
  check(timbre0==initialTimbre0,"released rank 0 voice timbre target retains old value",true);
  check(timbre1==initialTimbre1,"released rank 1 voice timbre target retains old value",true);
  std::printf("  OBSERVED bend=%g (old=%g), pressure=%g (old=%g), timbrePW=%g/%g (old=%g/%g)\n",after.bend,initial.bend,after.pressure,initial.pressure,timbre0,timbre1,initialTimbre0,initialTimbre1);
 }
 std::printf("MPE released-target baseline: %d scenarios, %d checks, %d failures (%d fixture, %d retention)\n",scenarios,checks,failures,fixtureFailures,retentionFailures);
 return failures?1:0;
}

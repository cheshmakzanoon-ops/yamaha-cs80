#include "plugin/PluginProcessor.h"
#include <cstdio>
using namespace cs80;
int held(const Engine& e,int note) { int n=0; for(int i=0;i<8;++i) { auto v=e.getVoiceInfo(i); n+=v.active&&v.held&&v.note==note&&v.channel==2; } return n; }
int main() {
 juce::ScopedJuceInitialiser_GUI init; juce::TemporaryFile fixture("-release-probe"); int failures=0;
 for(bool circuit:{false,true}) for(int frames:{0,32}) {
  Cs80AudioProcessor p(fixture.getFile()); p.setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
  p.setParamValue(P_parityMode,0);p.setParamValue(P_voiceMode,1);p.setParamValue(P_kasScanTiming,0);p.setParamValue(P_arpOn,0);p.setParamValue(P_analogNoise,0);p.setParamValue(P_voiceDispersion,0);p.setParamValue(P_drift,0);p.prepareToPlay(48000,64);
  juce::AudioBuffer<float> a(2,frames);juce::MidiBuffer m;p.processBlock(a,m);
  for(int note=60;note<68;++note)m.addEvent(juce::MidiMessage::noteOn(2,note,(juce::uint8)100),0);
  m.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),0);p.processBlock(a,m);m.clear();
  const int beforeA=held(p.getEngine(),60),beforeB=held(p.getExtensionEngine(),60);
  m.addEvent(juce::MidiMessage::noteOff(2,60),0);p.processBlock(a,m);m.clear();
  const int afterA=held(p.getEngine(),60),afterB=held(p.getExtensionEngine(),60);
  const bool pass=beforeA==1&&beforeB==1&&afterA==1&&afterB==0; failures+=!pass;
  std::printf("%s frames=%d before=%d/%d after=%d/%d expected=1/0 %s\n",circuit?"Circuit":"Legacy",frames,beforeA,beforeB,afterA,afterB,pass?"PASS":"FAIL");
 }
 std::printf("Sixteen release baseline: 4 checks, %d failures\n",failures);return failures?1:0;
}

#include "plugin/PluginProcessor.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
namespace {
int checks=0,failures=0;
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
void cc(juce::MidiBuffer& m,int ch,int n,int value){m.addEvent(juce::MidiMessage::controllerEvent(ch,n,value),0);}
void rpn(juce::MidiBuffer& m,int ch,int n,int value,int cents=-1){cc(m,ch,101,0);cc(m,ch,100,n);cc(m,ch,6,value);if(cents>=0)cc(m,ch,38,cents);}
bool same(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b){for(int c=0;c<2;++c)for(int i=0;i<a.getNumSamples();++i)if(!std::isfinite(a.getSample(c,i))||!std::isfinite(b.getSample(c,i))||std::bit_cast<std::uint32_t>(a.getSample(c,i))!=std::bit_cast<std::uint32_t>(b.getSample(c,i)))return false;return true;}
int find(const cs80::Engine& e,int note,int channel){for(int i=0;i<8;++i){auto v=e.getVoiceInfo(i);if(v.active&&v.note==note&&v.channel==channel)return i;}return -1;}
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;juce::TemporaryFile fixture("-manager-range-retarget");
 for(bool circuit:{false,true})for(bool upper:{false,true})for(int voiceMode:{0,1,2})for(int wheel:{4096,10240}){
  auto make=[&]{auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
   p->setParamValue(cs80::P_parityMode,0);p->setParamValue(cs80::P_mpeOn,1);p->setParamValue(cs80::P_voiceMode,voiceMode==1?1:0);
   p->setParamValue(cs80::P_kasScanTiming,0);p->setParamValue(cs80::P_arpOn,0);p->setParamValue(cs80::P_keyRelease,5);p->setParamValue(cs80::P_drift,0);p->setParamValue(cs80::P_voiceDispersion,0);p->setParamValue(cs80::P_analogNoise,0);
   p->prepareToPlay(48000,256);return p;};
  auto host=make(),reference=make();juce::AudioBuffer<float>a(2,0),b(2,0);juce::MidiBuffer midi,other;
  auto tick=[&]{host->processBlock(a,midi);reference->processBlock(b,other);midi.clear();other.clear();};tick();
  const int manager=upper?16:1,member=upper?15:2;
  for(auto* m:{&midi,&other}){rpn(*m,1,6,7);rpn(*m,16,6,7);rpn(*m,manager,0,12);m->addEvent(juce::MidiMessage::pitchWheel(manager,wheel),0);}tick();
  if(voiceMode==2){host->setParamValue(cs80::P_voiceMode,1);reference->setParamValue(cs80::P_voiceMode,1);tick();}
  for(auto* m:{&midi,&other}){if(voiceMode)for(int n=40;n<48;++n)m->addEvent(juce::MidiMessage::noteOn(5,n,(juce::uint8)100),0);m->addEvent(juce::MidiMessage::pitchWheel(member,9216),0);m->addEvent(juce::MidiMessage::noteOn(member,60,(juce::uint8)100),0);}tick();
  const auto& target=voiceMode?host->getExtensionEngine():host->getEngine();
  int index=find(target,60,member);check(index>=0,"range witness owns note on intended instrument");
  a.setSize(2,256);b.setSize(2,256);tick();check(same(a,b),"initial identical manager state renders exact audio");
  bool audible=false;for(int c=0;c<2;++c)for(int n=0;n<256;++n)audible|=a.getSample(c,n)!=0;check(audible,"comparison includes audible output");
  auto range=[&](int semis,int cents,const char* label){for(auto* m:{&midi,&other})rpn(*m,manager,0,semis,cents);other.addEvent(juce::MidiMessage::pitchWheel(manager,wheel),0);tick();check(same(a,b),label);};
  range(0,-1,"zero manager range matches explicit same-wheel replay");
  range(12,50,"zero-to-fractional range recovers the retained normalized wheel");
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOff(member,60),0);tick();
  if(index>=0)check(target.getVoiceInfo(index).active&&!target.getVoiceInfo(index).held,"released tail remains sounding for manager-range witness");
  range(4,-1,"manager range change continues to affect released sounding notes");
  if(index>=0)check(target.getVoiceInfo(index).bend==6.0f,"manager rescaling preserves the released member bend target");
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOn(member,61,(juce::uint8)100),0);tick();check(same(a,b),"future note inherits changed manager sensitivity without another bend");
  for(auto* m:{&midi,&other})rpn(*m,manager,6,7);other.addEvent(juce::MidiMessage::pitchWheel(manager,wheel),0);tick();check(same(a,b),"same-role MCM default reset rescales retained manager bend");
 }
 // Manual fallback and invalidation boundaries use the real processor too.
 for(bool circuit:{false,true})for(int boundary:{0,1,2,3,4}){
  auto make=[&]{auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);p->setParamValue(cs80::P_parityMode,0);p->setParamValue(cs80::P_mpeOn,1);p->setParamValue(cs80::P_voiceMode,0);p->setParamValue(cs80::P_pitchBendRange,12);p->setParamValue(cs80::P_kasScanTiming,0);p->setParamValue(cs80::P_arpOn,0);p->setParamValue(cs80::P_drift,0);p->setParamValue(cs80::P_voiceDispersion,0);p->setParamValue(cs80::P_analogNoise,0);p->prepareToPlay(48000,256);return p;};
  auto host=make(),reference=make();juce::AudioBuffer<float>a(2,0),b(2,0);juce::MidiBuffer midi,other;
  auto tick=[&]{host->processBlock(a,midi);reference->processBlock(b,other);midi.clear();other.clear();};tick();
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::pitchWheel(1,10240),0);tick();
  if(boundary==0){host->setParamValue(cs80::P_pitchBendRange,4);reference->setParamValue(cs80::P_pitchBendRange,4);other.addEvent(juce::MidiMessage::pitchWheel(1,10240),0);tick();}
  if(boundary==1){host->setParamValue(cs80::P_mpeOn,0);reference->setParamValue(cs80::P_mpeOn,0);tick();host->setParamValue(cs80::P_mpeOn,1);reference->setParamValue(cs80::P_mpeOn,1);tick();rpn(midi,1,0,4);tick();}
  if(boundary==2){for(auto* m:{&midi,&other})rpn(*m,1,6,0);tick();for(auto* m:{&midi,&other})rpn(*m,1,6,7);tick();rpn(midi,1,0,4);tick();}
  if(boundary==3){host->prepareToPlay(48000,256);reference->prepareToPlay(48000,256);rpn(midi,1,0,4);tick();}
  if(boundary==4){for(auto* m:{&midi,&other})cc(*m,1,121,0);tick();rpn(midi,1,0,4);tick();}
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),0);tick();a.setSize(2,256);b.setSize(2,256);tick();
  check(same(a,b),boundary==0?"manual MPE fallback edit rescales held wheel":"mode/role/prepare invalidation prevents old manager input from being reused");
 }
 // Channel 1 retains its conventional own-channel fallback when no lower zone exists.
 for(bool circuit:{false,true})for(bool opposite:{false,true}){
  auto make=[&]{auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);p->setParamValue(cs80::P_parityMode,0);p->setParamValue(cs80::P_mpeOn,1);p->setParamValue(cs80::P_voiceMode,0);p->setParamValue(cs80::P_pitchBendRange,12);p->setParamValue(cs80::P_kasScanTiming,0);p->setParamValue(cs80::P_arpOn,0);p->setParamValue(cs80::P_drift,0);p->setParamValue(cs80::P_voiceDispersion,0);p->setParamValue(cs80::P_analogNoise,0);p->prepareToPlay(48000,64);return p;};
  auto host=make(),reference=make();juce::AudioBuffer<float>a(2,0),b(2,0);juce::MidiBuffer midi,other;
  auto tick=[&]{host->processBlock(a,midi);reference->processBlock(b,other);midi.clear();other.clear();};tick();
  for(auto* m:{&midi,&other}){rpn(*m,1,6,0);rpn(*m,16,6,opposite?2:0);}tick();
  midi.addEvent(juce::MidiMessage::pitchWheel(1,10240),0);
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);tick();
  a.setSize(2,64);b.setSize(2,64);for(int settle=0;settle<64;++settle)tick();
  for(int rank=0;rank<2;++rank){float semis=12*std::log2(host->getEngine().getRankInfo(0,rank).oscillatorHz/reference->getEngine().getRankInfo(0,rank).oscillatorHz);check(std::isfinite(semis)&&std::abs(semis-3.0f)<.001f,"disabled lower-zone defaults cannot replace channel1 conventional bend range");}
 }
 std::cout<<"MPE manager retarget: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

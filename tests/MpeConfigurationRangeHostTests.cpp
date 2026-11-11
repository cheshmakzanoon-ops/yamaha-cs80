#include "plugin/PluginProcessor.h"
#include <cmath>
#include <iostream>
#include <memory>
namespace {
int checks=0,failures=0;
void check(bool ok,const char* label){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<label<<'\n';}}
void rpn(cs80::MpeZoneLayout& z,int ch,int n,int value){z.controlChange(ch,101,0);z.controlChange(ch,100,n);z.controlChange(ch,6,value);}
void cc(juce::MidiBuffer& m,int ch,int n,int value){m.addEvent(juce::MidiMessage::controllerEvent(ch,n,value),0);}
void rpn(juce::MidiBuffer& m,int ch,int n,int value){cc(m,ch,101,0);cc(m,ch,100,n);cc(m,ch,6,value);}
int voice(const cs80::Engine& e,int note,int ch){for(int i=0;i<8;++i){auto v=e.getVoiceInfo(i);if(v.active&&v.held&&v.note==note&&v.channel==ch)return i;}return -1;}
}
int main(){
 for(bool upper:{false,true})for(int count=0;count<=15;++count){
  cs80::MpeZoneLayout z;rpn(z,1,6,7);rpn(z,16,6,7);
  rpn(z,1,0,12);rpn(z,2,0,24);rpn(z,16,0,9);rpn(z,15,0,36);
  const int master=upper?16:1;rpn(z,master,6,count);
  check(z.masterRange(upper)==2&&z.memberRange(upper)==48,"received MCM installs explicit 2/48 defaults on addressed zone");
  check(z.masterRange(!upper)==(upper?12:9)&&z.memberRange(!upper)==(upper?24:36),"opposite zone ranges survive overlap resize or disable");
  if(count>0){rpn(z,master,0,11);rpn(z,upper?15:2,0,23);}
  rpn(z,master,6,count);check(z.masterRange(upper)==2&&z.memberRange(upper)==48,"same-size MCM also installs explicit defaults");
 }
 {
  cs80::MpeZoneLayout z;check(z.masterRange(false)<0&&z.memberRange(false)<0,"manual startup retains parameter fallback before MCM");
  rpn(z,1,6,7);rpn(z,16,6,7);rpn(z,1,0,12);rpn(z,2,0,24);
  rpn(z,2,6,3);check(z.masterRange(false)==12&&z.memberRange(false)==24,"member-channel RPN6 cannot reset zone ranges");
  z.controlChange(1,101,0);z.controlChange(1,100,6);z.controlChange(1,38,0);
  check(z.masterRange(false)==12&&z.memberRange(false)==24,"CC38 without MCM data entry cannot reset ranges");
  z.controlChange(1,99,1);z.controlChange(1,98,2);z.controlChange(1,6,4);
  check(z.masterRange(false)==12&&z.memberRange(false)==24,"NRPN data cannot reset ranges");
  z.controlChange(1,101,127);z.controlChange(1,100,127);z.controlChange(1,6,4);
  check(z.masterRange(false)==12&&z.memberRange(false)==24,"null RPN data cannot reset ranges");
  rpn(z,1,6,7);rpn(z,1,0,9);rpn(z,2,0,36);z.controlChange(2,38,50);
  check(z.masterRange(false)==9&&z.memberRange(false)==36.5f,"subsequent RPN0 including fractional data overrides defaults");
  rpn(z,1,6,7);rpn(z,2,0,24);z.controlChange(1,38,50);
  check(z.masterRange(false)==2&&z.memberRange(false)==24,"CC38 with a live MCM data MSB does not reapply defaults");
 }
 juce::ScopedJuceInitialiser_GUI init;juce::TemporaryFile fixture("-mcm-range");
 for(bool circuit:{false,true})for(bool sixteen:{false,true})for(int frames:{0,32})for(int mode:{0,1,2}){
  // mode0 is manual lower-zone fallback; mode1/2 receive lower/upper MCM.
  auto make=[&]{auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
   p->setParamValue(cs80::P_parityMode,0);p->setParamValue(cs80::P_voiceMode,0);p->setParamValue(cs80::P_mpeOn,1);
   p->setParamValue(cs80::P_pitchBendRange,12);p->setParamValue(cs80::P_mpeBendRange,7);
   p->setParamValue(cs80::P_kasScanTiming,0);p->setParamValue(cs80::P_arpOn,0);p->setParamValue(cs80::P_voiceDispersion,0);p->setParamValue(cs80::P_drift,0);p->setParamValue(cs80::P_analogNoise,0);
   p->prepareToPlay(48000,64);return p;};
  auto host=make(),neutral=make();juce::AudioBuffer<float>a(2,frames),b(2,frames);juce::MidiBuffer midi,other;
  auto tick=[&]{host->processBlock(a,midi);neutral->processBlock(b,other);midi.clear();other.clear();};tick();
  const bool upper=mode==2;const int master=upper?16:1,member=upper?15:2;
  if(mode)for(auto* m:{&midi,&other})rpn(*m,master,6,15);tick();
  // A later parameter edit must not override negotiated/defaulted MCM ranges.
  host->setParamValue(cs80::P_pitchBendRange,10);neutral->setParamValue(cs80::P_pitchBendRange,10);
  host->setParamValue(cs80::P_mpeBendRange,9);neutral->setParamValue(cs80::P_mpeBendRange,9);
  if(sixteen){host->setParamValue(cs80::P_voiceMode,1);neutral->setParamValue(cs80::P_voiceMode,1);}tick();
  midi.addEvent(juce::MidiMessage::pitchWheel(master,10240),0);
  for(auto* m:{&midi,&other}){m->addEvent(juce::MidiMessage::pitchWheel(member,10240),0);if(sixteen)for(int n=40;n<48;++n)m->addEvent(juce::MidiMessage::noteOn(5,n,(juce::uint8)100),0);m->addEvent(juce::MidiMessage::noteOn(member,60,(juce::uint8)100),0);}tick();
  const auto& e=sixteen?host->getExtensionEngine():host->getEngine();const auto& ref=sixteen?neutral->getExtensionEngine():neutral->getEngine();
  int i=voice(e,60,member),j=voice(ref,60,member);check(i>=0&&j>=0,"range witness has actual notes on intended instrument");
  if(i>=0)check(e.getVoiceInfo(i).bend==(mode?12.0f:2.25f),"member onset uses received MCM default or unchanged manual fallback");
  a.setSize(2,64);b.setSize(2,64);for(int settle=0;settle<64;++settle)tick();
  if(i>=0&&j>=0)for(int rank=0;rank<2;++rank){float semis=12*std::log2(e.getRankInfo(i,rank).oscillatorHz/ref.getRankInfo(j,rank).oscillatorHz);check(std::isfinite(semis)&&std::abs(semis-(mode?.5f:2.5f))<.001f,"settled manager bend uses received MCM default or manual fallback");}
 }
 std::cout<<"MCM bend ranges: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

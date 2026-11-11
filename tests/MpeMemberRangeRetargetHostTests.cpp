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
 juce::ScopedJuceInitialiser_GUI init;juce::TemporaryFile fixture("-member-range-retarget");
 for(bool circuit:{false,true})for(bool upper:{false,true})for(int voiceMode:{0,1,2})for(int wheel:{4096,10240}){
  auto make=[&]{auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
   p->setParamValue(cs80::P_parityMode,0);p->setParamValue(cs80::P_mpeOn,1);p->setParamValue(cs80::P_voiceMode,voiceMode==1?1:0);
   p->setParamValue(cs80::P_kasScanTiming,0);p->setParamValue(cs80::P_arpOn,0);p->setParamValue(cs80::P_keyRelease,5);p->setParamValue(cs80::P_drift,0);p->setParamValue(cs80::P_voiceDispersion,0);p->setParamValue(cs80::P_analogNoise,0);
   p->prepareToPlay(48000,256);return p;};
  auto host=make(),reference=make();juce::AudioBuffer<float>a(2,0),b(2,0);juce::MidiBuffer midi,other;
  auto tick=[&]{host->processBlock(a,midi);reference->processBlock(b,other);midi.clear();other.clear();};tick();
  const int manager=upper?16:1,member=upper?15:2;
  for(auto* m:{&midi,&other}){rpn(*m,1,6,7);rpn(*m,16,6,7);rpn(*m,member,0,12);m->addEvent(juce::MidiMessage::pitchWheel(manager,10240),0);m->addEvent(juce::MidiMessage::pitchWheel(member,wheel),0);}tick();
  if(voiceMode==2){host->setParamValue(cs80::P_voiceMode,1);reference->setParamValue(cs80::P_voiceMode,1);tick();}
  for(auto* m:{&midi,&other}){if(voiceMode)for(int n=40;n<48;++n)m->addEvent(juce::MidiMessage::noteOn(5,n,(juce::uint8)100),0);m->addEvent(juce::MidiMessage::noteOn(member,60,(juce::uint8)100),0);}tick();
  const auto& target=voiceMode?host->getExtensionEngine():host->getEngine();
  int index=find(target,60,member);check(index>=0,"range witness owns note on intended instrument");
  a.setSize(2,256);b.setSize(2,256);tick();check(same(a,b),"initial identical member state renders exact audio");
  bool audible=false;for(int c=0;c<2;++c)for(int n=0;n<256;++n)audible|=a.getSample(c,n)!=0;check(audible,"comparison includes audible output");
  auto range=[&](int semis,int cents,const char* label){for(auto* m:{&midi,&other})rpn(*m,member,0,semis,cents);other.addEvent(juce::MidiMessage::pitchWheel(member,wheel),0);tick();check(same(a,b),label);};
  range(0,-1,"zero member range matches explicit same-wheel replay");
  range(12,50,"zero-to-fractional range recovers the retained normalized wheel");
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOff(member,60),0);tick();
  if(index>=0)check(target.getVoiceInfo(index).active&&!target.getVoiceInfo(index).held,"released tail remains sounding for member-range witness");
  range(4,-1,"range change matches replay while released targets stay fixed");
  if(index>=0)check(target.getVoiceInfo(index).bend==((wheel-8192)/8192.0f)*12.5f,"rescaling preserves the released member bend target");
  for(auto* m:{&midi,&other})m->addEvent(juce::MidiMessage::noteOn(member,61,(juce::uint8)100),0);tick();check(same(a,b),"future note inherits changed member sensitivity without another bend");
  for(auto* m:{&midi,&other})rpn(*m,manager,6,7);other.addEvent(juce::MidiMessage::pitchWheel(member,wheel),0);tick();check(same(a,b),"same-role MCM default reset rescales retained member bend");
 }
 // Direct target witnesses complement the processor's exact-audio matrix.
 for(int policy:{0,1}) {
  cs80::ParamValues p; p[cs80::P_sourcePerformance]=policy;p[cs80::P_parityMode]=0;p[cs80::P_mpeOn]=1;
  p[cs80::P_kasScanTiming]=p[cs80::P_arpOn]=0;p[cs80::P_mpeBendRange]=12;p[cs80::P_keyRelease]=5;
  cs80::Engine e;e.prepare(48000);e.setParams(p);
  auto erpn=[&](int channel,int number,int value,int cents=-1){e.controlChange(101,0,channel);e.controlChange(100,number/127.0f,channel);e.controlChange(6,value/127.0f,channel);if(cents>=0)e.controlChange(38,cents/127.0f,channel);};
  auto bend=[&](int note,int channel){int i=find(e,note,channel);check(i>=0,"target witness retains its intended note/channel");return i<0?999.0f:e.getVoiceInfo(i).bend;};
  e.pitchBend(2,.25f);e.noteOn(60,.8f,2);p[cs80::P_mpeBendRange]=4;e.setParams(p);
  check(bend(60,2)==1,"manual fallback resizes a held member before negotiation");
  erpn(1,6,3);erpn(16,6,2);e.pitchBend(2,.25f);e.pitchBend(3,-.5f);e.pitchBend(15,.125f);
  e.noteOn(61,.8f,3);e.noteOn(62,.8f,15);erpn(4,0,8);
  check(bend(60,2)==2&&bend(61,3)==-4,"RPN on another member rescales all held members using distinct raw values");
  check(bend(62,15)==6,"opposite zone is unaffected by lower member sensitivity");
  p[cs80::P_mpeBendRange]=20;e.setParams(p);check(bend(60,2)==2,"manual parameter cannot replace negotiated range");
  e.controlChange(64,1.0f,1);e.noteOff(60,2);
  {const int i=find(e,60,2);check(i>=0&&!e.getVoiceInfo(i).held&&e.getVoiceInfo(i).pedalHeld,"pedal witness has accepted release while retaining pedal hold");}
  e.pitchBend(2,-.25f);erpn(3,0,12,50);
  check(bend(60,2)==2,"pedal-held accepted-release target does not follow cache or range changes");
  e.noteOn(63,.8f,2);check(bend(63,2)==-3.125f,"future note uses latest raw input and fractional sensitivity");
  e.controlChange(121,0,2);erpn(3,0,4);check(bend(63,2)==0,"CC121 zero remains zero after sensitivity change");
  // Role invalidation must not initialize a new zero cache into a surviving voice.
  e.pitchBend(3,.5f);erpn(1,6,0);erpn(1,6,3);erpn(2,0,7);
  check(find(e,61,3)<0,"membership loss stops the old voice rather than reviving its target");
  e.noteOn(65,.8f,3);check(bend(65,3)==0,"re-entering member starts with cleared raw expression");
 }
 // The later role-transfer policy cancels old held voices before retargeting.
 for(int policy:{0,1}) {
  cs80::Engine e;cs80::ParamValues p;p[cs80::P_sourcePerformance]=policy;p[cs80::P_parityMode]=0;p[cs80::P_mpeOn]=1;p[cs80::P_kasScanTiming]=0;p[cs80::P_arpOn]=0;p[cs80::P_mpeBendRange]=48;
  e.prepare(48000);e.setParams(p);e.pitchBend(2,.5f);e.noteOn(60,.8f,2);
  auto change=[&](int ch,int number,int value){e.controlChange(101,0,ch);e.controlChange(100,number/127.0f,ch);e.controlChange(6,value/127.0f,ch);};
  change(16,6,15);change(2,0,7);
  check(find(e,60,2)<0,"still-MPE zone transfer stops the old held voice before retarget");
  e.noteOn(61,.8f,2);int i=find(e,61,2);check(i>=0&&e.getVoiceInfo(i).bend==0,"fresh transferred member does not inherit invalidated raw input");
 }
 // Equal-offset event order uses accepted NoteOff, not event-block membership.
 for(int policy:{0,1})for(bool releaseFirst:{false,true}) {
  cs80::Engine e;cs80::ParamValues p;p[cs80::P_sourcePerformance]=policy;p[cs80::P_parityMode]=0;p[cs80::P_mpeOn]=1;p[cs80::P_kasScanTiming]=0;p[cs80::P_arpOn]=0;p[cs80::P_mpeBendRange]=12;
  e.prepare(48000);e.setParams(p);e.pitchBend(2,.25f);e.noteOn(60,.8f,2);
  e.controlChange(101,0,2);e.controlChange(100,0,2);
  if(releaseFirst)e.noteOff(60,2);e.controlChange(6,4/127.0f,2);if(!releaseFirst)e.noteOff(60,2);
  int i=find(e,60,2);check(i>=0&&e.getVoiceInfo(i).bend==(releaseFirst?3.0f:1.0f),"accepted release ordering bounds member retarget");
 }
 std::cout<<"MPE member range retarget: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

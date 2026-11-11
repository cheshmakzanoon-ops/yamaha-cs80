#include "plugin/PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
namespace {
int checks=0, failures=0;
void check(bool ok,const char* what){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<what<<'\n';}}
void cc(juce::MidiBuffer& m,int channel,int control,int value){m.addEvent(juce::MidiMessage::controllerEvent(channel,control,value),0);}
void rpn(juce::MidiBuffer& m,int channel,int number,int value){cc(m,channel,101,0);cc(m,channel,100,number);cc(m,channel,6,value);}
int voice(const cs80::Engine& e,int note,int channel){for(int i=0;i<8;++i){auto v=e.getVoiceInfo(i);if(v.active&&v.note==note&&v.channel==channel)return i;}return -1;}
}
int main(){
 juce::ScopedJuceInitialiser_GUI init;juce::TemporaryFile fixture("-controller-handoff");
 for(bool circuit:{false,true})for(int frames:{0,32})for(bool reactivate:{false,true})for(bool upper:{false,true}){
  auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
  host->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
  for(auto [key,value]:std::array<std::pair<int,float>,10>{{{cs80::P_parityMode,0},{cs80::P_mpeOn,1},{cs80::P_voiceMode,0},{cs80::P_kasScanTiming,0},{cs80::P_arpOn,0},{cs80::P_voiceDispersion,0},{cs80::P_drift,0},{cs80::P_analogNoise,0},{cs80::P_pressureCurve,0},{cs80::P_pressureThreshold,0}}})host->setParamValue(key,value);
  host->setParamValue(cs80::P_mod1Amount,1);
  host->getModernLayer().setSlot(0,{cs80::modern::ModSource::timbre,cs80::modern::ModSource::none,cs80::modern::ModDestination::pulseWidth,cs80::modern::ModTarget::both});
  host->prepareToPlay(48000,64);juce::AudioBuffer<float> audio(2,frames);juce::MidiBuffer midi;
  auto tick=[&]{host->processBlock(audio,midi);midi.clear();};tick();
  if(reactivate){host->setParamValue(cs80::P_voiceMode,1);tick();rpn(midi,1,6,4);tick();host->setParamValue(cs80::P_voiceMode,0);tick();}
  const int master=upper?16:1, member=upper?15:2;
  rpn(midi,1,6,0);rpn(midi,16,6,0);rpn(midi,master,6,2);rpn(midi,master,0,12);rpn(midi,member,0,24);
  midi.addEvent(juce::MidiMessage::pitchWheel(member,10240),0);
  midi.addEvent(juce::MidiMessage::pitchWheel(master,10240),0);
  midi.addEvent(juce::MidiMessage::channelPressureChange(member,80),0);cc(midi,member,74,96);cc(midi,master,64,127);
  // Leave the master RPN selector and its Data Entry MSB live across activation.
  tick();
  host->setParamValue(cs80::P_voiceMode,1);tick();
  const auto& ext=host->getExtensionEngine();const auto& zone=ext.getMpeZones();
  bool empty=true;for(int i=0;i<8;++i)empty&=!ext.getVoiceInfo(i).active;check(empty,"handoff never imports primary voices");
  check(zone.lowerMemberCount()==(upper?0:2)&&zone.upperMemberCount()==(upper?2:0),"activation inherits negotiated zone roles");
  check(zone.masterRange(upper)==12&&zone.memberRange(upper)==24,"activation inherits negotiated bend ranges");
  cc(midi,master,38,50);tick();check(ext.getMpeZones().masterRange(upper)==12.5f,"Data Entry LSB continues the inherited selector and MSB");
  cc(midi,master,6,7);tick();
  check(ext.getMpeZones().masterRange(upper)==7,"partial RPN selection survives activation");
  for(int n=40;n<48;++n)midi.addEvent(juce::MidiMessage::noteOn(n==40?member:8,n==40?60:n,(juce::uint8)100),0);
  midi.addEvent(juce::MidiMessage::noteOn(member,60,(juce::uint8)100),0);tick();
  int index=voice(ext,60,member);check(index>=0,"ninth note belongs to newly active extension");
  if(index>=0){auto v=ext.getVoiceInfo(index);check(v.bend==6,"extension onset retains raw member bend with negotiated range");check(v.pressure==80.0f/127,"extension onset retains raw member pressure");}
  audio.setSize(2,64);
  // Handoff copies targets, not the primary's already-moving dezipper.
  for(int settle=0;settle<64;++settle)tick();
  const int primaryIndex=voice(host->getEngine(),60,member);check(primaryIndex>=0,"primary retains matching note for master-bend comparison");
  if(index>=0&&primaryIndex>=0)for(int rank=0;rank<2;++rank)check(ext.getRankInfo(index,rank).oscillatorHz==host->getEngine().getRankInfo(primaryIndex,rank).oscillatorHz,"both zone master bends reach new extension at the same oscillator frequency");
  if(index>=0)for(int rank=0;rank<2;++rank)check(ext.getModernRankOffsets(index,rank).pulseWidth==(96.0f/127)*cs80::modern::kPulseWidthRange,"extension onset retains member timbre");
  midi.addEvent(juce::MidiMessage::noteOff(member,60),0);tick();
  if(index>=0)check(ext.getVoiceInfo(index).pedalHeld,"inherited zone-master sustain holds accepted release");
  cc(midi,master,64,0);tick();
  if(index>=0)check(!ext.getVoiceInfo(index).pedalHeld,"new pedal release releases inherited sustain");
  // Same-role reset while dormant must supersede prior raw expression.
  host->setParamValue(cs80::P_voiceMode,0);tick();cc(midi,member,121,0);cc(midi,master,99,1);cc(midi,master,98,2);tick();host->setParamValue(cs80::P_voiceMode,1);tick();
  int primaryHeld=0;bool extensionEmpty=true;for(int i=0;i<8;++i){primaryHeld+=host->getEngine().getVoiceInfo(i).held?1:0;extensionEmpty&=!ext.getVoiceInfo(i).active;}
  check(primaryHeld==8,"non-vacuous reactivation witness retains eight primary notes");check(extensionEmpty,"reactivation never copies held primary voices into extension");
  cc(midi,master,6,9);tick();check(ext.getMpeZones().masterRange(upper)==7,"dormant NRPN suspension prevents stale RPN data entry after reactivation");
  midi.addEvent(juce::MidiMessage::noteOn(member,61,(juce::uint8)100),0);tick();index=voice(ext,61,member);
  check(index>=0,"reactivation still routes the next note to extension");
  if(index>=0){auto v=ext.getVoiceInfo(index);check(v.bend==0&&v.pressure==0,"dormant CC121 replaces stale member targets");}
 }
 // Primary lifecycle invalidation wins over previously retained member state.
 for(bool circuit:{false,true})for(int resetKind:{0,1,2}){
  auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
  host->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
  host->setParamValue(cs80::P_parityMode,0);host->setParamValue(cs80::P_voiceMode,0);host->setParamValue(cs80::P_mpeOn,1);host->setParamValue(cs80::P_kasScanTiming,0);host->setParamValue(cs80::P_arpOn,0);
  host->prepareToPlay(48000,64);juce::AudioBuffer<float> audio(2,0);juce::MidiBuffer midi;
  auto tick=[&]{host->processBlock(audio,midi);midi.clear();};tick();
  midi.addEvent(juce::MidiMessage::pitchWheel(2,10240),0);midi.addEvent(juce::MidiMessage::channelPressureChange(2,80),0);tick();
  if(resetKind==0){host->setParamValue(cs80::P_mpeOn,0);tick();host->setParamValue(cs80::P_mpeOn,1);}
  if(resetKind==1)host->prepareToPlay(48000,64);
  if(resetKind==2)host->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::legacy:Cs80AudioProcessor::EngineMode::circuit);
  host->setParamValue(cs80::P_voiceMode,1);tick();
  for(int n=40;n<48;++n)midi.addEvent(juce::MidiMessage::noteOn(8,n,(juce::uint8)100),0);
  midi.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),0);tick();
  const auto& ext=host->getExtensionEngine();const int index=voice(ext,60,2);check(index>=0,"reset boundary witness uses extension");
  if(index>=0){auto v=ext.getVoiceInfo(index);check(v.bend==0&&v.pressure==0,"activation cannot resurrect primary state invalidated by mode/prepare/policy reset");}
 }
 // All retained global controller targets also feed the new instrument.
 for(bool circuit:{false,true})for(bool mpe:{false,true}){
  auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
  host->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
  host->setParamValue(cs80::P_parityMode,0);host->setParamValue(cs80::P_voiceMode,0);host->setParamValue(cs80::P_mpeOn,mpe?1:0);
  host->setParamValue(cs80::P_kasScanTiming,0);host->setParamValue(cs80::P_arpOn,0);
  host->setParamValue(cs80::P_pressureCurve,0);host->setParamValue(cs80::P_pressureThreshold,0);
  using S=cs80::modern::ModSource;using D=cs80::modern::ModDestination;
  const std::array<S,5> sources{S::modWheel,S::channelPressure,S::pitchBend,S::expression,S::ribbon};
  const std::array<D,5> destinations{D::pulseWidth,D::level,D::pitch,D::pan,D::hpCutoff};
  for(int i=0;i<5;++i){host->setParamValue(cs80::P_mod1Amount+i,1);host->getModernLayer().setSlot(i,{sources[i],S::none,destinations[i],cs80::modern::ModTarget::both});}
  host->prepareToPlay(48000,64);juce::AudioBuffer<float> audio(2,0);juce::MidiBuffer midi;
  auto tick=[&]{host->processBlock(audio,midi);midi.clear();};tick();
  cc(midi,1,1,76);cc(midi,1,11,51);midi.addEvent(juce::MidiMessage::channelPressureChange(1,80),0);
  midi.addEvent(juce::MidiMessage::pitchWheel(1,10240),0);host->setRibbonFromUI(.25f);tick();
  host->setParamValue(cs80::P_voiceMode,1);tick();
  for(int n=40;n<48;++n)midi.addEvent(juce::MidiMessage::noteOn(8,n,(juce::uint8)100),0);
  midi.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)100),0);tick();
  const auto& ext=host->getExtensionEngine();int index=voice(ext,60,2);check(index>=0,"global target witness uses extension");
  audio.setSize(2,64);tick();
  if(index>=0)for(int rank=0;rank<2;++rank){const auto& m=ext.getModernRankOffsets(index,rank);
   check(m.pulseWidth==(76.0f/127)*cs80::modern::kPulseWidthRange,"mod wheel target is handed off");
   check(m.level==80.0f/127,"channel pressure matrix target is handed off");
   check(m.pitch==.25f*cs80::modern::kPitchSemitones,"pitch-wheel matrix target is handed off");
   check(m.pan==51.0f/127,"expression target is handed off");
   check(m.hpCutoff==.25f*cs80::modern::kCutoffOctaves,"ribbon target is handed off");
  }
 }
 std::cout<<"Controller handoff: "<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}

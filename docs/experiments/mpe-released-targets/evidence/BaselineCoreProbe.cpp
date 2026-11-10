#include "core/Engine.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
using namespace cs80;
int main()
{
 int checks=0, failures=0;
 auto check=[&](bool value,const char* text){++checks;if(!value){++failures;std::printf("FAIL: %s\n",text);}};
 auto equal=[](float a,float b){return std::isfinite(a)&&std::isfinite(b)&&std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
 for(bool circuit:{false,true}) for(int channel:{2,15}) for(bool pedal:{false,true})
 {
  Engine e; e.prepare(48000); ParamValues p;
  p[P_sourcePerformance]=circuit; p[P_mpeOn]=1; p[P_kasScanTiming]=p[P_arpOn]=0;
  p[P_voiceDispersion]=p[P_drift]=p[P_analogNoise]=0; p[P_pressureCurve]=p[P_pressureThreshold]=0;
  p[P_keyRelease]=5; p[P_mpeBendRange]=48; p[P_mod1Amount]=1; e.setParams(p);
  modern::ModernConfig config; config.slots[0]={modern::ModSource::timbre,modern::ModSource::none,modern::ModDestination::pulseWidth,modern::ModTarget::both}; e.setModernConfig(config);
  auto zone=[&](int manager,int members){e.controlChange(101,0,manager);e.controlChange(100,6.0f/127,manager);e.controlChange(6,members/127.0f,manager);};
  zone(1,0);zone(16,0);zone(1,2);zone(16,2);
  e.pitchBend(channel,.25f);e.channelPressure(channel,.7f);e.controlChange(74,.8f,channel);
  e.noteOn(60,.8f,channel);std::array<float,512> audio{};e.render(audio.data(),256);
  const auto before=e.getVoiceInfo(0);const auto timbre=e.getModernRankOffsets(0,0).pulseWidth;
  check(before.active&&before.held&&before.channel==channel,"accepted held member fixture");
  if(pedal)e.controlChange(64,1,channel);e.noteOff(60,channel);
  const auto released=e.getVoiceInfo(0);check(released.active&&!released.held&&released.pedalHeld==pedal,"accepted release fixture");
  e.pitchBend(channel,-.5f);e.channelPressure(channel,.1f);e.controlChange(74,.2f,channel);e.render(audio.data(),256);
  const auto after=e.getVoiceInfo(0);check(after.active&&!after.held,"tail remains sounding after later member events");
  check(equal(after.bend,before.bend),"released member bend target retained");
  check(equal(after.pressure,before.pressure),"released member pressure target retained");
  check(equal(e.getModernRankOffsets(0,0).pulseWidth,timbre),"released member timbre trajectory retained");
 }
 std::printf("MPE released-target baseline: %d checks, %d failures\n",checks,failures);return failures?1:0;
}

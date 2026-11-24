#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cs80;
namespace {
constexpr double rate=48000;
int checks=0, failures=0;
double totalSeconds=0, maximumPeak=0;
void check(bool ok,const std::string& what) { ++checks; if(!ok){++failures;std::printf("FAIL: %s\n",what.c_str());} }
const FactoryPreset& preset(const char* name) {
 for(const auto& p:curatedLibrary()) if(p.name==name)return p;
 throw std::runtime_error(name);
}
struct Metrics { double peak=0,power=0;std::size_t samples=0;bool finite=true;double rms()const{return std::sqrt(power/std::max<std::size_t>(samples,1));} };
Metrics render(Engine& e,double seconds) {
 Metrics m;std::array<float,512> b{};int left=(int)std::llround(seconds*rate);
 while(left>0){int n=std::min(left,256);e.render(b.data(),n);left-=n;
  for(int i=0;i<2*n;++i){double x=b[i];m.finite=m.finite&&std::isfinite(x);m.peak=std::max(m.peak,std::abs(x));m.power+=x*x;++m.samples;}}
 totalSeconds+=seconds;maximumPeak=std::max(maximumPeak,m.peak);
 check(m.finite&&m.peak<1,"render is finite and below full scale");
 check(e.activeVoices()<=8,"physical voice capacity remains eight");return m;
}
int held(const Engine& e,int channel=-1,int note=-1,bool pedal=false){int n=0;for(int i=0;i<8;++i){auto v=e.getVoiceInfo(i);if(v.active&&(channel<0||v.channel==channel)&&(note<0||v.note==note)&&(pedal?v.pedalHeld:v.held))++n;}return n;}
bool hasHeld(const Engine& e,int note){return held(e,-1,note)>0;}
double retire(Engine& e,const std::string& label){double t=0;while(e.activeVoices()>0&&t<30){render(e,.25);t+=.25;}
 check(e.activeVoices()==0,label+": ordinary release retires every physical voice within 30 seconds");
 auto m=render(e,.25);check(m.rms()<1.e-4,label+": retired output settles below -80 dBFS RMS");return t;}
std::unique_ptr<Engine> fresh(const ParamValues& p){auto e=std::make_unique<Engine>();e->prepare(rate);e->setParams(p);return e;}
void run(const FactoryPreset& p,bool omitPedalUp){
 check(p.values[P_voiceMode]==0,p.name+": factory uses eight-voice polyphonic allocation");
 auto e=fresh(p.values);
 e->noteOn(60,.35f,2);render(*e,.025);e->noteOn(60,.85f,2);render(*e,.025);e->noteOn(67,.7f,3);render(*e,.05);
 check(held(*e,2,60)==2&&held(*e,3,67)==1,p.name+": repeated same key owns separate onsets");
 e->controlChange(64,1,2);e->noteOff(60,3);
 check(held(*e,2,60)==2,p.name+": wrong-channel note-off leaves both onsets held");
 e->noteOff(60,2);check(held(*e,2,60)==1&&held(*e,2,60,true)==1,p.name+": one note-off releases only the newest onset into pedal hold");
 for(int i=0;i<8;++i){auto v=e->getVoiceInfo(i);if(v.active&&v.channel==2&&v.held)check(v.velocity==.35f,p.name+": older strike identity survives newest note-off");}
 e->noteOff(60,2);e->noteOff(67,3);render(*e,.15);
 check(held(*e,2,60)==0&&held(*e,2,60,true)==2&&held(*e,3,-1,true)==0,p.name+": sustain holds only its own channel");
 e->controlChange(123,0,2);check(held(*e,2,60,true)==2,p.name+": channel All Notes Off respects the still-held pedal");
 if(!omitPedalUp)e->controlChange(64,0,2);
 check(held(*e,-1,-1,true)==0,p.name+": pedal-up clears every sustained onset");
 if(omitPedalUp)e->controlChange(64,0,2); // clean up after deliberately missing event; do not conceal the failed observation
 double overlapRelease=retire(*e,p.name+" overlap/pedal");

 e=fresh(p.values);
 for(int n=48;n<64;++n){e->noteOn(n,.85f,2);render(*e,.01);}
 check(e->activeVoices()==8&&held(*e)==8,p.name+": sixteen held keys occupy at most eight circuits");
 check(hasHeld(*e,48)&&!hasHeld(*e,56),p.name+": extra held keys do not steal the oldest held circuit");
 e->noteOff(48,2);render(*e,.025);
 check(!hasHeld(*e,48)&&hasHeld(*e,56)&&held(*e)==8,p.name+": a released circuit admits the lowest waiting key");
 for(int i=0;i<8;++i){auto v=e->getVoiceInfo(i);if(v.active&&v.held&&v.note==56)check(v.velocity==0,p.name+": waiting-key admission has no invented new strike velocity");}
 for(int n=49;n<64;++n)e->noteOff(n,2);
 check(held(*e)==0&&held(*e,-1,-1,true)==0,p.name+": releasing the over-capacity chord leaves no held or pedal voice");
 double excessRelease=retire(*e,p.name+" over-capacity");
 e->noteOn(60,.9f,2);render(*e,.1);e->noteOn(67,.8f,3);render(*e,.05);
 e->allNotesOff(false);check(e->activeVoices()==0&&e->activeVoiceMask()==0,p.name+": global panic immediately deactivates every physical voice");
 // Panic closes voices immediately; the shared PRA coupling capacitors,
 // noise/FX paths and reconstruction pipeline continue to run. Report the
 // first quarter-second without mislabelling it silent, then require that
 // their remaining output settles below -80 dBFS RMS in the next quarter.
 // Reported group latency alone does not bound those recursive histories.
 const double drain=.25;
 auto panic=render(*e,drain);
 auto settled=render(*e,.25);check(settled.rms()<1.e-4,p.name+": panic residual in the 250-500 ms window is below -80 dBFS RMS");
 e->noteOn(72,.8f,2);render(*e,.1);check(held(*e)==1&&hasHeld(*e,72),p.name+": fresh onset after panic cannot resurrect old waiting keys");e->allNotesOff(false);

 auto mpe=p.values;mpe[P_mpeOn]=1;mpe[P_mpeBendRange]=12;
 e=fresh(mpe);e->noteOn(60,.75f,2);e->noteOn(60,.75f,3);render(*e,.04);
 e->polyPressure(60,.9f,2);e->pitchBend(2,.5f);render(*e,.08);
 for(int i=0;i<8;++i){auto v=e->getVoiceInfo(i);if(v.active&&v.channel==2)check(v.pressure>.7f&&v.bend>0,p.name+": member pressure and pitch reach the addressed voice");if(v.active&&v.channel==3)check(v.pressure<1.e-5f&&v.bend==0,p.name+": same-key neighboring channel keeps its own expression");}
 e->controlChange(120,0,2);check(held(*e,2)==0&&held(*e,3)==1,p.name+": member All Sound Off preserves the neighboring member");render(*e,.025);
 e->controlChange(123,0,3);double expressionRelease=retire(*e,p.name+" expression");
 std::printf("RESULT %s overlap_release %.3f excess_release %.3f expression_release %.3f panic_drain_seconds %.9f pipeline_rms %.12g settled_rms %.12g\n",p.name.c_str(),overlapRelease,excessRelease,expressionRelease,drain,panic.rms(),settled.rms());std::fflush(stdout);
}
}
int main(int argc,char**argv){try{bool negative=argc>1&&std::string(argv[1])=="--omit-first-pedal-up";bool first=true;for(const char* n:{"Tubular Bells","Gong","Temple Bell","Agogo","Music Box","Banjo","Koto","Harp","Kalimba"}){run(preset(n),negative&&first);first=false;}}catch(const std::exception&e){check(false,e.what());}std::printf("Lifecycle: %d checks, %d failures; %.3f seconds; peak %.12g\n",checks,failures,totalSeconds,maximumPeak);return failures?1:0;}

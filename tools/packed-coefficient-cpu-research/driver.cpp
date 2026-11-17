// DRAFT, no timing execution authorized. Frozen stock frame-builder copied verbatim below.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
using namespace cs80;
namespace fs=std::filesystem;
constexpr int rate=48000,block=256,warmCallbacks=375,callbacks=11250;
constexpr std::array<int,8> notes {36,43,48,55,60,67,72,79};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Case {
    int rate = 48000, block = 128, selector = 0;
    bool isolated = true, moving = false, idle = false, noise = false;
    bool sourcePerformance = false;
    int performanceProfile = 0; // 0 dry, 1 MPE+chorus/reverb, 2 MPE+phaser/delay/drive
};

ControlFrame makeFrame(const Case& c, int selector) {
    ControlFrame frame;
    auto& p = frame.effectiveValues;
    p[P_parityMode] = c.isolated ? 1.0f : 0.0f;
    p[P_sourcePerformance] = c.sourcePerformance ? 1.0f : 0.0f;
    p[P_mix] = 0.5f;
    p[P_volume] = 0.12f; // fixed headroom, never per-tone normalization
    p[P_masterSustain] = p[P_keyRelease] = p[P_sustainMode] = 0.0f;
    p[P_chorusOn] = p[P_ringAmount] = 0.0f;
    p[P_subOriginalMode] = 1.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    p[P_subAtVco] = p[P_subAtVcf] = p[P_subAtSpeed] = p[P_subExtLevel] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = 0.0f;
    p[P_analogNoise] = c.noise ? 0.12f : 0.0f;
    p[P_lfo1Depth] = p[P_lfo2Depth] = p[P_lfo3Depth] = 0.0f;
    p[P_fx1Type] = p[P_fx2Type] = p[P_fx3Type] = 0.0f;
    p[P_fx1Mix] = p[P_fx2Mix] = p[P_fx3Mix] = 0.0f;
    p[P_mpeOn] = p[P_arpOn] = 0.0f;
    p[P_footMode] = c.moving ? 1.0f : 0.0f;
    p[P_outputLevel] = p[P_outputMode] = 1.0f;
    if (c.performanceProfile != 0) {
        p[P_mpeOn] = 1.0f;
        p[P_mpeBendRange] = 12.0f;
        p[P_pitchBendRange] = 2.0f;
        p[P_footMode] = 1.0f;
        p[P_modWheelSub] = 0.15f;
        p[P_modWheelVcf] = 0.20f;
        if (c.performanceProfile == 1) {
            p[P_fx1Type] = 1.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 4.0f; p[P_fx2Mix] = 0.15f;
        } else {
            p[P_fx1Type] = 2.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 3.0f; p[P_fx2Mix] = 0.15f;
            p[P_fx3Type] = 5.0f; p[P_fx3Mix] = 0.10f;
        }
    }
    for (int rank = 0; rank < 2; ++rank) {
        frame.hardwareSources[rank] = selector;
        ParamValues legacy;
        applyHardwareTone(legacy, rank, selector);
        // Match the processor's physical selector adapter. Footage, pan and
        // software bypasses are not among the 26 replaced programmer controls.
        for (int local = 0; local < CP_count; ++local)
            if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn)
                p[chParam(rank, local)] = legacy[chParam(rank, local)];
        p[chParam(rank, CP_feet)] = 1.0f;
        p[chParam(rank, CP_pan)] = 0.0f;
        p[chParam(rank, CP_hpfOn)] = p[chParam(rank, CP_lpfOn)] = 1.0f;
    }
    return frame;
}

struct Event { bool on=false,off=false,pressure=false; float value=0; };
std::array<Event,callbacks> events(int row){std::array<Event,callbacks> result{};for(int k=0;k<callbacks;++k){if(row==4){int phase=k%750;result[k].pressure=true;result[k].value=float(phase<=375?phase:750-phase)/375.0f;}if(row==5){result[k].on=k%1125==0;result[k].off=k%1125==375;}}return result;}
std::uint64_t cpuNs(){timespec t{};require(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"thread CPU clock failed");return std::uint64_t(t.tv_sec)*1000000000ull+std::uint64_t(t.tv_nsec);}
struct ScopedFp {
#if defined(__SSE__)
 unsigned before=_mm_getcsr();ScopedFp(){_mm_setcsr(before|0x8040u);}~ScopedFp(){_mm_setcsr(before);}unsigned current()const{return _mm_getcsr();}
#else
 unsigned before=0;unsigned current()const{return 0;}
#endif
};
int main(int argc,char**argv){
 if(argc!=5){std::cerr<<"usage: driver events|dry|timed stock|candidate ROW NEW_OUT\n";return 2;}
 fs::path out=argv[4];bool owned=false;try{
  std::string phase=argv[1],arm=argv[2];require(phase=="events"||phase=="dry"||phase=="timed","invalid phase");require(arm=="stock"||arm=="candidate","invalid arm");int row=std::stoi(argv[3]);require(row>=0&&row<7,"invalid row");require(!fs::exists(out),"output exists");require(fs::create_directories(out),"could not create fresh output");owned=true;
  const auto table=events(row);std::uint64_t noteons=0,noteoffs=0,pressures=0;
  {std::ofstream f(out/"events.csv");f<<"callback,frame,noteons,noteoffs,pressure_event,pressure_bits\n";for(int k=0;k<callbacks;++k){const auto&e=table[k];noteons+=e.on*8;noteoffs+=e.off*8;pressures+=e.pressure;f<<k<<','<<k*block<<','<<e.on*8<<','<<e.off*8<<','<<e.pressure<<','<<std::bit_cast<std::uint32_t>(e.value)<<'\n';}require(bool(f),"event output failed");}
  require(noteons==(row==5?80:0)&&noteoffs==(row==5?80:0)&&pressures==(row==4?11250:0),"event counts differ");
  if(phase=="events"){std::ofstream(out/"events-complete.txt")<<"11250 callbacks, 2880000 frames, no renderer or clock called\n";return 0;}
  ScopedFp fp;const auto initialMxcsr=fp.current();require((initialMxcsr & ~0x3fu)==0x9fc0u,"unsupported FP control state");auto engine=std::make_unique<Engine>();
  Case c;c.rate=rate;c.block=block;c.isolated=false;c.sourcePerformance=row!=6;c.selector=std::array<int,7>{0,4,7,10,4,4,4}[row];
  const auto frame=makeFrame(c,c.selector);require(engine->setControlFrame(frame),"initial frame rejected");engine->setNonRealtime(false);engine->prepare(rate);engine->reset();engine->setExpression(0.8f);
  require(engine->getOversamplingFactor()==2,"wrong factor");require(engine->getRenderPolicy()==(row==6?RenderPolicy::legacy:RenderPolicy::sourcePerformance),"wrong policy");
  if(row!=6)for(int rank=0;rank<2;++rank){auto info=engine->getSourceToneInfo(rank);require(info.selector==c.selector&&info.route==source_factory::route(rank,c.selector,true),"wrong source route");}
  {std::ofstream f(out/"parameters.csv");f<<"index,id,float_bits\n";for(int i=0;i<P_numParams;++i)f<<i<<','<<paramId(i)<<','<<std::bit_cast<std::uint32_t>(frame.effectiveValues[i])<<'\n';require(bool(f),"parameter output failed");}
  std::array<float,block*2> audio{};if(row!=5)for(int n:notes)engine->noteOn(n,0.8f,1);
  for(int k=0;k<warmCallbacks;++k){require(engine->setControlFrame(frame),"warmup rejected");engine->render(audio.data(),block);for(float x:audio)require(std::isfinite(x),"nonfinite warmup");}
  require(engine->activeVoices()==(row==5?0:8),"wrong warmup voice count");
  struct Sample{std::uint64_t ns=0,hash=0;int voices=0;};std::array<Sample,callbacks> samples{};std::uint64_t total=0;double peak=0,sum=0;
  std::vector<float> retainedAudio(callbacks*block*2); // allocated before timing; write once after allcallbacks
  for(int k=0;k<callbacks;++k){const auto&e=table[k];std::uint64_t before=0,after=0;if(phase=="timed")before=cpuNs();
   bool accepted=engine->setControlFrame(frame);if(e.on)for(int n:notes)engine->noteOn(n,0.8f,1);if(e.off)for(int n:notes)engine->noteOff(n,1);if(e.pressure)engine->channelPressure(1,e.value);engine->render(audio.data(),block);
   if(phase=="timed")after=cpuNs();require(accepted,"measured frame rejected");require(after>=before,"CPU timer went backwards");samples[k].ns=after-before;total+=samples[k].ns;samples[k].voices=engine->activeVoices();
   std::uint64_t hash=14695981039346656037ull;for(float x:audio){require(std::isfinite(x),"nonfinite output");auto bits=std::bit_cast<std::uint32_t>(x);for(int b=0;b<4;++b){hash^=(bits>>(8*b))&255u;hash*=1099511628211ull;}peak=std::max(peak,double(std::abs(x)));sum+=double(x)*x;}samples[k].hash=hash;
   std::copy(audio.begin(),audio.end(),retainedAudio.begin()+k*block*2);
  }
  {std::ofstream pcm(out/"audio.f32",std::ios::binary);pcm.write(reinterpret_cast<const char*>(retainedAudio.data()),retainedAudio.size()*sizeof(float));require(bool(pcm),"PCM write failed");}
  require((phase=="timed"&&total>0)||(phase=="dry"&&total==0),"timer scope failed");
  {std::ofstream f(out/"callbacks.csv");f<<"callback,cpu_ns,output_fnv64,active_voices\n";for(int k=0;k<callbacks;++k)f<<k<<','<<samples[k].ns<<','<<samples[k].hash<<','<<samples[k].voices<<'\n';require(bool(f),"callback output failed");}
  std::ofstream f(out/"result.json");f<<std::setprecision(17)<<"{\"phase\":\""<<phase<<"\",\"arm\":\""<<arm<<"\",\"row\":"<<row<<",\"selector\":"<<c.selector<<",\"factor\":"<<engine->getOversamplingFactor()<<",\"latency\":"<<engine->getLatencySamples()<<",\"render_policy\":"<<static_cast<int>(engine->getRenderPolicy())<<",\"rate\":48000,\"block\":256,\"batch_kernel\":"<<(engine->hasFilterBatchingKernel()?"true":"false")<<",\"callbacks\":11250,\"frames\":2880000,\"warmup_callbacks\":375,\"warmup_frames\":96000,\"measured_noteons\":"<<noteons<<",\"measured_noteoffs\":"<<noteoffs<<",\"pressure_events\":"<<pressures<<",\"cpu_ns\":"<<total<<",\"peak\":"<<peak<<",\"rms\":"<<std::sqrt(sum/(callbacks*block*2))<<",\"mxcsr_initial\":"<<initialMxcsr<<",\"mxcsr_final\":"<<fp.current()<<",\"finite\":true}\n";require(bool(f),"result output failed");return 0;
 }catch(const std::exception&e){if(owned&&fs::is_directory(out))std::ofstream(out/"FAILED.txt")<<e.what()<<'\n';std::cerr<<e.what()<<'\n';return 1;}
}

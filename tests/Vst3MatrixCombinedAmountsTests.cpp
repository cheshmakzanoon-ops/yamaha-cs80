// Bounded simultaneous sixteen matrix amount histories and queue permutations.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen108Roster.h"
#include <fstream>
namespace {
using NamedCurves = std::vector<std::pair<int, Curve>>;
int renderCount = 0;
std::string fileBytes(const std::filesystem::path& path)
{ std::ifstream f(path,std::ios::binary); require(bool(f), "state input open"); return {std::istreambuf_iterator<char>(f), {}}; }
void saveBytes(const std::filesystem::path& path,const std::string& bytes)
{ std::ofstream f(path,std::ios::binary); require(bool(f),"output open"); f.write(bytes.data(),std::streamsize(bytes.size()));require(bool(f),"output complete"); }
struct SessionInstance {
    IComponent* component=nullptr; IAudioProcessor* audio=nullptr;
    SessionInstance(Module& module,const cs80::ParamValues& p,const std::string& state) {
        require(module.factory->createInstance(module.componentId,IComponent::iid,reinterpret_cast<void**>(&component))==kResultOk,"component creation");
        require(component->initialize(nullptr)==kResultOk,"component initialize");
        require(component->queryInterface(IAudioProcessor::iid,reinterpret_cast<void**>(&audio))==kResultOk,"audio interface");
        ProcessSetup setup {testProcessMode,kSample32,1024,testSampleRate};
        require(audio->setupProcessing(setup)==kResultOk,"setup processing");
        require(component->activateBus(kAudio,kOutput,0,true)==kResultOk,"output bus");
        require(component->activateBus(kEvent,kInput,0,true)==kResultOk,"event bus");
        StateCapture stream; stream.bytes=state;
        require(component->setState(&stream)==kResultOk,"load original canonical serializer bytes before activation");
        Changes initial;
        for(int i=0;i<cs80::P_numParams;++i) initial.add(parameterId(cs80::paramId(i)),{{0,normalized(i,p[i])}});
        for(int rank=0;rank<2;++rank)initial.add(parameterId("hardware_source_"+std::to_string(rank+1)),{{0,1.0}});
        ProcessData flush {};flush.processMode=testProcessMode;flush.symbolicSampleSize=kSample32;flush.inputParameterChanges=&initial;
        require(audio->process(flush)==kResultOk,"ordinary initial parameter flush");
        require(component->setActive(true)==kResultOk,"activate");
        require(audio->setProcessing(true)==kResultOk,"start processing");
    }
    ~SessionInstance(){ if(audio)audio->setProcessing(false);if(component){component->setActive(false);component->terminate();}if(audio)audio->release();if(component)component->release(); }
    std::string readback(){StateCapture state;require(component->getState(&state)==kResultOk,"ordinary native config readback");return state.bytes;}
};
// Fixture-local long ordinary note history; historical Instance::process is unchanged.
void processHeld(SessionInstance& instance, int start, int count, Changes* changes, std::vector<float>& output)
{
    require(start >= 0 && count > 0 && count <= 1024 && start + count <= 16384, "bounded long process");
    std::vector<float> left(size_t(count), 0.f), right(size_t(count), 0.f);
    float* channels[] {left.data(), right.data()};
    AudioBusBuffers bus {}; bus.numChannels = 2; bus.channelBuffers32 = channels;
    Events events;
    for (int key = 0; key < 2; ++key) {
        for (bool on : {true, false}) {
            const int absolute = (on ? 17 : 14336) + key;
            if (absolute < start || absolute >= start + count) continue;
            Event event {}; event.busIndex = 0; event.sampleOffset = absolute - start;
            event.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
            if (on) { event.noteOn.channel = 0; event.noteOn.pitch = int16(48 + key);
                event.noteOn.velocity = .75f; event.noteOn.noteId = 48 + key; }
            else { event.noteOff.channel = 0; event.noteOff.pitch = int16(48 + key);
                event.noteOff.velocity = 0.f; event.noteOff.noteId = 48 + key; }
            events.events.push_back(event);
        }
    }
    std::stable_sort(events.events.begin(), events.events.end(),
        [](const Event& a, const Event& b) { return a.sampleOffset < b.sampleOffset; });
    ProcessContext context {}; context.sampleRate = testSampleRate; context.projectTimeSamples = start;
    ProcessData data {}; data.processMode = testProcessMode; data.symbolicSampleSize = kSample32;
    data.numSamples = count; data.numOutputs = 1; data.outputs = &bus;
    data.inputEvents = &events; data.inputParameterChanges = changes; data.processContext = &context;
    require(instance.audio->process(data) == kResultOk, "long ordinary VST3 processing");
    for (int i = 0; i < count; ++i) { output.push_back(left[size_t(i)]); output.push_back(right[size_t(i)]); }
}


std::vector<float> renderSession(Module& module,const cs80::ParamValues& p,const std::string& state,
                                const NamedCurves& curves,int block,bool flatten=false,bool nullChanges=false,std::string* readback=nullptr)
{
    ++renderCount; SessionInstance instance(module,p,state);
    if(readback)*readback=instance.readback();
    std::vector<float> audio; audio.reserve(32768);
    for(int start=0;start<16384;start+=block) {
        const int count=std::min(block,16384-start); Changes changes;
        for(const auto& [parameter,curve]:curves) {
            const double initial=normalized(parameter,p[parameter]); Curve points;
            if(!flatten) for(const auto& point:curve)
                if(point.first>=start && point.first<start+count)points.emplace_back(point.first-start,point.second);
            if(points.empty() || points.back().first!=count-1)
                points.emplace_back(flatten?0:count-1,absoluteValue(curve,initial,start+count-1));
            changes.add(parameterId(cs80::paramId(parameter)),std::move(points));
        }
        processHeld(instance,start,count,nullChanges?nullptr:&changes,audio);
    }
    (void)difference(audio,audio);require(audio.size()==32768,"complete stereo capture");return audio;
}
bool exactAudio(const std::vector<float>& a,const std::vector<float>& b)
{(void)difference(a,b);return std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;}
void capture(const std::filesystem::path& path,const std::vector<float>& audio)
{(void)difference(audio,audio);saveBytes(path,std::string(reinterpret_cast<const char*>(audio.data()),audio.size()*sizeof(float)));}
}
int main(int argc,char**argv)
{
 try {
    require(argc==8,"MODULE CANONICAL OUTPUT HOST_CONTRACT CONTRACT RATE MODE");
    const std::string rate(argv[6]),mode(argv[7]);
    require(rate=="44100" || rate=="48000" || rate=="96000","supported rate");
    require(mode=="realtime" || mode=="offline","supported mode");
    testSampleRate=std::stod(rate);testProcessMode=mode=="offline"?kOffline:kRealtime;
    const ws1::Tolerances host(argv[4],"host_automation"),limits(argv[5],"matrix_combined_amounts");
    require(cs80::kSampleAccurateParameters.size()==108,"unchanged108 roster");
    require(std::equal(cs80::kFrozen108Roster.begin(),cs80::kFrozen108Roster.end(),cs80::kSampleAccurateParameters.begin()),"literal108 roster");
    const std::filesystem::path input(argv[2]),out(argv[3]);
    require(!std::filesystem::exists(out),"fresh captures");std::filesystem::create_directory(out);
    Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_kbdLevelLow] = 0;
        parameters[cs80::P_kbdLevelHigh] = 0;
        parameters[cs80::P_kbdBrillianceLow] = parameters[cs80::P_kbdBrillianceHigh] = 0;
        parameters[cs80::P_portamento] = 0;
        parameters[cs80::P_initBend] = 0;
        parameters[cs80::P_ribbonCcEnable] = 0;
        parameters[cs80::P_sustainMode] = 0;
        parameters[cs80::P_glideMode] = 0;
        parameters[cs80::P_glideAssign] = 0;
        parameters[cs80::P_footswitchPlugged] = parameters[cs80::P_footswitchPressed] = 0;
        parameters[cs80::P_sustainAssign] = 0;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_lfo1Depth] = parameters[cs80::P_lfo2Depth] = parameters[cs80::P_lfo3Depth] = 0;
        for (int i = cs80::P_mod1Amount; i <= cs80::P_mod16Amount; ++i) parameters[i] = 0;
        parameters[cs80::P_mix] = .5f;
        parameters[cs80::P_masterSustain] = 0;
        parameters[cs80::P_keyRelease] = .5f;
        parameters[cs80::P_ribbon] = 0;
        // Neutral motion macro above yields Engine envTimeScale == 1; dispersion is zero.
        parameters[cs80::P_mpeOn] = parameters[cs80::P_pressureCurve] = parameters[cs80::P_pressureThreshold] = 0;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            parameters[cs80::chParam(rank, cs80::CP_vA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_vD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_vS)] = .7f;
            parameters[cs80::chParam(rank, cs80::CP_vR)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_fA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_fD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_fAl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fIl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .30f;
            parameters[cs80::chParam(rank, cs80::CP_lpfCutoff)] = 4000;
            parameters[cs80::chParam(rank, cs80::CP_hpfCutoff)] = 25;
            parameters[cs80::chParam(rank, cs80::CP_lpfRes)] = parameters[cs80::chParam(rank, cs80::CP_hpfRes)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_vcfLevel)] = .78f;
            parameters[cs80::chParam(rank, cs80::CP_pulse)] = 1;
            parameters[cs80::chParam(rank, cs80::CP_saw)] = parameters[cs80::chParam(rank, cs80::CP_sine)] = parameters[cs80::chParam(rank, cs80::CP_noise)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_pw)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_pwmSpeed)] = 3;
            parameters[cs80::chParam(rank, cs80::CP_pwmDepth)] = 0;
            for (int local : {cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel})
                parameters[cs80::chParam(rank, local)] = 0;
        }

        // Pin deterministic compatibility SUB oscillator: sine=0, 8 Hz, no external input.
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 8;
        parameters[cs80::P_subExtLevel] = 0;
        parameters[cs80::P_subVco] = parameters[cs80::P_subVcf] = parameters[cs80::P_subVca] = 0;
        parameters[cs80::P_subAtSpeed] = parameters[cs80::P_subAtVco] = parameters[cs80::P_subAtVcf] = 0;

        // Explicitly neutralize all other modulation and effects.
        parameters[cs80::P_modWheelSub] = parameters[cs80::P_modWheelVcf] = 0;
        parameters[cs80::P_ringAmount] = parameters[cs80::P_chorusOn] = 0;
        parameters[cs80::P_fx1Mix] = parameters[cs80::P_fx2Mix] = parameters[cs80::P_fx3Mix] = 0;
        parameters[cs80::P_fx1Type] = parameters[cs80::P_fx2Type] = parameters[cs80::P_fx3Type] = 0;
        parameters[cs80::P_velocityCurve] = 0;
        parameters[cs80::P_lfo1Rate] = 18; parameters[cs80::P_lfo2Rate] = 19; parameters[cs80::P_lfo3Rate] = 20;
        parameters[cs80::P_lfo1Wave] = parameters[cs80::P_lfo2Wave] = parameters[cs80::P_lfo3Wave] = 0;

        for(int i=0;i<3;++i)parameters[cs80::P_mseg1Rate+i]=0.f;


    const auto on=fileBytes(input/"all-on.state"),off=fileBytes(input/"all-off.state");
    auto p=parameters;p[cs80::P_voiceMode]=0.f;NamedCurves curves,constant;
    for(int i=0;i<16;++i){
      const int lane=cs80::P_mod1Amount+i;p[lane]=.02f;const double initial=normalized(lane,.02f);
      const float variation=.001f*float(i%4);
      curves.push_back({lane,{{0,initial},{1023,initial},{2047+17*i,normalized(lane,.04f-variation)},
        {4095+17*i,normalized(lane,-.03f+variation)},{8191+17*i,normalized(lane,.045f-variation)},
        {12287+17*i,normalized(lane,-.035f+variation)},{16383,initial}}});
      constant.push_back({lane,{{0,initial},{16383,initial}}});
    }
    int captures=0;
    auto save=[&](const std::string& name,const std::vector<float>& audio){capture(out/(name+".pcm"),audio);++captures;};
    std::string readback;const auto scalar=renderSession(module,p,on,curves,1,false,false,&readback);
    saveBytes(out/"all-on.state",readback);save("scalar",scalar);
    auto reversed=curves;std::reverse(reversed.begin(),reversed.end());
    auto rotated=curves;std::rotate(rotated.begin(),rotated.begin()+5,rotated.end());
    double maxError=0;
    for(int block:{7,64,256,1024}){
      const auto forward=renderSession(module,p,on,curves,block);
      const auto reverse=renderSession(module,p,on,reversed,block);
      const auto rotate=renderSession(module,p,on,rotated,block);
      const double error=difference(scalar,forward);maxError=std::max(maxError,error);
      check(error<=host["audio_max_abs_error"],"combined queues match scalar");
      check(exactAudio(forward,reverse),"reversed parameter queue order byte-identical");
      check(exactAudio(forward,rotate),"rotated parameter queue order byte-identical");
      save("block"+std::to_string(block),forward);save("reverse"+std::to_string(block),reverse);
      save("rotate"+std::to_string(block),rotate);
    }
    const auto fixed=renderSession(module,p,on,constant,1);save("constant",fixed);
    check(std::memcmp(scalar.data(),fixed.data(),2048*sizeof(float))==0,"initial1024frame exact hold");
    for(int lane=0;lane<16;++lane){
      auto disabled=curves;disabled[size_t(lane)]=constant[size_t(lane)];
      const auto witness=renderSession(module,p,on,disabled,1);save("fixed-lane"+std::to_string(lane),witness);
      const double effect=difference({scalar.begin()+2048,scalar.begin()+28672},{witness.begin()+2048,witness.begin()+28672});
      check(effect>limits["minimum_held_effect"],"independent per-lane motion audible");
      std::cout<<"lane="<<lane<<" held_effect="<<effect<<"\n";
    }
    const auto offAudio=renderSession(module,p,off,curves,256,false,false,&readback);
    saveBytes(out/"all-off.state",readback);const auto offFixed=renderSession(module,p,off,constant,256);
    check(exactAudio(offAudio,offFixed),"unrouted amounts inert");save("off",offAudio);save("off-fixed",offFixed);
    auto zero=p;NamedCurves zeroConstant;
    for(int i=0;i<16;++i){const int lane=cs80::P_mod1Amount+i;zero[lane]=0.f;
      zeroConstant.push_back({lane,{{0,normalized(lane,0.f)},{16383,normalized(lane,0.f)}}});}
    const auto zeroAudio=renderSession(module,zero,on,zeroConstant,256),zeroOff=renderSession(module,zero,off,zeroConstant,256);
    check(exactAudio(zeroAudio,zeroOff),"zero-static routed amounts equal no routes");save("zero-routed",zeroAudio);save("zero-off",zeroOff);
    auto pure=p;pure[cs80::P_parityMode]=1;
    const auto pureAudio=renderSession(module,pure,on,curves,256),pureFixed=renderSession(module,pure,on,constant,256);
    check(exactAudio(pureAudio,pureFixed),"PURE amounts inert");save("pure",pureAudio);save("pure-fixed",pureFixed);
    std::cout<<"max_error="<<maxError<<"\n";
    std::cout<<"cases=1 renders="<<renderCount<<" captures="<<captures<<" readbacks=2 failures="<<failures<<"\n";
    return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}

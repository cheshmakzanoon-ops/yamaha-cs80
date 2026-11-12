// Bounded ordinary canonical-session MSEG rate automation qualification.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen89Roster.h"
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
    require(argc==8 || argc==9,"MODULE CANONICAL OUTPUT HOST_CONTRACT RATE_CONTRACT RATE MODE [--compat-only]");
    const bool compat=argc==9;if(compat)require(std::string(argv[8])=="--compat-only","explicit compatibility mode");
    const std::string rate(argv[6]),mode(argv[7]);
    require(rate=="44100" || rate=="48000" || rate=="96000","supported rate");
    require(mode=="realtime" || mode=="offline","supported mode");
    testSampleRate=std::stod(rate);testProcessMode=mode=="offline"?kOffline:kRealtime;
    const ws1::Tolerances host(argv[4],"host_automation"),limits(argv[5],"host_automation_mseg_rate");
    require(cs80::kSampleAccurateParameters.size()>=89,"89 prefix available");
    require(std::equal(cs80::kFrozen89Roster.begin(),cs80::kFrozen89Roster.end(),cs80::kSampleAccurateParameters.begin()),"literal89 prefix preserved");
    constexpr std::array rates{int(cs80::P_mseg1Rate),int(cs80::P_mseg2Rate),int(cs80::P_mseg3Rate)};
    const std::array<std::string,3> ids{"mseg1_rate","mseg2_rate","mseg3_rate"};
    for(size_t i=0;i<rates.size();++i)require(cs80::paramId(rates[i])==ids[i],"stable MSEG rate IDs");
    require(limits["key_parameter_count"]==92,"92 lane contract");
    if(!compat){require(cs80::kSampleAccurateParameters.size()==92,"exact92 capability");
      require(std::equal(rates.begin(),rates.end(),cs80::kSampleAccurateParameters.begin()+89),"rates at89/90/91");}
    std::cout<<"compiled_lanes="<<cs80::kSampleAccurateParameters.size()<<"\n";
    const std::filesystem::path input(argv[2]),out(argv[3]);
    require(!std::filesystem::exists(out),"fresh capture directory");std::filesystem::create_directory(out);
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

    int scenarios=0;
    for(int route=0;route<3;++route){
      const auto on=fileBytes(input/("route"+std::to_string(route)+"-on.state"));
      const auto off=fileBytes(input/("route"+std::to_string(route)+"-off.state"));
      auto p=parameters;p[cs80::P_mod1Amount+route]=.15f;
      const int parameter=rates[size_t(route)];const double initial=normalized(parameter,0.f);
      const NamedCurves curves{{parameter,{{0,initial},{1023,initial},{2047,normalized(parameter,1.f)},
        {4095,normalized(parameter,-1.f)},{8191,normalized(parameter,2.f)},{12287,normalized(parameter,-2.f)},{16383,initial}}}};
      const NamedCurves constant{{parameter,{{0,initial},{16383,initial}}}};
      const std::string id="mseg-route"+std::to_string(route);
      std::string readback;const auto neutral=renderSession(module,p,on,{},256,false,false,&readback);
      saveBytes(out/(id+".state"),readback);
      check(exactAudio(neutral,renderSession(module,p,on,{},256,false,true)),"empty/null noqueue identity");
      capture(out/(id+"-noqueue.pcm"),neutral);
      NamedCurves preceding;
      for(int prior:{int(cs80::P_volume),int(cs80::P_masterTune),int(cs80::P_tuningRef),int(cs80::P_lfo1Rate)}){
        const double v=normalized(prior,p[prior]);preceding.push_back({prior,{{0,v},{1023,v},{4095,.4},{16383,.6}}});}
      capture(out/(id+"-preceding89.pcm"),renderSession(module,p,on,preceding,127));
      auto legacy=p;legacy[cs80::P_sourcePerformance]=0;
      const auto old=renderSession(module,legacy,on,curves,256);
      check(exactAudio(old,renderSession(module,legacy,on,curves,256,true)),"Legacy final-point flattening");
      capture(out/(id+"-legacy.pcm"),old);
      auto pure=p;pure[cs80::P_parityMode]=1;
      const auto pureAudio=renderSession(module,pure,on,curves,256);
      check(exactAudio(pureAudio,renderSession(module,pure,on,constant,256)),"PURE rate inert");
      capture(out/(id+"-pure.pcm"),pureAudio);
      if(!compat){
        const auto scalar=renderSession(module,p,on,curves,1);double error=0;
        for(int block:{7,64,256,1024})error=std::max(error,difference(scalar,renderSession(module,p,on,curves,block)));
        check(error<=host["audio_max_abs_error"],"rate queues match scalar reference");
        const auto fixed=renderSession(module,p,on,constant,1);
        check(std::memcmp(scalar.data(),fixed.data(),2048*sizeof(float))==0,"initial1024-frame bytehold");
        const double effect=difference({scalar.begin()+2048,scalar.begin()+28672},{fixed.begin()+2048,fixed.begin()+28672});
        check(effect>limits["minimum_held_effect"],"positive rate effect on held notes");
        check(exactAudio(renderSession(module,p,off,curves,256),renderSession(module,p,off,constant,256)),"unrouted rate inert");
        auto zero=p;zero[cs80::P_mod1Amount+route]=0;
        check(exactAudio(renderSession(module,zero,on,curves,256),renderSession(module,zero,on,constant,256)),"zero amount rate inert");
        std::cout<<id<<" notes=2 max_error="<<error<<" held_effect="<<effect<<"\n";
      }
      ++scenarios;
    }
    std::cout<<"scenarios="<<scenarios<<" renders="<<renderCount<<" captures="<<scenarios*4
             <<" failures="<<failures<<" mode="<<(compat?"compat-only":"full")<<"\n";
    return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}

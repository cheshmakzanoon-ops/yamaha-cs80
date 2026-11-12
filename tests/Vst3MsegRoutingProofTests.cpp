// Narrow ordinary canonical-session routing proof; no automation roster change.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen89Roster.h"
#include <fstream>
namespace {
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
        ProcessSetup setup {testProcessMode,kSample32,256,testSampleRate};
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
    require(start >= 0 && count > 0 && count <= 256 && start + count <= 16384, "bounded long process");
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
                                const std::filesystem::path& prefix)
{
    SessionInstance instance(module,p,state); saveBytes(prefix.string()+".state",instance.readback());
    std::vector<float> audio; audio.reserve(32768);
    for(int start=0;start<16384;start+=128)processHeld(instance,start,128,nullptr,audio);
    (void)difference(audio,audio);require(audio.size()==32768,"complete stereo capture");
    saveBytes(prefix.string()+".pcm",std::string(reinterpret_cast<const char*>(audio.data()),audio.size()*sizeof(float)));
    return audio;
}
bool exactAudio(const std::vector<float>& a,const std::vector<float>& b)
{(void)difference(a,b);return std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;}
}
int main(int argc,char**argv)
{
    try {
        require(argc==5,"MODULE CANONICAL_DIRECTORY OUTPUT CONTRACT");
        const ws1::Tolerances limits(argv[4],"mseg_native_routing_proof");
        require(cs80::kSampleAccurateParameters.size() >= 89, "89-lane prefix available");
            require(std::equal(cs80::kFrozen89Roster.begin(), cs80::kFrozen89Roster.end(),
                               cs80::kSampleAccurateParameters.begin()), "literal89 prefix unchanged");
        testSampleRate=48000;testProcessMode=kRealtime;
        const std::filesystem::path input(argv[2]), out(argv[3]);
        require(!std::filesystem::exists(out),"fresh native output");std::filesystem::create_directory(out);
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
        for(int route=0;route<3;++route){
            const auto on=fileBytes(input/("route"+std::to_string(route)+"-on.state"));
            const auto off=fileBytes(input/("route"+std::to_string(route)+"-off.state"));
            auto p=parameters;p[cs80::P_mod1Amount+route]=.15f;
            auto zero=p;zero[cs80::P_mod1Amount+route]=0.f;
            auto pure=p;pure[cs80::P_parityMode]=1.f;
            const std::string id="route"+std::to_string(route);
            const auto active=renderSession(module,p,on,out/(id+"-active"));
            const auto zeroAudio=renderSession(module,zero,on,out/(id+"-zero"));
            const auto offAudio=renderSession(module,p,off,out/(id+"-off"));
            const auto pureAudio=renderSession(module,pure,on,out/(id+"-pure"));
            const auto pureOff=renderSession(module,pure,off,out/(id+"-pure-off"));
            check(exactAudio(zeroAudio,offAudio),"zero amount equals no route");
            check(exactAudio(pureAudio,pureOff),"PURE configured route inert");
            const double effect=difference({active.begin()+2048,active.begin()+28672},{zeroAudio.begin()+2048,zeroAudio.begin()+28672});
            check(effect>limits["minimum_held_effect"],"configured MSEG route audible");
            std::cout<<id<<" effect="<<effect<<"\n";
        }
        std::cout<<"routes=3 renders=15 captures=15 readbacks=15 failures="<<failures<<"\n";
        return failures?1:0;
    }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}

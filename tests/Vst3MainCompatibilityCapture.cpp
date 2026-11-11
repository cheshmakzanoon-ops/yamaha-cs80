// Exact-main compatibility capture. Run unchanged against the clean base and
// candidate modules, then compare complete binary outputs byte-for-byte.
// Supported-key trajectories in modern policy are deliberately not a baseline
// equality case: preserving their offsets is the new feature being qualified.
#define main reviewed_automation_gate_main
#include "Vst3AutomationTests.cpp"
#undef main
#include <fstream>

namespace {
struct MainRecall { int bank, sample; };
void processMainCapture(Instance& instance, int start, int count, Changes* changes,
                        std::vector<float>& output, int voices, MainRecall recall)
{
    std::vector<float> left(count), right(count);
    float* channels[]{left.data(), right.data()};
    AudioBusBuffers bus{}; bus.numChannels=2; bus.channelBuffers32=channels;
    Events events;
    const auto cc=[&](int at, int control, int value) {
        if(at<start || at>=start+count) return;
        Event e{}; e.busIndex=0; e.sampleOffset=at-start; e.type=Event::kLegacyMIDICCOutEvent;
        e.midiCCOut.channel=0; e.midiCCOut.controlNumber=static_cast<uint8>(control);
        e.midiCCOut.value=static_cast<int8>(value); events.events.push_back(e);
    };
    if(recall.bank>=0) {
        cc(recall.sample,0,recall.bank); cc(recall.sample,32,0);
        cc(recall.sample,kCtrlProgramChange,0);
    }
    const auto note=[&](int at, int key, bool on) {
        if(at<start || at>=start+count) return;
        Event e{}; e.busIndex=0; e.sampleOffset=at-start;
        e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(on) { e.noteOn.channel=0; e.noteOn.pitch=key; e.noteOn.velocity=.75f; e.noteOn.noteId=key; }
        else { e.noteOff.channel=0; e.noteOff.pitch=key; e.noteOff.noteId=key; }
        events.events.push_back(e);
    };
    for(int n=0;n<voices;++n) { note(17+n,36+n,true); note(1536+n,36+n,false); }
    if(recall.bank>=0) { note(recall.sample,84,true); note(recall.sample+1,88,true); }
    note(1600,84,false); note(1601,88,false);
    std::stable_sort(events.events.begin(),events.events.end(),[](const auto& a,const auto& b){return a.sampleOffset<b.sampleOffset;});
    ProcessContext context{}; context.sampleRate=48000; context.projectTimeSamples=start;
    ProcessData data{}; data.processMode=kRealtime; data.symbolicSampleSize=kSample32;
    data.numSamples=count; data.numOutputs=1; data.outputs=&bus;
    data.inputEvents=&events; data.inputParameterChanges=changes; data.processContext=&context;
    require(instance.audio->process(data)==kResultOk,"main compatibility native process");
    for(int i=0;i<count;++i) {
        require(std::isfinite(left[i]) && std::isfinite(right[i]),"finite main compatibility audio");
        output.push_back(left[i]); output.push_back(right[i]);
    }
}
template<class T> void writeValue(std::ofstream& file,const T& value) {
    file.write(reinterpret_cast<const char*>(&value),sizeof(value));
    require(bool(file),"write main compatibility capture");
}
}
int main(int argc,char** argv) {
    try {
        require(argc==3,"usage: main-compatibility-capture MODULE OUTPUT");
        Module module(argv[1]); std::ofstream file(argv[2],std::ios::binary|std::ios::trunc);
        require(bool(file),"open main compatibility capture");
        const std::array<MainRecall,7> recalls{{{-1,64},{0,0},{0,64},{1,1},{1,64},{1,700},{3,64}}};
        int cases=0;
        for(int policy:{0,1,2}) for(int voices:{8,16}) for(int block:{64,256,1024})
            for(int kind:{0,1,2}) for(const auto recall:recalls) {
                if(kind==2 && policy==0) continue;
                auto p=cs80::defaultValues();
                p[cs80::P_sourcePerformance]=policy==1?0.f:1.f;
                p[cs80::P_parityMode]=policy==2?1.f:0.f;
                p[cs80::P_voiceMode]=voices==16?1.f:0.f;
                p[cs80::P_drift]=p[cs80::P_voiceDispersion]=p[cs80::P_analogNoise]=0.f;
                p[cs80::P_volume]=.11f;
                Instance instance(module,p); std::vector<float> output;
                for(int start=0;start<2048;start+=block) {
                    Changes changes;
                    if(start==0 && kind==1) {
                        const auto key=cs80::chParam(0,cs80::CP_vA);
                        changes.add(parameterId(cs80::paramId(key)),{{0,.2},{block-1,.8}});
                    }
                    if(start==0 && kind==2)
                        // Freeze this historical witness to its original eighteen
                        // lanes; new pan trajectories have a separate capability gate.
                        for(size_t lane=0;lane<18;++lane) {
                            const auto key=cs80::kSampleAccurateParameters[lane];
                            changes.add(parameterId(cs80::paramId(key)),{{0,.7},{block-1,.31+double(lane)*.008}});
                        }
                    processMainCapture(instance,start,block,start==0&&kind!=0?&changes:nullptr,output,voices,recall);
                }
                connectController(instance);
                StateCapture currentState;
                require(instance.component->getState(&currentState)==kResultOk,"capture complete main state");
                require(currentState.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"rewind complete main state");
                require(instance.controller->setComponentState(&currentState)==kResultOk,"refresh complete main controller state");
                const std::array<int,6> identity{{policy,voices,block,kind,recall.bank,recall.sample}};
                writeValue(file,identity);
                const std::uint64_t samples=output.size(); writeValue(file,samples);
                file.write(reinterpret_cast<const char*>(output.data()),static_cast<std::streamsize>(output.size()*sizeof(float)));
                const int32 parameterCount=instance.controller->getParameterCount();
                require(parameterCount>0,"main native parameter roster must be nonempty");
                writeValue(file,parameterCount);
                for(int32 index=0;index<parameterCount;++index) {
                    ParameterInfo info{};
                    require(instance.controller->getParameterInfo(index,info)==kResultOk,"complete main parameter metadata");
                    const double value=instance.controller->getParamNormalized(info.id);
                    require(std::isfinite(value) && value>=0.0 && value<=1.0,"complete main normalized state is finite and in range");
                    writeValue(file,info.id); writeValue(file,value);
                }
                require(bool(file),"complete main compatibility case");
                std::cout<<"CASE policy="<<policy<<" voices="<<voices<<" block="<<block<<" kind="<<kind<<" bank="<<recall.bank<<" sample="<<recall.sample<<'\n';
                ++cases;
            }
        require(cases==336,"fixed main compatibility case count");
        std::cout<<"SUMMARY cases="<<cases<<" samples_per_case=4096 acceptance=byte-identical-baseline\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 2; }
}

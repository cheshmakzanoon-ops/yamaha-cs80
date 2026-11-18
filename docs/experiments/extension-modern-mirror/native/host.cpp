// Genuine VST3 wrapper fixture. Every audio callback is IAudioProcessor::process.
// Reuses the repository's supported native SDK test scaffolding, not a processor cast.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "core/ModMatrix.h"
#include <cfenv>
#include <cerrno>
#include <fstream>
#include <sstream>
#include <xmmintrin.h>

namespace mirror_fixture {
struct FP { unsigned csr;unsigned short cw,sw;int error; };
FP fp(){FP f{};asm volatile("stmxcsr %0; fnstcw %1; fnstsw %2":"=m"(f.csr),"=m"(f.cw),"=m"(f.sw)::"memory");f.error=errno;return f;}
template<class T>void put(std::ofstream& f,const T& v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));require(bool(f),"complete output write");}
void putFP(std::ofstream& f,FP x){put(f,x.csr);put(f,x.cw);put(f,x.sw);put(f,x.error);}
void bytes(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f.write(s.data(),std::streamsize(s.size()));require(bool(f),"state write");}
std::string state(Instance& i){StateCapture s;require(i.component->getState(&s)==kResultOk,"real component getState");return s.bytes;}
std::string layer(int variant){
    using namespace cs80::modern;
    std::ostringstream x;
    x<<"<MODERN_LAYER version=\"1\">";
    for(int n=0;n<3;++n)x<<"<MSEG index=\""<<n<<"\" points=\"0 0 0;0.5 "<<(variant%2?"0.25":"0.75")<<" 0;1 0 0\" loopStart=\"0\" loopEnd=\"2\" loop=\"0\" trigger=\""<<(variant%3)<<"\" sync=\""<<(variant%2)<<"\" bipolar=\"0\"/>";
    x<<"<MATRIX>";
    for(int n=0;n<16;++n)x<<"<SLOT index=\""<<n<<"\" source=\""<<(n==0?int(variant%2?ModSource::velocity:ModSource::mseg1):0)<<"\" via=\"0\" destination=\""<<(n==0?int(variant%2?ModDestination::pitch:ModDestination::pulseWidth):0)<<"\" target=\""<<(n==0?variant%3:0)<<"\"/>";
    return x.str()+"</MATRIX></MODERN_LAYER>";
}
void edit(Instance& i,int variant,int block,const std::filesystem::path& out,std::ofstream& log){
    auto before=state(i),input=before;
    const auto close=input.find("</CS80_PARAMS>");require(close!=std::string::npos,"raw processor XML prefix in wrapper state");
    const auto begin=input.find("<MODERN_LAYER");
    if(begin!=std::string::npos){const auto end=input.find("</MODERN_LAYER>",begin);require(end!=std::string::npos&&end<close,"one complete existing modern subtree");input.erase(begin,end+15-begin);}
    input.insert(input.find("</CS80_PARAMS>"),layer(variant));
    StateCapture stream;stream.bytes=input;
    require(i.component->setState(&stream)==kResultOk,"real component setState");
    auto after=state(i);require(after.find("<MODERN_LAYER")!=std::string::npos,"side state published by real wrapper");
    const auto stem=std::to_string(block)+"-config-"+std::to_string(variant);
    bytes(out/(stem+"-before.state"),before);bytes(out/(stem+"-input.state"),input);bytes(out/(stem+"-after.state"),after);
    log<<"{\"event\":\"config\",\"block\":"<<block<<",\"variant\":"<<variant<<",\"stem\":\""<<stem<<"\"}\n";
}
void note(Events& e,int pitch,int offset){Event n{};n.type=Event::kNoteOnEvent;n.busIndex=0;n.sampleOffset=offset;n.noteOn.channel=0;n.noteOn.pitch=int16(pitch);n.noteOn.velocity=.75f;n.noteOn.noteId=pitch;e.events.push_back(n);}
void reprepare(Instance& i,double rate){
    require(i.audio->setProcessing(false)==kResultOk,"stop before native prepare");
    require(i.component->setActive(false)==kResultOk,"deactivate before native prepare");i.active=false;
    ProcessSetup setup{kRealtime,kSample32,64,rate};require(i.audio->setupProcessing(setup)==kResultOk,"native reprepare");
    require(i.component->setActive(true)==kResultOk,"reactivate native component");i.active=true;
    require(i.audio->setProcessing(true)==kResultOk,"resume native processing");
}
int run(const char* modulePath,const std::filesystem::path& out){
    require(!std::filesystem::exists(out),"fresh capture directory");std::filesystem::create_directories(out);
    Module module(modulePath);
    auto parameters=cs80::defaultValues();parameters[cs80::P_sourcePerformance]=1;parameters[cs80::P_parityMode]=0;
    parameters[cs80::P_voiceMode]=0;parameters[cs80::P_volume]=.15f;parameters[cs80::P_drift]=.37f;
    parameters[cs80::P_voiceDispersion]=.18f;parameters[cs80::P_driftModel]=0;parameters[cs80::P_analogNoise]=0;
    parameters[cs80::P_mod1Amount]=.5f;parameters[cs80::P_mseg1Rate]=2;
    Instance instance(module,parameters,0,0);
    std::ofstream trace(out/"callbacks.bin",std::ios::binary),pcm(out/"pcm.bin",std::ios::binary),log(out/"events.jsonl");
    require(bool(trace)&&bool(pcm)&&bool(log),"capture streams");
    edit(instance,0,-1,out,log);
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;
    unsigned callbacks=0,noteOns=0,nonzero=0;double rate=48000;unsigned mode=0;
    for(unsigned b=0;b<80;++b){
        std::fenv_t saved;std::fegetenv(&saved);const int savedErrno=errno;
        if(b==8||b==24||b==40||b==64)edit(instance,b==8?1:b==24?2:b==40?3:4,int(b),out,log);
        if(b==56){rate=96000;reprepare(instance,rate);log<<"{\"event\":\"reprepare\",\"block\":56,\"rate\":96000}\n";}
        Changes changes;
        if(b==16||b==32||b==48||b==72){mode=(b==16||b==48)?1:0;changes.add(parameterId(cs80::paramId(cs80::P_voiceMode)),{{0,normalized(cs80::P_voiceMode,float(mode))}});}
        Events events;
        if(b==0)for(int n=0;n<8;++n)note(events,36+n,n);
        if(b==16||b==49)for(int n=0;n<8;++n)note(events,(b==16?60:76)+n,n);
        if(b==56)for(int n=0;n<16;++n)note(events,36+n,n);
        noteOns+=unsigned(events.events.size());
        std::array<float,64> left{},right{};float* outputPointers[]={left.data(),right.data()};float* inputPointers[]={nullptr};
        AudioBusBuffers output{};output.numChannels=2;output.channelBuffers32=outputPointers;
        AudioBusBuffers input{};input.numChannels=1;input.channelBuffers32=inputPointers;input.silenceFlags=1;
        ProcessContext context{};context.sampleRate=rate;context.projectTimeSamples=int64(b)*64;context.tempo=b<24?120.:97.;
        context.projectTimeMusic=b<24?0.:8.5;context.state=ProcessContext::kTempoValid|ProcessContext::kProjectTimeMusicValid|(b>=24?ProcessContext::kPlaying:0);
        ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;
        data.numInputs=1;data.inputs=&input;data.numOutputs=1;data.outputs=&output;
        data.inputParameterChanges=changes.queues.empty()?nullptr:&changes;data.inputEvents=&events;data.processContext=&context;
        std::fesetenv(&saved);errno=savedErrno;const auto before=fp();
        const auto code=instance.audio->process(data);const auto after=fp();require(code==kResultOk,"actual native process return");
        std::fegetenv(&saved);const int observedErrno=errno;
        volatile double a=1.,delta=0x1p-54;volatile double probe=a+delta;const auto next=fp();
        put(trace,b);put(trace,mode);put(trace,rate);put(trace,code);put(trace,output.silenceFlags);putFP(trace,before);putFP(trace,after);putFP(trace,next);const double probeCopy=probe;put(trace,probeCopy);
        for(unsigned n=0;n<64;++n){require(std::isfinite(left[n])&&std::isfinite(right[n]),"finite native PCM");nonzero+=left[n]!=0||right[n]!=0;put(pcm,left[n]);put(pcm,right[n]);}
        if(b==0||b==8||b==16||b==24||b==32||b==40||b==48||b==49||b==56||b==64||b==72||b==79)bytes(out/("block-"+std::to_string(b)+".state"),state(instance));
        std::fesetenv(&saved);errno=observedErrno;++callbacks;
    }
    require(callbacks==80&&noteOns==40&&nonzero>0,"bounded nonvacuous capture");
    std::cout<<"{\"schema\":\"modern-mirror-real-wrapper-v1\",\"callbacks\":"<<callbacks<<",\"frames\":5120,\"note_on_events\":"<<noteOns<<",\"nonzero_stereo_frames\":"<<nonzero<<",\"process_entry\":\"IAudioProcessor::process\",\"side_state_entry\":\"IComponent::setState\",\"private_voice_counts_observed\":false,\"timing\":false}\n";
    return 0;
}
}
int main(int argc,char** argv){try{require(argc==3,"MODULE CAPTURE_DIR");return mirror_fixture::run(argv[1],argv[2]);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

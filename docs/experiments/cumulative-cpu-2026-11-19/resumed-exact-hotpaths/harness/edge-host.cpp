// Non-timed, public VST3 regression capture for callback histories not covered by the short work fixture.
// Compile against the repository's frozen factory8 scaffold. No production symbols or counters used.
#define main cs80_archived_admission_main
#include "factory8_base.cpp"
#undef main
#include <charconv>
#include <map>

namespace edge {
using namespace factory8;
namespace fs = std::filesystem;
struct RestoreFP {
    std::fenv_t e{}; int saved;
    RestoreFP():saved(errno){ require(std::fegetenv(&e)==0,"save FP"); }
    ~RestoreFP(){ std::fesetenv(&e); errno=saved; }
};
int run(const fs::path &elf, const fs::path &out, const std::string &scene, int sr) {
    bool quiet=scene=="quiet", ctrl=scene=="automation", legacy=scene=="legacy", ext=scene=="external", mpe=scene=="mpe", fx=scene=="effects", pure=scene=="pure", pulse=scene=="pulse";
    require(quiet||ctrl||legacy||ext||mpe||fx||pure||pulse||scene=="factory", "unknown scene");
    require(sr==44100||sr==48000||sr==96000,"unsupported sample rate");
    require(fs::create_directory(out),"fresh output directory");
    testSampleRate=sr;
    std::filesystem::path mod=fs::canonical(elf);
    Module module(mod.c_str()); void* sym=dlsym(module.handle,"GetPluginFactory"); Dl_info image{};
    require(sym && dladdr(sym,&image) && image.dli_fname && fs::equivalent(image.dli_fname,mod),"module identity");
    auto p=factory8::controls();
    if(legacy) p[cs80::P_sourcePerformance]=0;
    if(pure) p[cs80::P_parityMode]=1;
    if(pulse) for(int rank=0;rank<2;++rank){p[cs80::chParam(rank,cs80::CP_saw)]=0;p[cs80::chParam(rank,cs80::CP_pulse)]=1;p[cs80::chParam(rank,cs80::CP_pw)]=.67f;p[cs80::chParam(rank,cs80::CP_pwmDepth)]=0.f;}
    if(mpe) p[cs80::P_mpeOn]=1;
    if(ext) { p[cs80::P_subExtLevel]=0.84f; p[cs80::P_subVcf]=0.62f; p[cs80::P_subOriginalFunction]=5; }
    if(fx) {p[cs80::P_chorusOn]=1; p[cs80::P_chorusDepth]=.63f;p[cs80::P_ringAmount]=.35f;p[cs80::P_fx1Model]=3;p[cs80::P_fx1Mix]=.31f;}
    writeControls(out/"controls.csv",p);
    Instance i(module,p,pulse?13:0,pulse?13:0);
    require(i.audio->canProcessSampleSize(kSample32)==kResultTrue,"32-bit samples");
    require(i.component->activateBus(kAudio,kInput,0,ext)==kResultOk,"external input setting");
    {RestoreFP g; observeMetadata(i,out,"initial"); save(out/"initial.state",state(i));}
    std::ofstream pcm(out/"pcm.bin",std::ios::binary), trace(out/"callbacks.bin",std::ios::binary);
    std::array<float,64> left{},right{},inputSamples{}; float* outs[]={left.data(),right.data()}; float* ins[]={ext?inputSamples.data():nullptr};
    AudioBusBuffers outBus{},in{};outBus.channelBuffers32=outs;outBus.numChannels=2;in.channelBuffers32=ins;in.numChannels=1;in.silenceFlags=ext?0:1;
    ProcessContext ctx{};ctx.sampleRate=sr;ctx.tempo=121.75;ctx.state=ProcessContext::kTempoValid|ProcessContext::kPlaying;
    ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numInputs=1;data.inputs=&in;data.numOutputs=1;data.outputs=&outBus;data.processContext=&ctx;
    factory8::HeldEvents events;data.inputEvents=&events;
    constexpr unsigned total=3072; constexpr std::array<int,16> sizes{64,64,31,1,32,16,63,7,64,2,48,64,29,32,64,53};
    std::uint64_t frames=0,nonzero=0,notesRead=0,ons=0,offs=0,pressure=0,hostQueues=0,hostPoints=0; unsigned mask=0; double peak=0.;
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;
    for(unsigned b=0;b<total;++b){
        int n=sizes[b%sizes.size()]; data.numSamples=n;ctx.projectTimeSamples=frames; left.fill(0);right.fill(0);outBus.silenceFlags=0;events.count=events.countReads=events.reads=0;
        for(int j=0;j<n;++j)inputSamples[j]=float(int((frames+j)*53%1021)-509)*0x1p-10f;
        const bool on=!quiet&&(b==1||b==1919),off=!quiet&&(b==513||b==2879),press=(mpe||ctrl)&&b%43==18;
        if(on||off||press){
            events.count=8;
            for(int v=0;v<events.count;++v){auto&e=events.values[v];e={};e.busIndex=0;e.sampleOffset=(v*(n-1))/7;int ch=mpe?1+v:0;
                if(on){e.type=Event::kNoteOnEvent;e.noteOn.channel=ch;e.noteOn.pitch=notes[v];e.noteOn.velocity=float(44+v*10)/127.f;e.noteOn.noteId=v;}
                else if(off){e.type=Event::kNoteOffEvent;e.noteOff.channel=ch;e.noteOff.pitch=notes[v];e.noteOff.noteId=v;}
                else{e.type=Event::kPolyPressureEvent;e.polyPressure.channel=ch;e.polyPressure.pitch=notes[v];e.polyPressure.noteId=v;e.polyPressure.pressure=float(3+(7*v+int(b))%122)/127.f;}
            }
            if(on)ons+=8; else if(off)offs+=8; else pressure+=8;
        }
        Changes edits;
        if((ctrl||legacy||fx)&&b%37==3&&n>=7){
            auto add=[&](int idx,double a,double z){edits.add(parameterId(cs80::paramId(idx)),{{0,a},{n/2,z},{n-1,a*.25+z*.5}}); ++hostQueues;hostPoints+=3;};
            add(cs80::P_brilliance,(b%17)/18.,(b%23)/25.); add(cs80::P_volume,.06+double(b%5)/30.,.16+double(b%9)/40.);
            add(cs80::chParam(1,cs80::CP_pan),.09+(b%6)/12.,.85-(b%4)/11.);
            if(fx){add(cs80::P_ringAmount,.21,.48);add(cs80::P_chorusDepth,.09,.72);}
        }
        data.inputParameterChanges=edits.queues.empty()?nullptr:&edits;
        auto before=fp();auto ret=i.audio->process(data);auto after=fp();require(ret==kResultOk && events.reads==unsigned(events.count),"real callback / event receipts");
        {RestoreFP g;
            volatile double one=1,delta=0x1p-54,probe=one+delta;auto next=fp();double val=probe;
            const auto latency=i.audio->getLatencySamples(),tail=i.audio->getTailSamples();
            put(trace,b);put(trace,n);put(trace,ctx.projectTimeSamples);put(trace,ret);put(trace,outBus.silenceFlags);put(trace,events.count);put(trace,events.reads);put(trace,events.countReads);
            putFP(trace,before);putFP(trace,after);putFP(trace,next);put(trace,val);put(trace,latency);put(trace,tail);
            mask|=latency==67?1u:2u;
            for(int j=0;j<n;++j)for(auto s:{left[j],right[j]}){require(std::isfinite(s),"nonfinite PCM");put(pcm,s);peak=std::max(peak,std::fabs(double(s)));nonzero+=s!=0;}
            if(b==1||b==512||b==513||b==1918||b==1919||b==2879||b==total-1)save(out/("after-"+std::to_string(b)+".state"),state(i));
        }
        frames+=n;notesRead+=events.reads;
    }
    {RestoreFP g;observeMetadata(i,out,"final");}
    require(notesRead==ons+offs+pressure,"event census");
    require(quiet?peak<=1e-6:(nonzero>0 && peak>1e-5),"quiet/audible guard");
    std::ofstream result(out/"outcome.json");result<<"{\"scene\":"<<std::quoted(scene)<<",\"sample_rate\":"<<sr<<",\"callbacks\":"<<total<<",\"frames\":"<<frames<<",\"peak\":"<<std::setprecision(17)<<peak<<",\"nonzero_samples\":"<<nonzero<<",\"note_ons\":"<<ons<<",\"note_offs\":"<<offs<<",\"poly_pressure\":"<<pressure<<",\"queues_sent\":"<<hostQueues<<",\"points_sent\":"<<hostPoints<<",\"reported_latency_67_every_call\":"<<(mask==1?"true":"false")<<"}\n";require(bool(result),"result write");return 0;
}
}
int main(int argc,char**argv){try{require(argc==5,"MODULE SCENE RATE NEW_DIRECTORY");return edge::run(argv[1],argv[4],argv[2],std::stoi(argv[3]));}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

// Native cross-selector continuation: all 14x14 physical rank selector identities on one running processor.
#define main archived_factory_main
#include "factory8_base.cpp"
#undef main
namespace selector {
using namespace factory8;
namespace fs=std::filesystem;
int run(const fs::path& elf,const fs::path& out){
    require(fs::create_directory(out),"fresh directory");auto path=fs::canonical(elf);Module m(path.c_str());Dl_info di{};require(dladdr(dlsym(m.handle,"GetPluginFactory"),&di)&&di.dli_fname&&fs::equivalent(di.dli_fname,path),"module identity");auto p=controls();Instance i(m,p,0,0);require(i.component->activateBus(kAudio,kInput,0,false)==kResultOk,"disable external input");observeMetadata(i,out,"initial");
    std::array<float,64> l{},r{};float* outs[]={l.data(),r.data()};float* ins[]={nullptr};AudioBusBuffers ob{},in{};ob.numChannels=2;ob.channelBuffers32=outs;in.numChannels=1;in.channelBuffers32=ins;in.silenceFlags=1;
    ProcessContext cx{};cx.sampleRate=48000;HeldEvents ev;ProcessData d{};d.symbolicSampleSize=kSample32;d.processMode=kRealtime;d.numInputs=d.numOutputs=1;d.inputs=&in;d.outputs=&ob;d.inputEvents=&ev;d.processContext=&cx;
    std::ofstream pcm(out/"pcm.bin",std::ios::binary),tr(out/"trace.bin",std::ios::binary),receipt(out/"pairs.csv");receipt<<"phase,rank1,rank2,frames,peak\n";std::uint64_t frames=0;unsigned nOns=0,nOffs=0;
    constexpr std::array<int,12> sizes{64,61,1,16,42,7,64,32,47,64,11,63};
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;
    for(int phase=0;phase<196;++phase){const int perm=(phase*73+19)%196;int rank1=perm/14,rank2=perm%14;auto start=frames;double peak=0;
        for(int block=0;block<32;++block){int n=sizes[(phase+block)%sizes.size()];d.numSamples=n;cx.projectTimeSamples=frames;Changes ch;if(block==0){ch.add(parameterId("hardware_source_1"),{{0,rank1/13.}});ch.add(parameterId("hardware_source_2"),{{0,rank2/13.}});}d.inputParameterChanges=ch.queues.empty()?nullptr:&ch;
            ev.count=ev.reads=ev.countReads=0;bool on=(phase%4==0&&block==2),off=(phase%4==3&&block==15);if(on||off){ev.count=8;for(int v=0;v<8;++v){auto&e=ev.values[v];e={};e.busIndex=0;e.sampleOffset=(v*(n-1))/7;if(on){e.type=Event::kNoteOnEvent;e.noteOn.channel=0;e.noteOn.pitch=notes[v]+(phase%3);e.noteOn.noteId=v;e.noteOn.velocity=float(41+v*11)/127.f;}else{e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=notes[v]+((phase-3)%3);e.noteOff.noteId=v;}}if(on)nOns+=8;else nOffs+=8;}
            l.fill(0);r.fill(0);ob.silenceFlags=0;auto f0=fp();auto ret=i.audio->process(d);auto f1=fp();require(ret==kResultOk&&ev.reads==unsigned(ev.count),"callback or MIDI receipts");std::fenv_t saved{};auto er=errno;std::fegetenv(&saved);volatile double one=1,delta=0x1p-54,probe=one+delta;auto f2=fp();double pr=probe;for(auto y:{f0,f1,f2})putFP(tr,y);put(tr,pr);put(tr,ev.reads);put(tr,ev.countReads);put(tr,ret);put(tr,ob.silenceFlags);auto latency=i.audio->getLatencySamples();require(latency==67,"latency");put(tr,latency);
            for(int j=0;j<n;++j)for(float s:{l[j],r[j]}){require(std::isfinite(s),"finite PCM");put(pcm,s);peak=std::max(peak,std::fabs(double(s)));}
            if(block==31)save(out/("selector-"+std::to_string(phase)+".state"),state(i));std::fesetenv(&saved);errno=er;frames+=n;
        }receipt<<phase<<','<<rank1<<','<<rank2<<','<<frames-start<<','<<std::setprecision(17)<<peak<<'\n';
    }observeMetadata(i,out,"final");require(nOns==392&&nOffs==392,"complete notes");std::cout<<"{\"actual_callbacks\":6272,\"stereo_frames\":"<<frames<<",\"note_ons\":"<<nOns<<",\"note_offs\":"<<nOffs<<"}\n";return 0;
}
}
int main(int argc,char**argv){try{require(argc==3,"MODULE OUT");return selector::run(argv[1],argv[2]);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

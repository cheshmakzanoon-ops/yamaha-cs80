// Excited controller fixture. Each bit independently toggles only that MIDI value stream.
#define main archived_factory_main
#include "factory8_base.cpp"
#undef main
#include <pluginterfaces/vst/ivstmidicontrollers.h>
namespace midi {
namespace fs=std::filesystem;
using namespace factory8;
struct Pins {std::array<ParamID,4> id;};
Pins mappings(Instance& i){
    TUID cid{};require(i.component->getControllerClassId(cid)==kResultOk,"class ID");IEditController* c=nullptr;require(i.factory->createInstance(cid,IEditController::iid,reinterpret_cast<void**>(&c))==kResultOk && c && c->initialize(nullptr)==kResultOk,"controller");
    IConnectionPoint *cc=nullptr,*ec=nullptr;require(i.component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&cc))==kResultOk && c->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&ec))==kResultOk,"connection points");require(cc->connect(ec)==kResultOk&&ec->connect(cc)==kResultOk,"controller binding");
    IMidiMapping* m=nullptr;require(c->queryInterface(IMidiMapping::iid,reinterpret_cast<void**>(&m))==kResultOk&&m,"IMidiMapping");
    std::array<int16,4> ch{0,1,0,0};std::array<CtrlNumber,4> type{kPitchBend,kPitchBend,kAfterTouch,kCtrlModWheel};Pins p{};
    for(unsigned x=0;x<4;++x)require(m->getMidiControllerAssignment(0,ch[x],type[x],p.id[x])==kResultTrue,"real MIDI mapping");
    auto ids=p.id;std::sort(ids.begin(),ids.end());require(std::adjacent_find(ids.begin(),ids.end())==ids.end(),"four distinct parameter IDs");
    m->release();i.controller=c;i.controllerConnection=ec;i.componentConnection=cc;return p;
}
int run(const fs::path& path,unsigned mask,const fs::path& out){
    require(mask<=15 && fs::create_directory(out),"mask and fresh output");auto elf=fs::canonical(path);Module mod(elf.c_str());Dl_info di{};require(dladdr(dlsym(mod.handle,"GetPluginFactory"),&di)&&di.dli_fname&&fs::equivalent(di.dli_fname,elf),"loaded identity");
    auto p=controls();p[cs80::P_mpeOn]=1;p[cs80::P_mpeBendRange]=12;p[cs80::P_pitchBendRange]=7;p[cs80::P_subOriginalFunction]=0;p[cs80::P_subVcf]=.24f;p[cs80::P_subAtVcf]=.75f;p[cs80::P_modWheelVcf]=.79f;p[cs80::P_modWheelSub]=.6f;
    for(int rank=0;rank<2;++rank){p[cs80::chParam(rank,cs80::CP_saw)]=1;p[cs80::chParam(rank,cs80::CP_pulse)]=1;p[cs80::chParam(rank,cs80::CP_afterBrill)]=.72f;p[cs80::chParam(rank,cs80::CP_afterLevel)]=.81f;}
    Instance i(mod,p,13,13);require(i.component->activateBus(kAudio,kInput,0,false)==kResultOk,"disable external input");observeMetadata(i,out,"initial");save(out/"initial.state",state(i)); const Pins pins=mappings(i);
    std::array<float,64> l{},r{};float* outputs[]={l.data(),r.data()};float* inputs[]={nullptr};AudioBusBuffers outb{},in{};outb.channelBuffers32=outputs;outb.numChannels=2;in.channelBuffers32=inputs;in.numChannels=1;in.silenceFlags=1;
    ProcessContext ctx{};ctx.sampleRate=48000;ProcessData d{};d.symbolicSampleSize=kSample32;d.processMode=kRealtime;d.numSamples=64;d.numInputs=d.numOutputs=1;d.inputs=&in;d.outputs=&outb;d.processContext=&ctx;HeldEvents events;d.inputEvents=&events;
    std::ofstream pcm(out/"pcm.bin",std::ios::binary),fpout(out/"fp.bin",std::ios::binary), ledger(out/"delivered.csv");ledger<<"kind,vst_id\n";for(unsigned j=0;j<4;++j)ledger<<j<<','<<pins.id[j]<<'\n';ledger<<"block,controller,offset,normalized\n"<<std::setprecision(17);
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;unsigned ons=0,offs=0,controlsSent=0;double peak=0;
    for(int b=0;b<512;++b){int n=64;ctx.projectTimeSamples=int64(b)*64;events.count=events.reads=events.countReads=0;bool on=b==0||b==354,off=b==244||b==480;
        if(on||off){events.count=8;for(int v=0;v<8;++v){auto& e=events.values[v];e={};e.busIndex=0;e.sampleOffset=v*8;if(on){e.type=Event::kNoteOnEvent;e.noteOn.channel=1+v;e.noteOn.pitch=notes[v];e.noteOn.noteId=v;e.noteOn.velocity=(73.f+6*v)/127.f;}else{e.type=Event::kNoteOffEvent;e.noteOff.channel=1+v;e.noteOff.pitch=notes[v];e.noteOff.noteId=v;}}if(on)ons+=8;else offs+=8;}
        Changes curves;bool moving=(b>=4&&b<222&&b%7==4)||(b>=363&&b<460&&b%11==0),reset=b==226||b==466;
        if(moving||reset){for(unsigned j=0;j<4;++j){double value=j<2?.5:0.;if(!reset&&(mask&(1u<<j))){double wave=(17*b+37*j)%109/108.;value=j<2?.24+.52*wave:.1+.84*wave;}int offset=2+j*13;curves.add(pins.id[j],{{offset,value}});ledger<<b<<','<<j<<','<<offset<<','<<value<<'\n';++controlsSent;}}
        d.inputParameterChanges=curves.queues.empty()?nullptr:&curves;l.fill(0);r.fill(0);outb.silenceFlags=0;
        auto before=fp();auto ret=i.audio->process(d);auto after=fp();require(ret==kResultOk&&events.reads==unsigned(events.count),"real VST3 event receipts");std::fenv_t saved{};std::fegetenv(&saved);auto savedErr=errno;
        volatile double one=1,delta=0x1p-54,probe=one+delta;auto next=fp();double probed=probe;for(auto a:{before,after,next})putFP(fpout,a);put(fpout,probed);put(fpout,b);put(fpout,ret);put(fpout,events.reads);put(fpout,events.countReads);put(fpout,outb.silenceFlags);auto latency=i.audio->getLatencySamples();require(latency==67,"source reported latency");put(fpout,latency);
        for(int k=0;k<n;++k)for(float s:{l[k],r[k]}){require(std::isfinite(s),"finite audio");peak=std::max(peak,std::abs(double(s)));put(pcm,s);}
        if(b==127||b==226||b==355||b==511)save(out/("after-"+std::to_string(b)+".state"),state(i));std::fesetenv(&saved);errno=savedErr;
    }i.releaseReadbackController();observeMetadata(i,out,"final");require(ons==16&&offs==16&&controlsSent>120&&peak>1e-4,"excitation");
    std::ofstream o(out/"outcome.json");o<<"{\"mask\":"<<mask<<",\"actual_callbacks\":512,\"stereo_frames\":32768,\"note_ons\":"<<ons<<",\"note_offs\":"<<offs<<",\"controller_points_sent\":"<<controlsSent<<",\"latency_samples\":67,\"peak\":"<<std::setprecision(17)<<peak<<"}\n";require(bool(o)&&bool(ledger),"public file writes");return 0;
}
}
int main(int argc,char**argv){try{require(argc==4,"MODULE MASK OUT");return midi::run(argv[1],std::stoul(argv[2]),argv[3]);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

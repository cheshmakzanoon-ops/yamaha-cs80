// Zero-callback-clock acquisition only. Intentionally no timing mode in this binary.
#define main archived_factory8_main_not_called
#include "../../docs/experiments/eight-stage-filter/native-cost/host/factory8_base.cpp"
#undef main
#include <sched.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <charconv>
#include <sstream>

namespace dry {
using namespace factory8;
namespace fs=std::filesystem;
constexpr int warm=1024, releases=8192, window=576, segment=36, tail=33;
constexpr int varied[]={41,53,65,77,89,101,113,125};
const std::array<std::string,5> scenes{"idle","release_equal","release_varied","held_varied","wake_varied"};

int number(const std::string& s) {
    int v=0; auto [p,ec]=std::from_chars(s.data(),s.data()+s.size(),v);
    require(ec==std::errc{} && p==s.data()+s.size(),"integer argument"); return v;
}
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> r; std::istringstream f(s); std::string cell;
    while(std::getline(f,cell,',')) r.push_back(cell);
    require(!s.empty() && s.back()!=',',"invalid CSV field"); return r;
}
void checkPin(int cpu) {
    cpu_set_t mask; require(sched_getaffinity(0,sizeof(mask),&mask)==0 && CPU_COUNT(&mask)==1 &&
            CPU_ISSET(cpu,&mask) && sched_getcpu()==cpu,"incorrect worker pin");
}

struct PlanRow { int index,quantum,first,extra; };
using Plan=std::array<PlanRow,window>;
Plan loadPlan(const fs::path& file) {
    std::ifstream in(file); std::string line;
    require(bool(std::getline(in,line)) && line=="index,quantum,first_lane,extra","unexpected schedule header");
    Plan p{}; int extras=0;
    for (int i=0;i<window;++i) {
        require(bool(std::getline(in,line)),"short schedule"); auto fields=split(line);
        require(fields.size()==4,"schedule fields");
        auto& r=p[i]; r={number(fields[0]),number(fields[1]),number(fields[2]),number(fields[3])};
        require(r.index==i && (r.quantum==1||r.quantum==4) && (r.first==0||r.first==1) &&
                (r.extra==0||r.extra==1),"invalid schedule"); extras+=r.extra;
        if(i%r.quantum) require(r.quantum==p[i-1].quantum && r.first==p[i-1].first,"split four-call burst");
        if(i%segment) require(r.quantum==p[i-1].quantum,"changing quantum inside block");
        else if(i) require(r.quantum!=p[i-1].quantum,"nonalternating quantum");
        if(i%9==8) { require(extras==1,"one real extra per nine"); extras=0; }
    }
    require(!std::getline(in,line) && in.eof(),"trailing schedule"); return p;
}

struct Visit { int scene,placement,phase,index,count,first,lane; unsigned serial; };
struct Baton {
    int lane, fd; unsigned serial=0; std::vector<Visit> audit;
    Baton(int l,int f):lane(l),fd(f){audit.reserve(4096);}
    void io(bool transmit,std::uint32_t token) {
        // A typed sequence number detects duplicate, late or wrong-phase handoffs. IO never
        // changes the caller's visible errno. SOCK_SEQPACKET retains message boundaries.
        int old=errno; std::uint32_t wire=token; ssize_t n;
        do { n=transmit ? send(fd,&wire,sizeof(wire),MSG_NOSIGNAL) : recv(fd,&wire,sizeof(wire),0); }
        while(n<0 && errno==EINTR);
        require(n==ssize_t(sizeof(wire)) && wire==token,"peer disconnected or wrong baton token");
        errno=old;
    }
    template<class Work>
    void visit(int scene,int placement,int phase,int begin,int count,int first,Work work) {
        const auto id=serial++;
        audit.push_back({scene,placement,phase,begin,count,first,lane,id});
        if(lane<0) { work(); return; }
        // Serial barrier even for warm-up/continuation. Neither peer can run the
        // next visit's audio before this visit's second peer acknowledges completion.
        if(lane==first) {work(); io(true,2*id+1); io(false,2*id+2);}
        else {io(false,2*id+1); work(); io(true,2*id+2);}
    }
};

enum class Ev{none,on,off};
struct Record {
    std::array<float,128> pcm;
    FP before,after,next; double probe; tresult result; std::uint64_t silence;
    unsigned reads, queries; int placement, phase, index, event, physicalVelocity;
};
struct Input {int place,phase,index,voice,type,pitch,noteId;std::uint32_t velocityBits;};
struct Capture {
    std::vector<Record> records; std::vector<std::string> states; std::vector<Input> inputs;
    explicit Capture(std::size_t size){records.reserve(size);states.reserve(48);inputs.reserve(512);}
};
struct ReadEvents final : IEventList {
    std::array<Event,8> values{}; int count=0; unsigned reads=0,countReads=0,mask=0; bool duplicate=false;
    tresult PLUGIN_API queryInterface(const TUID t,void** o) override {
        if(!std::memcmp(t,IEventList::iid,sizeof(TUID))||!std::memcmp(t,FUnknown::iid,sizeof(TUID))){
            *o=this;return kResultOk;
        }*o=nullptr;return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override{return 1;} uint32 PLUGIN_API release() override{return 1;}
    int32 PLUGIN_API getEventCount() override{++countReads;return count;}
    tresult PLUGIN_API getEvent(int32 at,Event& result) override{
        if(at<0||at>=count)return kInvalidArgument;
        const unsigned bit=1u<<at;duplicate|=(mask&bit)!=0;mask|=bit;++reads;result=values[at];return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event&) override{return kNotImplemented;}
};

struct Driver {
    Instance instance; ReadEvents events; std::array<float,64> l{},r{};
    float* outs[2]{l.data(),r.data()};float* ins[1]{nullptr};
    AudioBusBuffers out{},in{};ProcessContext context{};ProcessData data{}; int64 cursor=0;
    Capture& cap; int place;
    Driver(Module& mod,Capture& c,int placement):instance(mod,controls(),0,0),cap(c),place(placement){
        require(instance.audio->canProcessSampleSize(kSample32)==kResultTrue &&
                instance.component->getBusCount(kAudio,kInput)==1 && instance.component->getBusCount(kAudio,kOutput)==1 &&
                instance.component->getBusCount(kEvent,kInput)==1,"native setup");
        BusInfo a{},b{};
        require(instance.component->getBusInfo(kAudio,kInput,0,a)==kResultOk && a.channelCount==1 &&
                instance.component->getBusInfo(kAudio,kOutput,0,b)==kResultOk && b.channelCount==2 &&
                instance.component->activateBus(kAudio,kInput,0,false)==kResultOk,"native bus topology");
        out.numChannels=2;out.channelBuffers32=outs;in.numChannels=1;in.channelBuffers32=ins;in.silenceFlags=1;
        context.sampleRate=48000;
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;
        data.numInputs=1;data.inputs=&in;data.numOutputs=1;data.outputs=&out;data.inputEvents=&events;data.processContext=&context;
        require(std::fesetenv(FE_DFL_ENV)==0,"reset FP");_mm_setcsr(0x9fc0);errno=41;
    }
    void snapshot(){
        const int err=errno;std::fenv_t env;require(std::fegetenv(&env)==0,"snapshot FP");
        cap.states.push_back(state(instance));require(!cap.states.back().empty(),"empty public state");
        require(std::fesetenv(&env)==0,"restore snapshot FP");errno=err;
    }
    double call(int phase,int index,Ev ev,bool variedVelocity){
        require(cap.records.size()<cap.records.capacity(),"record storage overrun");
        auto& rec=cap.records.emplace_back();
        l.fill(0);r.fill(0);out.silenceFlags=0;context.projectTimeSamples=cursor;cursor+=64;
        events.count=ev==Ev::none?0:8;events.reads=events.countReads=events.mask=0;events.duplicate=false;
        for(int v=0;v<events.count;++v){
            auto& e=events.values[v];e={};e.busIndex=0;e.sampleOffset=0;
            if(ev==Ev::on){
                e.type=Event::kNoteOnEvent;e.noteOn.channel=0;e.noteOn.pitch=int16(notes[v]);e.noteOn.noteId=v;
                e.noteOn.velocity=float(variedVelocity?varied[v]:102)/127.0f;
            } else {
                e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=int16(notes[v]);e.noteOff.noteId=v;
            }
        }
        rec.before=fp();rec.result=instance.audio->process(data);rec.after=fp();
        const int savedError=errno;std::fenv_t saved;require(std::fegetenv(&saved)==0,"save callback FP");
        volatile double one=1.,delta=0x1p-54;volatile double pr=one+delta;
        rec.next=fp();rec.probe=pr;rec.silence=out.silenceFlags;rec.reads=events.reads;rec.queries=events.countReads;
        rec.placement=place;rec.phase=phase;rec.index=index;rec.event=int(ev);
        rec.physicalVelocity=ev==Ev::on?(variedVelocity?1:0):-1;
        double peak=0;
        for(int j=0;j<64;++j){
            const float x=l[j],y=r[j];require(std::isfinite(x)&&std::isfinite(y),"nonfinite PCM");
            rec.pcm[2*j]=x;rec.pcm[2*j+1]=y;peak=std::max({peak,std::abs(double(x)),std::abs(double(y))});
        }
        require(rec.result==kResultOk && rec.reads==unsigned(events.count) && !events.duplicate &&
                events.mask==(events.count?255u:0u),"native result or incomplete/duplicate MIDI reads");
        for(int v=0;v<events.count;++v) {
            const auto& e=events.values[v];const bool on=ev==Ev::on;
            cap.inputs.push_back({place,phase,index,v,int(ev),on?e.noteOn.pitch:e.noteOff.pitch,
                                  on?e.noteOn.noteId:e.noteOff.noteId,
                                  std::bit_cast<std::uint32_t>(on?e.noteOn.velocity:e.noteOff.velocity)});
        }
        require(std::fesetenv(&saved)==0,"restore callback FP");errno=savedError;return peak;
    }
};

void output(const fs::path& dir,const Capture& cap,unsigned expected){
    require(cap.records.size()==expected,"complete public history");
    std::ofstream pcm(dir/"all.pcm",std::ios::binary), trace(dir/"all.fp",std::ios::binary),states(dir/"states.bin",std::ios::binary);
    for(const auto& r:cap.records){
        pcm.write(reinterpret_cast<const char*>(r.pcm.data()),sizeof(r.pcm));
        for(auto f:{r.before,r.after,r.next})putFP(trace,f);
        put(trace,r.probe);put(trace,r.result);put(trace,r.silence);put(trace,r.reads);put(trace,r.queries);
        put(trace,r.placement);put(trace,r.phase);put(trace,r.index);put(trace,r.event);put(trace,r.physicalVelocity);
    }
    for(auto& s:cap.states){std::uint64_t n=s.size();put(states,n);states.write(s.data(),std::streamsize(n));}
    pcm.close();trace.close();states.close();require(bool(pcm)&&bool(trace)&&bool(states),"complete capture outputs");
    std::ofstream events(dir/"midi.csv");events<<"placement,phase,index,input,type,pitch,note_id,velocity_bits\n";
    for(const auto& e:cap.inputs) events<<e.place<<','<<e.phase<<','<<e.index<<','<<e.voice<<','<<e.type<<','
                                        <<e.pitch<<','<<e.noteId<<','<<e.velocityBits<<'\n';
    events.close();require(bool(events),"complete input receipts");
}

void runScene(Module& mod,Baton& gate,int scene,int placements,const Plan& plan,const fs::path& dir){
    const bool released=scene==1||scene==2, held=scene==3,wake=scene==4;
    const bool quiet=scene<3; const int nplacements=wake?placements:1,take=wake?segment:window;
    const int setup=warm+(released?releases:0),total=nplacements*(setup+take+tail);
    Capture cap(total);
    struct Stats {double initial=0,guard=0,observed=0,continuation=0;};
    std::vector<Stats> stats(nplacements);
    std::fenv_t startup;require(std::fegetenv(&startup)==0,"preserve common setup FP"); const int startupErr=errno;
    for(int place=0;place<nplacements;++place){
        require(std::fesetenv(&startup)==0,"restore common setup FP");errno=startupErr;
        Driver d(mod,cap,place);auto& s=stats[place];
        gate.visit(scene,place,0,0,setup,(place+scene)%2,[&]{
            for(int j=0;j<setup;++j){
                const Ev ev=j==0 && (held||released)?Ev::on:j==warm && released?Ev::off:Ev::none;
                const double peak=d.call(0,j,ev,scene!=1);
                if(j<warm)s.initial=std::max(s.initial,peak);
                if(j>=setup-64)s.guard=std::max(s.guard,peak);
            }
            d.snapshot();
        });
        const int begin=wake?place*segment:0;
        for(int local=0;local<take;){
            const int k=begin+local,q=plan[k].quantum,first=plan[k].first;
            require(local+q<=take,"quantum outside placement");
            gate.visit(scene,place,1,k,q,first,[&]{
                for(int j=0;j<q;++j){
                    const double peak=d.call(1,k+j,(wake&&local+j==0)?Ev::on:Ev::none,true);
                    s.observed=std::max(s.observed,peak);
                }
            });local+=q;
        }
        gate.visit(scene,place,2,0,tail,(place+scene+1)%2,[&]{
            d.snapshot();
            for(int j=0;j<tail;++j){
                const Ev ev=j==0&&quiet?Ev::on:j==tail-1?Ev::off:Ev::none;
                const double peak=d.call(2,j,ev,true);s.continuation=std::max(s.continuation,peak);
            }
            d.snapshot();
        });
    }
    output(dir,cap,total);
    require(cap.states.size()==std::size_t(3*nplacements),"snapshot count");
    std::ofstream out(dir/"guards.csv");out.precision(17);
    out<<"placement,initial_peak,last64_peak,window_peak,continuation_peak\n";
    for(int p=0;p<nplacements;++p){
        const auto& s=stats[p];out<<p<<','<<s.initial<<','<<s.guard<<','<<s.observed<<','<<s.continuation<<'\n';
    }
    out.close();require(bool(out),"guard output");
    for(const auto& s:stats){
        if(wake||quiet) require(s.guard<1e-6,"final 64 setup callbacks not quiet");
        if(released||held) require(s.initial>1e-5,"held preparation inaudible");
        require(quiet?s.observed<1e-6:s.observed>1e-5,"wrong window quiet/audible behavior");
        require(s.continuation>1e-5,"inaudible continuation");
    }
}

int run(int argc,char** argv){
    require(argc==8,"zero-clock worker: STOCK OUT CPU LANE FD PLAN_DIR wake-two|full");
    const fs::path modulePath=fs::canonical(argv[1]),dir=argv[2],planDir=argv[6];
    const int cpu=number(argv[3]),lane=number(argv[4]),fd=number(argv[5]);
    require((lane==-1 && fd==-1)||((lane==0||lane==1)&&fd>=0),"lane/socket mismatch");
    const std::string scope=argv[7];require(scope=="wake-two"||scope=="full","zero-clock scope only");
    cpu_set_t mask;CPU_ZERO(&mask);require(cpu>=0 && cpu<CPU_SETSIZE,"invalid CPU");CPU_SET(cpu,&mask);
    require(sched_setaffinity(0,sizeof(mask),&mask)==0,"pin worker");checkPin(cpu);
    require(fs::create_directory(dir),"fresh worker directory");
    Module mod(modulePath.c_str());Dl_info info{};struct stat st{};
    require(dladdr(dlsym(mod.handle,"GetPluginFactory"),&info)!=0 && info.dli_fname &&
            fs::equivalent(modulePath,info.dli_fname) && stat(info.dli_fname,&st)==0 && S_ISREG(st.st_mode),"loaded ELF");
    writeControls(dir/"controls.csv",controls());
    Baton gate(lane,fd);std::ifstream ordered(planDir/"scenes.txt");std::string scene;std::vector<int> seen;
    std::fesetenv(FE_DFL_ENV);std::fenv_t initial;require(std::fegetenv(&initial)==0,"initial FP");
    while(std::getline(ordered,scene)){
        const auto found=std::find(scenes.begin(),scenes.end(),scene);require(found!=scenes.end(),"unknown scene");
        int n=int(found-scenes.begin());require(std::find(seen.begin(),seen.end(),n)==seen.end(),"duplicate scene");seen.push_back(n);
        require(scope=="full"||n==4,"unexpected scene in two-wake rehearsal");
        const auto plan=loadPlan(planDir/(scene+".csv"));const auto out=dir/scene;require(fs::create_directory(out),"fresh scene");
        require(std::fesetenv(&initial)==0,"restore initial scene FP");errno=41;
        runScene(mod,gate,n,scope=="full"?16:2,plan,out);checkPin(cpu);
    }
    require(ordered.eof() && seen.size()==(scope=="full"?5u:1u),"incomplete scene order");
    std::ofstream audit(dir/"baton.csv");audit<<"serial,scene,placement,phase,index,count,first,lane\n";
    for(const auto& v:gate.audit) audit<<v.serial<<','<<scenes[v.scene]<<','<<v.placement<<','<<v.phase<<','<<v.index<<','<<v.count<<','<<v.first<<','<<v.lane<<'\n';
    audit.close();require(bool(audit),"complete baton ledger");
    std::ofstream id(dir/"identity.json");id<<"{\"schema\":\"cs80-pair-dry-worker-v1\",\"callback_clock_reads\":0,\"lane\":"<<lane<<",\"cpu\":"<<cpu
        <<",\"dev\":"<<st.st_dev<<",\"ino\":"<<st.st_ino<<",\"size\":"<<st.st_size<<",\"visits\":"<<gate.serial<<"}\n";
    id.close();require(bool(id),"native identity");return 0;
}
}
int main(int argc,char**argv){try{return dry::run(argc,argv);}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

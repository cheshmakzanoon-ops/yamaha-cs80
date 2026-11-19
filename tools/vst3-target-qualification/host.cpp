// Separate Linux x86-64 instrumentation. The frozen experiment and plugin are unchanged.
#define main archived_factory8_main_not_used
#include "../../docs/experiments/eight-stage-filter/native-cost/host/factory8_base.cpp"
#undef main
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <charconv>

namespace interleave {
using namespace factory8;
namespace fs = std::filesystem;
constexpr int warmCalls = 1024, stripes = 32, width = 17;
constexpr int endWindow = warmCalls + stripes * width, totalCalls = endWindow + 1;
constexpr std::array<const char*, 3> names{"S0", "S1", "S+"};
using Perm = std::array<int, 3>;

struct Schedule {
    Perm physical{}; // Indexed by role.
    std::array<Perm, stripes> order{}; // Each row is a sequence of roles.
    explicit Schedule(const fs::path& file) {
        std::ifstream stream(file);
        auto read = [&] {
            std::string s; require(bool(std::getline(stream, s)), "missing order");
            auto sorted = s; std::sort(sorted.begin(), sorted.end());
            require(sorted == "012", "invalid permutation");
            return Perm{s[0]-'0', s[1]-'0', s[2]-'0'};
        };
        physical = read();
        for (auto& row : order) row = read();
        std::string extra; require(!std::getline(stream, extra) && stream.eof(), "trailing order");
    }
};

void pin(int cpu) {
    cpu_set_t mask;
    require(cpu >= 0 && cpu < CPU_SETSIZE && sched_getaffinity(0, sizeof(mask), &mask) == 0 &&
            CPU_ISSET(cpu, &mask), "requested CPU unavailable");
    CPU_ZERO(&mask); CPU_SET(cpu, &mask);
    require(sched_setaffinity(0, sizeof(mask), &mask) == 0, "pin benchmark thread");
}
void verifyCpu(int cpu) {
    cpu_set_t mask;
    require(sched_getaffinity(0, sizeof(mask), &mask) == 0 && CPU_COUNT(&mask) == 1 &&
            CPU_ISSET(cpu, &mask) && sched_getcpu() == cpu, "thread migrated or affinity changed");
}
std::uint64_t ns(timespec t) {
    require(t.tv_sec >= 0 && t.tv_nsec >= 0 && t.tv_nsec < 1000000000L, "invalid clock value");
    return std::uint64_t(t.tv_sec) * 1000000000ULL + std::uint64_t(t.tv_nsec);
}

struct Record {
    std::array<float, 128> pcm{};
    FP before{}, after{}, next{}; double probe{};
    tresult result{}; std::uint64_t silence{}; unsigned reads{}, countReads{};
    std::array<std::uint64_t, 4> time{}; // Outer wall, inner thread CPU.
};

struct Lane {
    Instance instance; const fs::path path; HeldEvents events;
    std::array<float,64> left{}, right{};
    float* outputPtrs[2]{left.data(), right.data()}; float* inputPtrs[1]{nullptr};
    AudioBusBuffers input{}, output{}; ProcessContext context{}; ProcessData data{};
    std::vector<Record> records = std::vector<Record>(totalCalls);
    std::fenv_t env{}; int error = 41, cursor = 0; std::array<std::string, 4> states;
    Lane(Module& module, const cs80::ParamValues& parameters, const fs::path& dir)
      : instance(module, parameters, 0, 0), path(dir) {
        require(fs::create_directory(path), "fresh lane");
        require(instance.audio->canProcessSampleSize(kSample32) == kResultTrue, "sample32");
        require(instance.component->getBusCount(kAudio,kInput) == 1 &&
                instance.component->getBusCount(kAudio,kOutput) == 1 &&
                instance.component->getBusCount(kEvent,kInput) == 1, "native bus count");
        BusInfo in{}, out{};
        require(instance.component->getBusInfo(kAudio,kInput,0,in) == kResultOk && in.channelCount == 1 &&
                instance.component->getBusInfo(kAudio,kOutput,0,out) == kResultOk && out.channelCount == 2 &&
                instance.component->activateBus(kAudio,kInput,0,false) == kResultOk, "native audio buses");
        observeMetadata(instance,path,"initial"); states[0] = state(instance);
        for (int n=0; n<8; ++n) {
            auto& e = events.values[n]; e.type = Event::kNoteOnEvent; e.busIndex=0; e.sampleOffset=0;
            e.noteOn.channel=0; e.noteOn.pitch=int16(notes[n]); e.noteOn.velocity=102.0f/127.0f; e.noteOn.noteId=n;
        }
        output.numChannels=2; output.channelBuffers32=outputPtrs;
        input.numChannels=1; input.channelBuffers32=inputPtrs; input.silenceFlags=1;
        context.sampleRate=48000;
        data.processMode=kRealtime; data.symbolicSampleSize=kSample32; data.numSamples=64;
        data.numInputs=1; data.inputs=&input; data.numOutputs=1; data.outputs=&output;
        data.inputEvents=&events; data.processContext=&context;
        require(std::fesetenv(FE_DFL_ENV) == 0, "reset FP"); _mm_setcsr(0x9fc0);
        require(std::fegetenv(&env) == 0, "initial lane FP");
    }
    void visit(int begin, int count, int clocked) {
        require(begin == cursor && count > 0 && begin+count <= totalCalls &&
                clocked >= 0 && clocked <= count, "lane timeline");
        require(std::fesetenv(&env) == 0, "restore lane FP"); errno=error;
        for (int k=0; k<count; ++k) {
            const int b=cursor++;
            auto& r=records[b];
            left.fill(0); right.fill(0); output.silenceFlags=0;
            context.projectTimeSamples=int64(b)*64; events.count=b==0 ? 8 : 0; events.reads=events.countReads=0;
            r.before=fp();
            if (k < clocked) {
                timespec w0{}, c0{}, c1{}, w1{};
                const int a=clock_gettime(CLOCK_MONOTONIC,&w0), d=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&c0);
                r.result=instance.audio->process(data);
                const int e=clock_gettime(CLOCK_THREAD_CPUTIME_ID,&c1), f=clock_gettime(CLOCK_MONOTONIC,&w1);
                require(a==0 && d==0 && e==0 && f==0,"clock read failure");
                r.time={ns(w0),ns(c0),ns(c1),ns(w1)};
            } else {
                r.result=instance.audio->process(data);
            }
            r.after=fp(); r.silence=output.silenceFlags;
            r.reads=events.reads; r.countReads=events.countReads;
            std::fenv_t saved; const int savedError=errno;
            require(std::fegetenv(&saved)==0,"save post-callback FP");
            volatile double one=1.0, delta=0x1p-54; volatile double probe=one+delta;
            r.next=fp(); r.probe=probe;
            for (int n=0; n<64; ++n) { r.pcm[2*n]=left[n]; r.pcm[2*n+1]=right[n]; }
            require(std::fesetenv(&saved)==0,"restore post-callback FP"); errno=savedError;
        }
        error=errno; require(std::fegetenv(&env)==0,"save lane FP");
    }
    void snapshot(int phase) {
        // Run only after ALL instances finish the phase. Preserve each caller's FP
        // history and errno independently of snapshots and other lanes.
        require(std::fesetenv(&env)==0,"restore snapshot FP"); errno=error;
        states[phase]=state(instance);
    }
    void finish() {
        require(cursor==totalCalls,"complete lane timeline");
        require(std::fesetenv(&env)==0,"restore final lane FP"); errno=error;
        std::ofstream pcm(path/"pcm.bin",std::ios::binary), trace(path/"callbacks.bin",std::ios::binary);
        unsigned nonzero=0;
        for (unsigned b=0; b<records.size(); ++b) {
            const auto& r=records[b];
            require(r.result==kResultOk && r.reads==(b==0 ? 8u : 0u),"callback result or MIDI receipt");
            for (int n=0; n<64; ++n) {
                require(std::isfinite(r.pcm[2*n]) && std::isfinite(r.pcm[2*n+1]),"nonfinite PCM");
                nonzero+=r.pcm[2*n]!=0 || r.pcm[2*n+1]!=0;
            }
            pcm.write(reinterpret_cast<const char*>(r.pcm.data()),sizeof(r.pcm));
            put(trace,b); put(trace,r.result); put(trace,r.silence); put(trace,r.reads); put(trace,r.countReads);
            putFP(trace,r.before); putFP(trace,r.after); putFP(trace,r.next); put(trace,r.probe);
        }
        pcm.close(); trace.close();
        require(bool(pcm) && bool(trace) && nonzero==totalCalls*64u,"complete nonvacuous capture");
        const char* files[]={"initial.state","postwarm.state","postwindow.state","continuation.state"};
        for (int k=0; k<4; ++k) { require(!states[k].empty(),"empty component state"); save(path/files[k],states[k]); }
        observeMetadata(instance,path,"final");
    }
};

struct OS {
    std::uint64_t wall; long voluntary, involuntary, minor, major;
};
OS observe(int cpu) {
    verifyCpu(cpu); rusage ru{}; timespec wall{};
    require(getrusage(RUSAGE_THREAD,&ru)==0 && clock_gettime(CLOCK_MONOTONIC,&wall)==0,"OS boundary");
    return {ns(wall),ru.ru_nvcsw,ru.ru_nivcsw,ru.ru_minflt,ru.ru_majflt};
}
struct Slot { int stripe, position, role, physical; OS before, after; };
struct Resolution { std::uint64_t cpu, wall; };
Resolution resolution() {
    timespec c{}, w{};
    require(clock_getres(CLOCK_THREAD_CPUTIME_ID,&c)==0 && clock_getres(CLOCK_MONOTONIC,&w)==0,"clock resolution");
    return {ns(c),ns(w)};
}

int run(const std::string& mode, const char* modulePath, const fs::path& dir, int cpu, const Schedule& plan) {
    const bool single=mode=="reference", timed=mode=="measure"; const int nLanes=single?1:3;
    require(single || timed || mode=="untimed","unsupported mode");
    require(!timed || std::getenv("CS80_STOCK_INTERLEAVE_SESSION"),"measure via qualified collector");
    require(fs::create_directory(dir), "fresh capture directory required");
    pin(cpu); verifyCpu(cpu);
    const auto file=fs::canonical(modulePath); struct stat expected{}, actual{};
    require(::stat(file.c_str(),&expected)==0 && S_ISREG(expected.st_mode),"regular native image");
    Module module(file.c_str()); Dl_info loaded{};
    require(dladdr(dlsym(module.handle,"GetPluginFactory"),&loaded)!=0 && loaded.dli_fname &&
            fs::canonical(loaded.dli_fname)==file && ::stat(loaded.dli_fname,&actual)==0 &&
            expected.st_dev==actual.st_dev && expected.st_ino==actual.st_ino,"actual loaded stock image");
    auto p=controls(); writeControls(dir/"controls.csv",p);
    std::array<std::unique_ptr<Lane>,3> lanes;
    std::fenv_t startup; const int startupError=errno;
    require(std::fegetenv(&startup)==0,"save common component initialization FP");
    for (int i=0; i<nLanes; ++i) {
        require(std::fesetenv(&startup)==0,"common component initialization FP"); errno=startupError;
        lanes[i]=std::make_unique<Lane>(module,p,dir/("lane"+std::to_string(i)));
        for (int j=0; j<i; ++j) require(lanes[j]->instance.component!=lanes[i]->instance.component &&
                                            lanes[j]->instance.audio!=lanes[i]->instance.audio,"distinct instances required");
    }
    auto physical=[&](int role) { return single ? 0 : plan.physical[role]; };
    // Rotate warm-ups as well; the first note event is delivered only at timeline block zero.
    for (int w=0; w<stripes; ++w) for (int pos=0; pos<nLanes; ++pos)
        lanes[physical(single?0:plan.order[w][pos])]->visit(w*32,32,0);
    for (int i=0; i<nLanes; ++i) lanes[i]->snapshot(1);
    // The host does not allocate, hash, serialize, or perform disk IO in this loop.
    std::array<Slot,stripes*3> slots{}; int used=0;
    for (int s=0; s<stripes; ++s) for (int pos=0; pos<nLanes; ++pos) {
        const int role=single ? 0 : plan.order[s][pos], lane=physical(role);
        auto& slot=slots[used++]; slot.stripe=s; slot.position=pos; slot.role=role; slot.physical=lane;
        slot.before=observe(cpu);
        lanes[lane]->visit(warmCalls+s*width,width,timed ? (role==2 ? width : width-1) : 0);
        slot.after=observe(cpu);
    }
    for (int i=0; i<nLanes; ++i) lanes[i]->snapshot(2);
    for (int i=0; i<nLanes; ++i) lanes[i]->visit(endWindow,1,0);
    for (int i=0; i<nLanes; ++i) lanes[i]->snapshot(3);
    for (int i=0; i<nLanes; ++i) lanes[i]->finish();
    std::ofstream raw(dir/"clocks.csv"), os(dir/"os.csv");
    raw<<"stripe,position,role,lane,index,global_block,wall_start_ns,cpu_start_ns,cpu_end_ns,wall_end_ns\n";
    os<<"stripe,position,role,lane,wall_start_ns,wall_end_ns,voluntary_switches,involuntary_switches,minor_faults,major_faults\n";
    for (int i=0; i<used; ++i) {
        const auto& slot=slots[i]; const auto& a=slot.before; const auto& b=slot.after;
        os<<slot.stripe<<','<<slot.position<<','<<names[slot.role]<<','<<slot.physical<<','<<a.wall<<','<<b.wall<<','
          <<b.voluntary-a.voluntary<<','<<b.involuntary-a.involuntary<<','<<b.minor-a.minor<<','<<b.major-a.major<<'\n';
        for (int j=0; j<width; ++j) {
            const int block=warmCalls+slot.stripe*width+j;
            const auto& t=lanes[slot.physical]->records[block].time;
            raw<<slot.stripe<<','<<slot.position<<','<<names[slot.role]<<','<<slot.physical<<','<<j<<','<<block;
            for (auto v:t) raw<<','<<v;
            raw<<'\n';
        }
    }
    raw.close(); os.close(); require(bool(raw)&&bool(os),"complete clock files");
    const auto res=resolution(); verifyCpu(cpu);
    std::ofstream id(dir/"identity.json");
    id<<"{\"schema\":\"stock-interleave-host-v1\",\"mode\":\""<<mode<<"\",\"cpu\":"<<cpu<<",\"lanes\":"<<nLanes
      <<",\"warm_calls_per_lane\":"<<warmCalls<<",\"window_calls_per_lane\":"<<stripes*width
      <<",\"continuation_calls_per_lane\":1,\"sample_rate\":48000,\"block_size\":64,\"module_dev\":"<<actual.st_dev
      <<",\"module_ino\":"<<actual.st_ino<<",\"module_size\":"<<actual.st_size<<",\"thread_clock_resolution_ns\":"<<res.cpu
      <<",\"wall_clock_resolution_ns\":"<<res.wall<<"}\n";
    id.close(); require(bool(id),"complete host identity");
    return 0;
}
}
int main(int argc, char** argv) {
    try {
        require(argc==6,"reference|untimed|measure STOCK_MODULE NEW_OUT CPU SCHEDULE_FILE");
        std::string id=argv[4]; int cpu{};
        const auto [p,ec]=std::from_chars(id.data(),id.data()+id.size(),cpu);
        require(ec==std::errc{} && p==id.data()+id.size(),"integer CPU id");
        return interleave::run(argv[1],argv[2],argv[3],cpu,interleave::Schedule(argv[5]));
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

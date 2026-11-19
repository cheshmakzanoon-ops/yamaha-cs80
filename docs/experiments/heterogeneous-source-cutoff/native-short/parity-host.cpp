// This separate sampling executable is never used for CPU/latency qualification.
// Reuse the historical real-VST3 module/instance/event/physical-control adapters.
#define main archived_factory8_main_not_called
#include "../../docs/experiments/eight-stage-filter/native-cost/host/factory8_base.cpp"
#undef main
#include "pc_sampler.h"
#include <chrono>

extern "C" [[gnu::noinline, gnu::noclone]] std::uint64_t cs80_profile_calibration(std::uint64_t x) {
    for (unsigned i=0;i<1000000;++i) { x=x*6364136223846793005ull+1; asm volatile("" : "+r"(x)); }
    return x;
}
namespace {
std::ofstream allPcm,allFP,allStates;
void keepState(Instance& inst) {
    std::fenv_t f;std::fegetenv(&f);const int e=errno;const std::string state=factory8::state(inst);
    const std::uint64_t n=state.size();factory8::put(allStates,n);allStates.write(state.data(),std::streamsize(n));
    require(bool(allStates),"complete state record");std::fesetenv(&f);errno=e;
}
bool variedDrives=false; enum class EventType { none, on, off };
struct Stats {
    std::uint64_t frames{}, nonzero{}, hash=14695981039346656037ull;
    unsigned calls{}, noteOns{}, noteOffs{};
    double peak{}, power{};
    void consume(float x) {
        require(std::isfinite(x),"non-finite host output");
        const auto bits=std::bit_cast<std::uint32_t>(x);
        for (int k=0;k<4;++k) { hash^=(bits>>(8*k))&255u; hash*=1099511628211ull; }
        nonzero+=x!=0; peak=std::max(peak,std::abs(double(x))); power+=double(x)*x;
    }
};
struct Driver {
    Instance i;
    factory8::HeldEvents events;
    std::array<float,64> l{},r{};
    float* outs[2]={l.data(),r.data()}; float* ins[1]={nullptr};
    AudioBusBuffers out{},in{};
    ProcessContext context{}; ProcessData data{};
    int64 cursor{};
    explicit Driver(Module& m):i(m,factory8::controls(),0,0) {
        require(i.audio->canProcessSampleSize(kSample32)==kResultTrue,"native sample32");
        require(i.component->activateBus(kAudio,kInput,0,false)==kResultOk,"disable external input");
        out.numChannels=2;out.channelBuffers32=outs;in.numChannels=1;in.channelBuffers32=ins;in.silenceFlags=1;
        context.sampleRate=48000.;
        data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;
        data.numInputs=1;data.inputs=&in;data.numOutputs=1;data.outputs=&out;data.inputEvents=&events;data.processContext=&context;
        std::fesetenv(FE_DFL_ENV); _mm_setcsr(0x9fc0); errno=41;
    }
    void render(EventType ev=EventType::none,Stats* s=nullptr,pcs::Region region=pcs::outside) {
        l.fill(0);r.fill(0);out.silenceFlags=0;context.projectTimeSamples=cursor;cursor+=64;
        events.count=ev==EventType::none?0:8;events.reads=events.countReads=0;
        for (int v=0;v<events.count;++v) {
            auto& e=events.values[std::size_t(v)]; e={}; e.busIndex=0;e.sampleOffset=0;
            if (ev==EventType::on) {e.type=Event::kNoteOnEvent;e.noteOn.channel=0;e.noteOn.pitch=int16(factory8::notes[v]);
                e.noteOn.velocity=variedDrives?float(44+10*v)/127.f:102.f/127.f;e.noteOn.noteId=v;}
            else {e.type=Event::kNoteOffEvent;e.noteOff.channel=0;e.noteOff.pitch=int16(factory8::notes[v]);e.noteOff.noteId=v;}
        }
        pcs::mark(region);
        const auto before=factory8::fp(); const auto result=i.audio->process(data); const auto after=factory8::fp();
        // Capture all calls, including warm/release wait; comparisons are never timing inputs.
        std::fenv_t saved;std::fegetenv(&saved);const int oldErrno=errno;volatile double one=1.,delta=0x1p-54;volatile double pr=one+delta;const auto next=factory8::fp();const double value=pr;
        for(auto f:{before,after,next})factory8::putFP(allFP,f);
        factory8::put(allFP,value);factory8::put(allFP,result);factory8::put(allFP,out.silenceFlags);factory8::put(allFP,events.reads);factory8::put(allFP,events.countReads);
        std::array<float,128> samples{};for(int k=0;k<64;++k){samples[2*k]=l[k];samples[2*k+1]=r[k];}
        allPcm.write(reinterpret_cast<const char*>(samples.data()),sizeof(samples));require(bool(allPcm),"complete PCM");
        std::fesetenv(&saved);errno=oldErrno;
        pcs::mark(pcs::outside);
        require(result==kResultOk && events.reads==unsigned(events.count),"failed process or wrong event read count");
        if (s) {
            for(int n=0;n<64;++n) {s->consume(l[n]);s->consume(r[n]);}
            s->frames+=64;++s->calls;
            if(ev==EventType::on) s->noteOns+=events.reads;
            if(ev==EventType::off) s->noteOffs+=events.reads;
        } else {
            for (int n=0;n<64;++n) require(std::isfinite(l[n])&&std::isfinite(r[n]),"non-finite preparation");
        }
    }
};
std::uint64_t threadNs() {
    timespec t{};require(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0,"calibration clock");
    return std::uint64_t(t.tv_sec)*1000000000ull+std::uint64_t(t.tv_nsec);
}
void calibrate() {
    auto t=threadNs(); std::uint64_t sum=1;
    do {
        pcs::mark(pcs::calibration);sum=cs80_profile_calibration(sum);pcs::mark(pcs::outside);
    } while(threadNs()-t<150000000ull);
    asm volatile("" :: "r"(sum));
}
void plan(bool yes,const char* msg) { require(yes,msg); }
int run(const std::filesystem::path& modulePath,const std::string& scene,unsigned period,const std::filesystem::path& dir) {
    variedDrives=scene.starts_with("varied-"); const bool held=scene=="held8"||scene=="varied-held", fresh=scene=="fresh-idle", released=scene=="post-release"||scene=="varied-release", wake=scene=="wake"||scene=="varied-wake";
    plan(held||fresh||released||wake,"scene must be held8|fresh-idle|post-release|wake");
    plan(period==0||period==997||period==3109,"period must be 0 (no sampling), 997 or 3109 microseconds");
    require(std::filesystem::create_directory(dir),"output directory must be new");
    require(period==0,"never sample or time in parity host");
    allPcm.open(dir/"all.pcm",std::ios::binary);allFP.open(dir/"all.fp",std::ios::binary);allStates.open(dir/"states.bin",std::ios::binary);
    Module module(modulePath.c_str()); void* entry=dlsym(module.handle,"GetPluginFactory");
    Dl_info image{}; require(entry && dladdr(entry,&image)!=0 && image.dli_fname &&
            std::filesystem::equivalent(modulePath,image.dli_fname),"loaded module identity");
    pcs::Sampler sampler(period);
    if(period) calibrate();
    Stats observed, warmHeld, quietGuard, transitions;
    constexpr unsigned warm=1024, guard=64, releaseWait=8192, wakeCycles=16;
    const unsigned take=held?4096u:wake?32u:16384u, repeats=wake?wakeCycles:1;
    for(unsigned repeat=0;repeat<repeats;++repeat) {
        Driver d(module);keepState(d.i);
        for(unsigned b=0;b<warm;++b) {
            const bool play=held||released;
            d.render(play&&b==0?EventType::on:EventType::none,play?&warmHeld:(b>=warm-guard?&quietGuard:nullptr));
        }
        keepState(d.i);
        if(released) {
            for(unsigned b=0;b<releaseWait;++b)
                d.render(b==0?EventType::off:EventType::none,b==0?&transitions:(b>=releaseWait-guard?&quietGuard:nullptr));
        }
        if(released)keepState(d.i);
        for(unsigned b=0;b<take;++b) {
            const bool onset=wake&&b==0;
            d.render(onset?EventType::on:EventType::none,&observed,onset?pcs::wakeFirst:pcs::callback);
        }
        keepState(d.i);
    }
    sampler.stop();
    pcs::write(dir,entry);
    require(observed.calls==take*repeats && observed.frames==std::uint64_t(take)*repeats*64 &&
            warmHeld.noteOns==((held||released)?8u:0u) && transitions.noteOffs==(released?8u:0u) &&
            observed.noteOns==(wake?repeats*8:0u),"incomplete callback/event census");
    // -120 dBFS peak guard distinguishes public near-silence from exact zeros
    // or a private claim that every voice is inactive (not observable via VST3).
    const bool quiet=quietGuard.peak<=1e-6 && (!(released||fresh)||observed.peak<=1e-6);
    const bool audible=(!held&&!released||warmHeld.peak>1e-5) && (!held&&!wake||observed.peak>1e-5);
    std::ofstream meta(dir/"scene.json");
    meta<<std::setprecision(17)<<"{\"scene\":\""<<scene<<"\",\"period_us\":"<<period
        <<",\"rate\":48000,\"block\":64,\"placements\":"<<repeats<<",\"warm_calls_per_instance\":"<<warm
        <<",\"release_wait_calls\":"<<(released?releaseWait:0)<<",\"region_calls\":"<<observed.calls
        <<",\"region_frames\":"<<observed.frames<<",\"warm_note_on_reads\":"<<warmHeld.noteOns
        <<",\"release_note_off_reads\":"<<transitions.noteOffs<<",\"region_note_on_reads\":"<<observed.noteOns
        <<",\"region_nonzero_samples\":"<<observed.nonzero<<",\"region_peak\":"<<observed.peak
        <<",\"region_rms\":"<<std::sqrt(observed.power/(2.0*observed.frames))<<",\"region_pcm_fnv1a64\":\""
        <<std::hex<<std::setw(16)<<std::setfill('0')<<observed.hash<<std::dec<<"\",\"quiet_guard_frames\":"<<quietGuard.frames
        <<",\"quiet_guard_peak\":"<<quietGuard.peak<<",\"warm_held_peak\":"<<warmHeld.peak
        <<",\"quiet_admission\":"<<(quiet?"true":"false")<<",\"audible_admission\":"<<(audible?"true":"false")<<"}\n";
    meta.close();require(bool(meta),"scene receipt write");
    require(quiet && audible,"public quiet/audible guard failed; retain scene receipt");
    return 0;
}
} // namespace
int main(int argc,char** argv) {
    try {
        require(argc==5,"usage: profile_host MODULE held8|fresh-idle|post-release|wake 0|997|3109 NEW_DIRECTORY");
        const std::string period=argv[3]; require(period=="0"||period=="997"||period=="3109","invalid timer period");
        return run(argv[1],argv[2],unsigned(std::stoul(period)),argv[4]);
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}

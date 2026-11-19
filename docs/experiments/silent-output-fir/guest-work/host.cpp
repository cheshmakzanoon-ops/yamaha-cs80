#define main cs80_historical_factory8_main
#include "factory8_base.cpp"
#undef main
#include <valgrind/callgrind.h>
#include <cpuid.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

extern "C" __attribute__((noinline,noclone)) tresult cs80_counted_process(IAudioProcessor* p,ProcessData* d){
    auto result=p->process(*d); asm volatile("" : "+r"(result) : : "memory"); return result;
}
namespace {
void putFile(const std::filesystem::path& f,const std::string& contents){
    std::ofstream s(f,std::ios::binary); s.write(contents.data(),std::streamsize(contents.size())); require(bool(s),"file write");
}
void snapshot(Instance& i,const std::filesystem::path& f){
    std::fenv_t saved;std::fegetenv(&saved);int e=errno;putFile(f,factory8::state(i));std::fesetenv(&saved);errno=e;
}
unsigned long long xcr0(){ unsigned a{},b{},c{},d{};if(!__get_cpuid(1,&a,&b,&c,&d)||!(c&(1u<<27)))return 0;asm volatile("xgetbv":"=a"(a),"=d"(d):"c"(0));return (static_cast<std::uint64_t>(d)<<32)|a; }
int run(int argc,char** argv){
    require(argc==5,"MODULE fresh-quiet|held-short|settled-release first4|all5|fifth NEW_OUTPUT");
    std::string scene=argv[2],kind=argv[3];const bool released=scene=="settled-release",played=scene=="held-short"||released;
    const unsigned warm=released?1024+8192:32,w=5,total=warm+w+1;
    require((played||scene=="fresh-quiet")&&(kind=="first4"||kind=="all5"||kind=="fifth"),"unknown workload");
    std::filesystem::path output=argv[4],modulePath=std::filesystem::canonical(argv[1]);
    require(std::filesystem::create_directory(output),"fresh output dir required");Module module(modulePath.c_str());
    Dl_info image{};require(dladdr(dlsym(module.handle,"GetPluginFactory"),&image)&&image.dli_fname&&std::filesystem::equivalent(modulePath,image.dli_fname),"loaded module mismatch");
    Instance i(module,factory8::controls(),0,0);require(i.audio->canProcessSampleSize(kSample32)==kResultTrue&&i.component->activateBus(kAudio,kInput,0,false)==kResultOk,"sample32 input");
    factory8::HeldEvents events;std::array<float,64> l{},r{};float* outs[]={l.data(),r.data()};float* ins[]={nullptr};
    AudioBusBuffers out{},in{};out.numChannels=2;out.channelBuffers32=outs;in.numChannels=1;in.channelBuffers32=ins;in.silenceFlags=1;
    ProcessContext ctx{};ctx.sampleRate=48000.;ProcessData data{};data.processMode=kRealtime;data.symbolicSampleSize=kSample32;data.numSamples=64;data.numInputs=1;data.inputs=&in;data.numOutputs=1;data.outputs=&out;data.inputEvents=&events;data.processContext=&ctx;
    for(int v=0;v<8;++v){auto&e=events.values[v];e={};e.type=Event::kNoteOnEvent;e.busIndex=0;e.sampleOffset=0;e.noteOn.channel=0;e.noteOn.pitch=int16(factory8::notes[v]);e.noteOn.velocity=102.f/127.f;e.noteOn.noteId=v;}
    snapshot(i,output/"initial.state");std::ofstream pcm(output/"pcm.bin",std::ios::binary),fp(output/"fp.bin",std::ios::binary);
    std::fesetenv(FE_DFL_ENV);_mm_setcsr(0x9fc0);errno=41;unsigned callbacks=0,counted=0,reads=0;
    double peak=0,windowPeak=0;const bool under=RUNNING_ON_VALGRIND!=0;__builtin_cpu_init();
    for(unsigned b=0;b<total;++b){
        const bool inWindow=b>=warm&&b<warm+w;
        const bool selected=inWindow&&(kind=="all5"||(kind=="first4"?b<warm+4:b==warm+4));
        l.fill(0);r.fill(0);out.silenceFlags=0;ctx.projectTimeSamples=int64(b)*64;events.count=played&&b==0?8:0;events.countReads=events.reads=0;
        if(released&&b==1024){events.count=8;for(int v=0;v<8;++v){auto&e=events.values[v];e={};e.type=Event::kNoteOffEvent;e.busIndex=0;e.sampleOffset=0;e.noteOff.channel=0;e.noteOff.pitch=int16(factory8::notes[v]);e.noteOff.noteId=v;}}
        if(b==warm){CALLGRIND_START_INSTRUMENTATION;CALLGRIND_ZERO_STATS;}
        const auto before=factory8::fp();
        tresult result=selected ? cs80_counted_process(i.audio,&data) : i.audio->process(data);
        const auto after=factory8::fp();
        if(b==warm+w-1){CALLGRIND_DUMP_STATS_AT("selected_callbacks");CALLGRIND_STOP_INSTRUMENTATION;}
        ++callbacks;counted+=selected;reads+=events.reads;require(result==kResultOk&&events.reads==unsigned(events.count),"callback or event failure");
        std::fenv_t f;std::fegetenv(&f);int e=errno;volatile double one=1.,delta=0x1p-54;volatile double probe=one+delta;const auto next=factory8::fp();const double value=probe;
        factory8::put(fp,result);factory8::put(fp,out.silenceFlags);factory8::put(fp,events.reads);factory8::put(fp,events.countReads);
        for(auto v:{before,after,next})factory8::putFP(fp,v);factory8::put(fp,value);
        for(int k=0;k<64;++k)for(float s:{l[k],r[k]}){
            require(std::isfinite(s),"non-finite PCM");peak=std::max(peak,std::abs(double(s)));if(inWindow)windowPeak=std::max(windowPeak,std::abs(double(s)));factory8::put(pcm,s);
        }
        std::fesetenv(&f);errno=e;
        if(b==warm-1)snapshot(i,output/"postwarm.state");if(b==warm+w-1)snapshot(i,output/"window.state");if(b==total-1)snapshot(i,output/"continuation.state");
    }
    require(callbacks==total&&counted==(kind=="first4"?4u:kind=="all5"?5u:1u)&&reads==(released?16u:played?8u:0u),"callback ledger");
    require((!played||peak>1e-5)&&(scene=="held-short"?windowPeak>1e-5:windowPeak<=1e-6),"audible/quiet guard");
    std::ostringstream m;m<<std::setprecision(17)<<"{\"scene\":"<<std::quoted(scene)<<",\"arm_window\":"<<std::quoted(kind)<<",\"module\":"<<std::quoted(modulePath.string())<<",\"under_valgrind\":"<<under<<",\"cpu_avx2\":"<<(!!__builtin_cpu_supports("avx2"))<<",\"cpu_avx512f\":"<<(!!__builtin_cpu_supports("avx512f"))<<",\"xcr0\":"<<xcr0()<<",\"window_peak\":"<<windowPeak<<",\"peak\":"<<peak<<",\"counted_calls\":"<<counted<<",\"genuine_calls\":"<<callbacks<<",\"warm\":"<<warm<<",\"window\":"<<w<<",\"note_event_reads\":"<<reads<<"}\n";putFile(output/"outcome.json",m.str());
    return 0;
}
}
int main(int argc,char**argv){try{return run(argc,argv);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

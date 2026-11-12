// Reuse only the normal-input native module host/renderer. The historical
// suite's boundary/fuzz/other feature fixtures and main are not compiled here.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include <fstream>
namespace {
constexpr std::array oldRoster {
    int(cs80::P_volume),int(cs80::P_mix),int(cs80::P_brilliance),int(cs80::P_resonance),int(cs80::P_masterTune),int(cs80::P_detune),
    cs80::chParam(0,cs80::CP_hpfCutoff),cs80::chParam(0,cs80::CP_hpfRes),cs80::chParam(0,cs80::CP_lpfCutoff),
    cs80::chParam(0,cs80::CP_lpfRes),cs80::chParam(0,cs80::CP_vLevel),cs80::chParam(0,cs80::CP_pw),
    cs80::chParam(1,cs80::CP_hpfCutoff),cs80::chParam(1,cs80::CP_hpfRes),cs80::chParam(1,cs80::CP_lpfCutoff),
    cs80::chParam(1,cs80::CP_lpfRes),cs80::chParam(1,cs80::CP_vLevel),cs80::chParam(1,cs80::CP_pw),
    cs80::chParam(0,cs80::CP_pan),cs80::chParam(1,cs80::CP_pan)};
void capture(const std::filesystem::path& path,const std::vector<float>& audio) {
    std::ofstream file(path,std::ios::binary);require(bool(file),"capture open");
    file.write(reinterpret_cast<const char*>(audio.data()),std::streamsize(audio.size()*sizeof(float)));
    require(bool(file),"complete capture write");
}
Curve subCurve() {return {{511,.1},{1023,.85},{1024,.2},{2047,.75},{3071,.3},{4095,.65}};}
bool readSupportedPoint(void* context,int index,int& offset,double& value) noexcept {
    const auto& points=*static_cast<const Curve*>(context);
    if(index<0 || size_t(index)>=points.size())return false;
    offset=points[size_t(index)].first;value=points[size_t(index)].second;return true;
}
void validScalarCheck(const ws1::Tolerances& limits) {
    for(Curve points:{Curve{{4095,.4}},subCurve(),Curve{{0,.2},{1,.8},{2047,.8},{4095,.1}}}) {
        cs80::HostAutomationCurve curve;
        curve.begin({&points,int(points.size()),readSupportedPoint,nullptr,nullptr},.17);
        double error=0;
        for(int sample=0;sample<4096;++sample)
            error=std::max(error,std::abs(curve.valueAt(sample)-absoluteValue(points,.17,sample)));
        check(error<=limits["normalized_curve_error"],"supported finite scalar curve preserves registered normalized limit");
    }
}

}
int main(int argc,char**argv) {
    try {
        require(argc==5,"usage: sub-test MODULE ORIGINAL_CONTRACT SUB_CONTRACT OUTPUT_DIRECTORY");
        const ws1::Tolerances limits(argv[2],"host_automation"),subLimits(argv[3],"host_automation_sub");
        require(cs80::kSampleAccurateParameters.size()==subLimits["key_parameter_count"],"registered22-lane roster");
        require(std::equal(oldRoster.begin(),oldRoster.end(),cs80::kSampleAccurateParameters.begin()),"entire original20-lane prefix/order unchanged");
        require(cs80::kSampleAccurateParameters[20]==cs80::P_subOriginalSpeed && cs80::kSampleAccurateParameters[21]==cs80::P_subVco,"exact two appended SUB lanes");
        std::filesystem::create_directories(argv[4]);
        const std::filesystem::path out(argv[4]);
        validScalarCheck(limits);
        Module module(argv[1]);
        auto parameters=cs80::defaultValues();
        parameters[cs80::P_sourcePerformance]=1;parameters[cs80::P_parityMode]=0;
        parameters[cs80::P_drift]=parameters[cs80::P_voiceDispersion]=parameters[cs80::P_analogNoise]=0;
        parameters[cs80::P_subOriginalMode]=1;parameters[cs80::P_subOriginalFunction]=0;
        parameters[cs80::P_subOriginalSpeed]=12;parameters[cs80::P_subVco]=.7f;
        int scenarios=0;
        for(int voices:{8,16}) for(int source:{13,0,11}) for(size_t lane:{20u,21u}) {
            auto p=parameters;p[cs80::P_voiceMode]=voices==16?1.f:0.f;
            Curves curves;curves[lane]=subCurve();
            const auto scalar=render(module,p,curves,1,true,false,source);
            double error=0;
            for(int block:{7,64,256,1024,4096}) error=std::max(error,difference(scalar,render(module,p,curves,block,true,false,source)));
            check(error<=limits["audio_max_abs_error"],"SUB supported native queues match per-sample reference across partitions");
            const auto neutral=render(module,p,curves,256,false,false,source);
            const double effect=difference(scalar,neutral);
            check(effect>0.00001,"SUB modulation positive nonzero PCM witness");
            check(neutral==render(module,p,curves,256,false,false,source,true),"empty/no queue unchanged");
            const std::string id=std::to_string(voices)+"-"+std::to_string(source)+"-"+std::to_string(lane);
            capture(out/(id+"-noqueue.pcm"),neutral);
            for(int policy:{0,2}) {
                auto protectedP=p;protectedP[cs80::P_sourcePerformance]=policy==0?0.f:1.f;protectedP[cs80::P_parityMode]=policy==2?1.f:0.f;
                const auto actual=render(module,protectedP,curves,256,true,false,source);
                check(actual==render(module,protectedP,curves,256,true,true,source),"Legacy/PURE retain flattened native queues");
                capture(out/(id+"-policy"+std::to_string(policy)+".pcm"),actual);
            }
            ++scenarios;
            std::cout<<"scenario="<<id<<" max_error="<<error<<" effect="<<effect<<'\n';
        }
        // Existing20-lane modern trajectories retain their numerical contract,
        // including old pan authority; no new SUB curve participates here.
        auto old=curvesFor(parameters);old[20].clear();old[21].clear();
        const auto oldScalar=render(module,parameters,old,1);
        for(int block:{7,64,256,1024,4096}) {
            auto actual=render(module,parameters,old,block);
            check(difference(oldScalar,actual)<=limits["audio_max_abs_error"],"preserved20-lane supported trajectory contract");
            capture(out/("old20-"+std::to_string(block)+".pcm"),actual);
        }
        std::cout<<scenarios<<" native SUB scenarios, "<<failures<<" failures\n";
        return failures?1:0;
    } catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 2;}
}

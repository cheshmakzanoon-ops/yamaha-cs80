// Bounded diagnostic: reuse the repository's actual VST3 protocol host.
#define main historical_automation_main
#include "../tests/Vst3AutomationTests.cpp"
#undef main
#include "core/FactoryPresets.h"
#include <pluginterfaces/vst/ivstunits.h>
#include <fstream>
#include <iomanip>
#include <set>

namespace {
int countChecks = 0;
void verify(bool condition, const std::string& label) {
    ++countChecks;
    if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
std::string ascii(const char16* text) {
    std::string result;
    for (; *text; ++text) { require(*text < 128, "ASCII fixture factory name"); result += static_cast<char>(*text); }
    return result;
}
struct Reader {
    Instance& instance;
    IUnitInfo* units = nullptr;
    explicit Reader(Instance& value): instance(value) {
        TUID id{};
        require(instance.component->getControllerClassId(id)==kResultOk,"controller class");
        require(instance.factory->createInstance(id,IEditController::iid,reinterpret_cast<void**>(&instance.controller))==kResultOk,"controller create");
        require(instance.controller->initialize(nullptr)==kResultOk,"controller initialise");
        require(instance.component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&instance.componentConnection))==kResultOk,"component connection");
        require(instance.controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&instance.controllerConnection))==kResultOk,"controller connection");
        require(instance.componentConnection->connect(instance.controllerConnection)==kResultOk && instance.controllerConnection->connect(instance.componentConnection)==kResultOk,"native connection");
        require(instance.controller->queryInterface(IUnitInfo::iid,reinterpret_cast<void**>(&units))==kResultOk,"unit info");
        refresh();
    }
    ~Reader(){ if(units)units->release(); instance.releaseReadbackController(); }
    StateCapture snapshot() {
        StateCapture state;
        require(instance.component->getState(&state)==kResultOk,"processor getState");
        require(state.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"state rewind");
        return state;
    }
    void refresh(){ auto state=snapshot(); require(instance.controller->setComponentState(&state)==kResultOk,"controller state transfer"); }
};
struct Program {
    std::string name; int hostIndex; int parameterCount; size_t stateBytes;
    double loadError=0, restoreError=0, roundtripError=0;
};
}
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"usage: probe MODULE_SO OUTPUT_DIR\n";return 64;}
    try {
        const std::filesystem::path out(argv[2]);std::filesystem::create_directories(out);
        std::ofstream csv(out/"parameters.csv");
        csv<<"program,host_index,phase,id,expected_plain,actual_plain,expected_normalized,actual_normalized,normalized_error\n"<<std::setprecision(17);
        std::vector<Program> results;
        const std::vector<std::string> names={"Flute","Breathy Flute","Harp","Kalimba","Sine Bass","Upright Pluck","Ether Pad","Noir Swell Brass","Brass Ensemble 16","Neo-Noir Pad","Arctic Pad","Synthwave Chords"};
        const auto& bank=cs80::programLibrary();
        Module module(argv[1]);
        for(const auto& name:names) {
            auto found=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==name && p.library==cs80::ProgramLibrary::curated;});
            require(found!=bank.end(),"expected factory exists");
            const int index=static_cast<int>(found-bank.begin());
            Instance instance(module,cs80::defaultValues()); Reader reader(instance);
            verify(reader.units->getProgramListCount()==1,name+" one factory list");
            ProgramListInfo list{};require(reader.units->getProgramListInfo(0,list)==kResultOk,"program list");
            verify(list.programCount==523,name+" native program count523");
            String128 programName{};require(reader.units->getProgramName(list.id,index,programName)==kResultOk,"program name");
            verify(ascii(programName)==name,name+" native name at stable host index");
            ParamID programId=0;int programs=0;std::set<ParamID> ids;
            const auto parameterCount=instance.controller->getParameterCount();
            for(int i=0;i<parameterCount;++i){ParameterInfo info{};require(instance.controller->getParameterInfo(i,info)==kResultOk,"parameter metadata");
                verify(ids.insert(info.id).second,name+" unique native parameter ID");
                if(info.flags&ParameterInfo::kIsProgramChange){programId=info.id;++programs;verify(info.stepCount==522,name+" native program stepCount");}}
            verify(programs==1,name+" unique program-change parameter");
            Changes select;select.add(programId,{{0,double(index)/522.0}});instance.flush(select);reader.refresh();
            verify(std::abs(instance.controller->getParamNormalized(programId)-double(index)/522.0)<1e-12,name+" selected native program index");
            auto state=reader.snapshot();
            Program record{name,index,parameterCount,state.bytes.size()};
            std::array<double,cs80::P_numParams> loaded{};
            auto compare=[&](const char* phase,bool restored){
                for(int key=0;key<cs80::P_numParams;++key){
                    const auto id=parameterId(cs80::paramId(key));verify(ids.count(id)==1,name+" core key exposed");
                    const double actual=instance.controller->getParamNormalized(id), expected=normalized(key,found->values[key]);
                    const double plain=instance.controller->normalizedParamToPlain(id,actual), error=std::abs(actual-expected);
                    verify(std::isfinite(actual)&&actual>=0&&actual<=1,name+" normalized finite range");
                    verify(error<=1e-6,name+" factory value "+cs80::paramId(key)+" "+phase);
                    if(restored){record.restoreError=std::max(record.restoreError,error);record.roundtripError=std::max(record.roundtripError,std::abs(actual-loaded[key]));verify(std::abs(actual-loaded[key])<=1e-6,name+" roundtrip value");}
                    else {loaded[key]=actual;record.loadError=std::max(record.loadError,error);}
                    csv<<name<<','<<index<<','<<phase<<','<<cs80::paramId(key)<<','<<found->values[key]<<','<<plain<<','<<expected<<','<<actual<<','<<error<<'\n';
                }
            };
            compare("load",false);
            // Move to a different real factory program, then restore native state.
            Changes other;other.add(programId,{{0,1.0}});instance.flush(other);reader.refresh();
            verify(std::abs(instance.controller->getParamNormalized(programId)-1.0)<1e-12,name+" changed to Init before restore");
            require(state.seek(0,IBStream::kIBSeekSet,nullptr)==kResultOk,"restore rewind");
            require(instance.component->setState(&state)==kResultOk,"native processor setState");reader.refresh();
            verify(std::abs(instance.controller->getParamNormalized(programId)-double(index)/522.0)<1e-12,name+" native state restores program index");
            compare("restore",true);
            std::ofstream saved(out/(std::to_string(index)+"-state.bin"),std::ios::binary);saved.write(state.bytes.data(),state.bytes.size());
            results.push_back(record);
            std::cout<<name<<" index="<<index<<" native_parameters="<<parameterCount<<" state_bytes="<<state.bytes.size()<<" max_load_error="<<record.loadError<<" max_restore_error="<<record.restoreError<<" roundtrip_error="<<record.roundtripError<<'\n';
        }
        std::ofstream records(out/"programs.csv");records<<"name,host_index,native_parameter_count,state_bytes,max_load_error,max_restore_error,max_roundtrip_error\n"<<std::setprecision(17);
        for(const auto& p:results)records<<p.name<<','<<p.hostIndex<<','<<p.parameterCount<<','<<p.stateBytes<<','<<p.loadError<<','<<p.restoreError<<','<<p.roundtripError<<'\n';
        std::cout<<"Native factory boundary: "<<countChecks<<" checks, "<<failures<<" failures\n";
        return failures==0?0:1;
    }catch(const std::exception& e){std::cerr<<"ERROR "<<e.what()<<'\n';return 2;}
}

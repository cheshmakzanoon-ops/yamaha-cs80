// Bounded diagnostic only; production code and assignment44 evidence are unchanged.
#define main historical_automation_main
#include "../tests/Vst3AutomationTests.cpp"
#undef main
#include "core/FactoryPresets.h"
#include <pluginterfaces/vst/ivstunits.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <thread>
namespace {
struct Handler final : IComponentHandler {
    std::atomic<int> edits{0}, restarts{0}, flags{0};
    tresult PLUGIN_API queryInterface(const TUID t, void** o) override {
        if (std::memcmp(t,IComponentHandler::iid,sizeof(TUID))==0 || std::memcmp(t,FUnknown::iid,sizeof(TUID))==0) {*o=this; return kResultOk;}
        *o=nullptr; return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID,ParamValue) override { ++edits; return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32 f) override { ++restarts; flags.fetch_or(f); return kResultOk; }
    void clear(){edits=0;restarts=0;flags=0;}
};
struct Reader {
    Instance& instance; ParamID programId=0; int32 flags=0, steps=0;
    Reader(Instance& i, Handler* h):instance(i) {
        TUID id{}; require(i.component->getControllerClassId(id)==kResultOk,"controller id");
        require(i.factory->createInstance(id,IEditController::iid,reinterpret_cast<void**>(&i.controller))==kResultOk,"controller create");
        require(i.controller->initialize(nullptr)==kResultOk,"controller initialise");
        if(h)require(i.controller->setComponentHandler(h)==kResultOk,"component handler");
        require(i.component->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&i.componentConnection))==kResultOk,"component connection");
        require(i.controller->queryInterface(IConnectionPoint::iid,reinterpret_cast<void**>(&i.controllerConnection))==kResultOk,"controller connection");
        require(i.componentConnection->connect(i.controllerConnection)==kResultOk && i.controllerConnection->connect(i.componentConnection)==kResultOk,"connect both");
        refresh();
        for(int j=0;j<i.controller->getParameterCount();++j){ParameterInfo p{};require(i.controller->getParameterInfo(j,p)==kResultOk,"parameter info");if(p.flags&ParameterInfo::kIsProgramChange){programId=p.id;flags=p.flags;steps=p.stepCount;}}
        require(programId!=0 && steps==522,"program metadata");
    }
    ~Reader(){instance.controller->setComponentHandler(nullptr);instance.releaseReadbackController();}
    StateCapture snapshot(){StateCapture s; require(instance.component->getState(&s)==kResultOk,"snapshot");s.seek(0,IBStream::kIBSeekSet,nullptr);return s;}
    void refresh(){auto s=snapshot();require(instance.controller->setComponentState(&s)==kResultOk,"setComponentState");}
};
std::string attribute(const std::string& text, const std::string& key, size_t start=0){auto p=text.find(key+"=\"",start);if(p==std::string::npos)return "MISSING";p+=key.size()+2;auto e=text.find('"',p);return text.substr(p,e-p);}
}
int main(int argc,char** argv){
 if(argc!=3)return 64;
 try{
  std::filesystem::path out(argv[2]);std::filesystem::create_directories(out);
  std::ofstream rows(out/"observations.csv");rows<<"mode,stage,program_id,flags,step_count,controller_program_normalized,factory_index,preset_name,volume,handler_edits,handler_restarts,handler_flags\n"<<std::setprecision(17);
  auto& bank=cs80::programLibrary(); auto f=std::find_if(bank.begin(),bank.end(),[](const auto& p){return p.name=="Flute"&&p.library==cs80::ProgramLibrary::curated;}); require(f!=bank.end(),"Flute");int index=int(f-bank.begin());double target=double(index)/522.0;
  Module module(argv[1]);
  const std::vector<std::string> modes{"flush_no_handler","flush_handler","audio_handler","audio_thread_handler","controller_then_queue"};
  for(auto mode:modes){Handler handler;Instance instance(module,cs80::defaultValues());Reader reader(instance,mode=="flush_no_handler"?nullptr:&handler);handler.clear();
   auto record=[&](const char* stage){auto s=reader.snapshot();std::string xml(s.bytes.begin(),s.bytes.end());auto v=xml.find("id=\"volume\"");rows<<mode<<','<<stage<<','<<reader.programId<<','<<reader.flags<<','<<reader.steps<<','<<instance.controller->getParamNormalized(reader.programId)<<','<<attribute(xml,"factoryIndex")<<','<<attribute(xml,"presetName")<<','<<attribute(xml,"value",v)<<','<<handler.edits.load()<<','<<handler.restarts.load()<<','<<handler.flags.load()<<'\n';std::ofstream file(out/(mode+"-"+stage+".bin"),std::ios::binary);file.write(s.bytes.data(),s.bytes.size());};
   record("before");
   if(mode=="controller_then_queue")require(instance.controller->setParamNormalized(reader.programId,target)==kResultOk,"controller selection");
   Changes q;q.add(reader.programId,{{0,target}});q.add(parameterId("volume"),{{0,normalized(cs80::P_volume,.31f)}});
   if(mode=="audio_handler" || mode=="audio_thread_handler") {std::vector<float> audio;if(mode=="audio_thread_handler"){std::thread t([&]{instance.process(0,128,&q,audio);});t.join();}else instance.process(0,128,&q,audio);}
   else instance.flush(q);
   record("after_queue");
   // JUCE's Linux wrapper runs an internal message thread until a host run loop is registered.
   std::this_thread::sleep_for(std::chrono::milliseconds(250));record("after_250ms");
   if(mode!="controller_then_queue"){require(instance.controller->setParamNormalized(reader.programId,target)==kResultOk,"delayed controller selection");record("after_controller_sync");}
   reader.refresh();record("after_state_transfer");
   std::cout<<mode<<" target="<<index<<" complete\n";
  }
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}

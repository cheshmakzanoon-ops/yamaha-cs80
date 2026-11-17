"""Materialize a separately hashed diagnostic host; never alter qualification sources."""
import hashlib,json,pathlib,shutil,sys
ROOT=pathlib.Path(__file__).resolve().parent

def changed(text,old,new,count=1):
 if text.count(old)!=count:raise ValueError('timing overlay source drift: '+old[:90])
 return text.replace(old,new)

def materialize(destination):
 dest=pathlib.Path(destination).resolve();dest.mkdir(mode=0o700,parents=True,exist_ok=False)
 hashes={}
 for source in sorted(ROOT.iterdir()):
  if source.suffix in ('.hpp','.cpp','.py'):
   shutil.copy2(source,dest/source.name);hashes[source.name]=hashlib.sha256(source.read_bytes()).hexdigest()
 def edit(name,changes):
  p=dest/name;s=p.read_text()
  for old,new,*n in changes:s=changed(s,old,new,*n)
  p.write_text(s)
 edit('native_observer_render.hpp',[
  ('#include <memory>','#include <memory>\n#include <time.h>'),
  ('struct RenderCallback {std::int64_t start;int32 count;tresult returned;uint64 silenceFlags;};','inline std::uint64_t studyCpuNs(){timespec t{};require(::clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0 && t.tv_sec>=0 && t.tv_nsec>=0 && t.tv_nsec<1000000000,"thread CPU clock");return std::uint64_t(t.tv_sec)*1000000000ull+std::uint64_t(t.tv_nsec);}\nstruct RenderCallback {std::int64_t start;int32 count;tresult returned;uint64 silenceFlags;std::uint64_t cpuNs=0;};'),
  ('std::vector<TimedParameter> outputParameters;','std::vector<TimedParameter> outputParameters;std::vector<std::uint64_t> clockBaseline;'),
  ('validateProtocol(p);RenderObservation out;','validateProtocol(p);RenderObservation out;out.callbacks.reserve(static_cast<std::size_t>((p.frames+p.block-1)/p.block));out.clockBaseline.reserve(128);for(int i=0;i<128;++i){const auto a=studyCpuNs(),b=studyCpuNs();require(b>a,"monotonic empty thread clock");out.clockBaseline.push_back(b-a);}'),
  ('const auto result=processor.process(data);out.callbacks.push_back({start,count,result,bus.silenceFlags});','const auto before=studyCpuNs();const auto result=processor.process(data);const auto after=studyCpuNs();require(after>before && after-before<=25000000000ull,"bounded monotonic callback CPU clock");out.callbacks.push_back({start,count,result,bus.silenceFlags,after-before});'),
 ])
 edit('native_observer_render_encoding.hpp',[
  ('b.raw("CS80ROBS",8);','b.raw("CS80TIME",8);'),
  ('b.u64(c.silenceFlags);}','b.u64(c.silenceFlags);b.u64(c.cpuNs);}'),
  ('    return b;\n}\n} // namespace cs80_observer','    require(o.clockBaseline.size()==128,"fixed clock baseline count");b.u64(o.clockBaseline.size());for(auto n:o.clockBaseline){require(n>0 && n<=25000000000ull,"bounded clock baseline");b.u64(n);}\n    return b;\n}\n} // namespace cs80_observer'),
 ])
 edit('native_observer_runloop.hpp',[('capacity=256,receiptCapacity=4096','capacity=256,receiptCapacity=32768')])
 edit('native_observer_module_history.hpp',[
  ('void pump(std::uint64_t deadline){require(valid_ && services_.healthy() && loop_->pump({deadline,64,8}).ok,"owned host pump failed");}',
   'void pump(std::uint64_t deadline){require(valid_ && services_.healthy(),"owned host validity/services failed");const auto r=loop_->pump({deadline,64,8});if(!r.ok)throw std::runtime_error("owned host pump failed failure="+std::to_string(static_cast<unsigned>(loop_->failure()))+" receipt_rows="+std::to_string(loop_->receiptSize())+" callbacks="+std::to_string(r.callbacks)+" rounds="+std::to_string(r.rounds));}'),
 ])
 edit('native_observer_processing.hpp',[('stage<4','stage<6')])
 edit('native_observer_parameter_policy.hpp', [('"cs80-instance-history-A-D-v2"','"cs80-instance-timing-A-D-steady-v1"')])
 edit('native_observer_history.hpp',[
  ('std::array<RenderObservation,4>','std::array<RenderObservation,6>'),('std::array<Bytes,4>','std::array<Bytes,6>',2),('std::array<bool,4>','std::array<bool,6>'),('std::array<std::optional<worker::TaskReceipt>,6>','std::array<std::optional<worker::TaskReceipt>,8>'),
  ('i<4;++i','i<6;++i'),
  ('            const auto render=[&]', '            auto warmup=plan.render;warmup.frames=65536;warmup.events.erase(std::remove_if(warmup.events.begin(),warmup.events.end(),[](const auto& e){return e.value.type!=Event::kNoteOnEvent;}),warmup.events.end());for(auto& e:warmup.events)e.value.noteOn.noteId+=48;auto measured=warmup;measured.frames=524288;measured.events.clear();\n            const auto render=[&]'),
  ('"render-A","render-B","render-C","render-D"','"render-A","render-B","render-C","render-D","steady-warmup","steady-measured"'),
  ('owner_.render(plan.render,budget,pump)','owner_.render(index<4?plan.render:index==4?warmup:measured,budget,pump)'),
  ('encodeRenderObservation(plan.render,evidence_.rendered[index])','encodeRenderObservation(index<4?plan.render:index==4?warmup:measured,evidence_.rendered[index])'),
  ('render(3,5,HistoryStage::D);','render(3,5,HistoryStage::D);render(4,6,HistoryStage::D);render(5,7,HistoryStage::D);'),
  ('submitted()==6','submitted()==8'),('attempted()==6','attempted()==8'),('completed()==6','completed()==8'),('consumed()==6','consumed()==8'),
 ])
 edit('native_observer_process_child.hpp',[('"cs80-process-lifetime-child-v4"','"cs80-process-lifetime-timing-v1"')])
 edit('native_observer_driver.cpp',[
  ('auto q=cs80_observer::request::read(argv[1],argv[2]);','auto q=cs80_observer::request::read(argv[1],argv[2]);\n  cs80_observer::require(q.binding==cs80_observer::ParameterBindingPolicy::SourceAwarePanel && q.topology==cs80_observer::AudioInputTopology::DisabledMonoExternalInput && q.workload.rate==48000 && q.workload.mode==packed_filter_workload::Mode::realtime && q.workload.voices==16 && q.workload.block==64,"fixed steady timing profile");'),
 ])
 edit('process_lifetime_transaction.py',[
  ("SCHEMA='cs80-process-lifetime-child-v4'","SCHEMA='cs80-process-lifetime-timing-v1'"),
  ("SOURCE_HISTORY='cs80-instance-history-A-D-v2'","SOURCE_HISTORY='cs80-instance-timing-A-D-steady-v1'"),
  ('renders=[r.blob(PAYLOAD_CAP) for _ in range(4)]','renders=[r.blob(PAYLOAD_CAP) for _ in range(6)]'),
  ('enumerate((1,2,1,2,2,2),1)','enumerate((1,2,1,2,2,2,2,2),1)'),
  ('==[6,6,6,6,0]','==[8,8,8,8,0]'),
  ('for stage in range(4)]','for stage in range(6)]'),
 ])
 result={'schema':'cs80-steady-timing-overlay-v2','qualification':False,'source_sha256':hashes,'materialized_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dest.iterdir()) if p.is_file()}}
 (dest/'overlay-manifest.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':materialize(sys.argv[1])

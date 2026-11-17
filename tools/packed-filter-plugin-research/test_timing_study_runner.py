"""Plan/overlay controls and one complete inert timing receipt, no real plugin."""
import hashlib,importlib.util,json,os,pathlib,subprocess,tempfile,unittest
import timing_study_materialize as material
import timing_study_run as runner
from timing_study_decode import decode_timing,source_protocols,digest
ROOT=pathlib.Path(__file__).parent
class PlanControls(unittest.TestCase):
 def test_schedule(self):
  rows=runner.schedule();self.assertEqual(len(rows),96);self.assertTrue(all(x['arm']=='stock' for x in rows[:24]));self.assertTrue(all(x['family']=='comparison' for x in rows[24:]));
  for start in range(0,96,4):
   block=rows[start:start+4];self.assertEqual([x['label'] for x in block],list('ABBA'));self.assertEqual([x['slot'] for x in block],list(range(4)));self.assertEqual(len({(x['policy'],x['block'],x['family']) for x in block}),1)
 def test_drift(self):
  with self.assertRaises(ValueError):material.changed('a','b','c')
 def test_existing_output_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   with self.assertRaises(FileExistsError):material.materialize(d)
class LedgerBounds(unittest.TestCase):
 def test_old_and_new_full_capacity_terminal(self):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:self.skipTest('Explicit SDK required')
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d);host=root/'host';material.materialize(host)
   for includes,cap in ((ROOT,4096),(host,32768)):
    source=root/f'ledger-{cap}.cpp';exe=root/f'ledger-{cap}'
    source.write_text('#define INIT_CLASS_IID\n#include "native_observer_runloop.hpp"\nint main(){using namespace cs80_observer;runloop::MonotonicClock c;auto* p=new runloop::Dispatcher;auto deadline=c.now()+10000000000ull;for(int i=0;i<'+str(cap)+';++i){auto r=p->pump({deadline,64,8});if(!r.ok||!r.sampledIdle)return 1;}auto r=p->pump({deadline,64,8});return !(!r.ok && p->failure()==runloop::Failure::Receipt && p->receiptSize()=='+str(cap)+');}\n')
    p=subprocess.run(['g++','-std=c++20','-O2','-pthread','-I',sdk,'-I',str(includes),str(source),'-o',str(exe)],capture_output=True,text=True,timeout=30);self.assertEqual(p.returncode,0,p.stderr)
    p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10);self.assertEqual(p.returncode,0,p.stderr)
class InertPipeline(unittest.TestCase):
 def test_complete_inert_timing_receipt(self):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:self.skipTest('Explicit pinned SDK required')
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d);host=root/'host';material.materialize(host);bundle=root/'bundle';(bundle/'Contents/x86_64-linux').mkdir(parents=True);lib=bundle/'Contents/x86_64-linux/CS-80 Reference.so'
   text=(ROOT/'native_observer_module_history_library.cpp').read_text().replace('static std::vector<ParamID> fullIds;','static std::vector<ParamID> fullIds; static std::vector<SourceParameterExpectation> sourceRows;')
   text=text.replace('for(const auto& q:packed_filter_workload::controls(c))fullIds.push_back(q.id);','sourceRows=sourceParameterExpectations();for(const auto& q:sourceRows)fullIds.push_back(q.info.id);(void)c;')
   text=text.replace("p={};p.id=fullIds.at(static_cast<std::size_t>(index));p.title[0]='P';p.defaultNormalizedValue=defaultNormalized;",'p=sourceRows.at(static_cast<std::size_t>(index)).info;')
   text=text.replace('ParamValue PLUGIN_API getParamNormalized(ParamID) override {return normalized;}','ParamValue PLUGIN_API getParamNormalized(ParamID id) override {for(const auto& q:sourceRows)if(q.info.id==id)return q.info.defaultNormalizedValue;throw std::runtime_error("unknown fake param");}')
   text=text.replace('return processHook?processHook(data):kResultOk;','for(int i=0;i<data.numSamples;++i){data.outputs[0].channelBuffers32[0][i]=.1f;data.outputs[0].channelBuffers32[1][i]=-.1f;}return processHook?processHook(data):kResultOk;')
   text=text.replace('return (media==kAudio && direction==kOutput) || (media==kEvent && direction==kInput)?1:0;','return media==kAudio?1:(direction==kInput?1:0);')
   text=text.replace("b.channelCount=2;b.name[0]='O';", "b.channelCount=2;b.name[0]='O';if(m==kAudio && d==kInput){b.channelCount=1;b.busType=kMain;b.flags=0;const char16_t* n=u\"External Modulation\";for(std::size_t i=0;n[i];++i)b.name[i]=n[i];}")
   text=text.replace('activateBus(MediaType media,BusDirection,int32,TBool active)', 'activateBus(MediaType media,BusDirection direction,int32,TBool active)').replace('const int i=media==kAudio?0:1;','if(media==kAudio && direction==kInput){require(!active,"inactive input");return kResultOk;}const int i=media==kAudio?0:1;')
   text=text.replace('setBusArrangements(SpeakerArrangement*,int32 inputs,','setBusArrangements(SpeakerArrangement* input,int32 inputs,').replace('require(inputs==0 && outputs==1 && output && *output==SpeakerArr::kStereo,"exact stereo arrangement");','require(inputs==1 && input && *input==SpeakerArr::kMono && outputs==1 && output && *output==SpeakerArr::kStereo,"exact mono stereo arrangement");')
   text=text.replace('getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;','getBusArrangement(BusDirection direction,int32,SpeakerArrangement& a) override {a=direction==kInput?SpeakerArr::kMono:SpeakerArr::kStereo;')
   source=root/'fake.cpp';source.write_text(text)
   flags=['g++','-std=c++20','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-pedantic','-pthread','-I',sdk,'-I',str(ROOT.parents[1]/'src')]
   for cmd in ([*flags,'-I',str(ROOT),'-shared','-fPIC',str(source),'-o',str(lib)],[*flags,str(host/'native_observer_driver.cpp'),'-ldl','-lcrypto','-o',str(host/'observer')],[*flags,'-I',str(ROOT),str(ROOT/'timing_study_expect.cpp'),'-ldl','-lcrypto','-o',str(root/'probe')]):
    p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);self.assertEqual(p.returncode,0,p.stderr)
   # Inert factory uses CID05 and controller07, supported state; only request representation differs.
   req=root/'request';data=runner.request_bytes(bundle,'stock',0).replace(runner.T('abcdef019182faeb5265664943733830'),runner.T('05'*16),1)
   # Replace fixed state/query policies in the bounded header, leaving topology zero for inert fixture.
   pos=0
   for _ in range(4):n=int.from_bytes(data[pos:pos+8],'little');pos+=8+n
   data=data[:pos+5*8]+runner.U(1)+runner.U(2)+runner.U(2)+data[pos+8*8:]
   req.write_bytes(data);req.chmod(0o600)
   lines=subprocess.run([str(root/'probe'),str(req),runner.H(req)],capture_output=True,text=True,check=True,timeout=10).stdout.splitlines()
   spec=importlib.util.spec_from_file_location('timing_test_helper',host/'process_lifetime_transaction.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
   protocols=source_protocols(bytes.fromhex(lines[2]));c={'schema':helper.SCHEMA,'history_schema':helper.SOURCE_HISTORY,'arm':'stock','expected_manifest_digest':lines[0],'expected_evidence_digest':lines[1],'module_sha256':runner.H(lib),'case_identity_hex':lines[2],'protocol_sha256':lines[3:],'warmup_protocol_sha256':digest(protocols[4]),'measured_protocol_sha256':digest(protocols[5]),'controller_query_policy':2,'controller_query_result':0,'controller_query_pointer_present':True,'audio_input_topology':2,'parameter_binding_policy':2,'controller_state_policy':1,'controller_state_result':0,'selected_cid_hex':'05'*16,'controller_cid_hex':None,'module_root':str(bundle),'module_relative':'Contents/x86_64-linux/CS-80 Reference.so'}
   output=root/'run';env={'PATH':'/usr/bin:/bin','HOME':str(root),'LANG':'C','LC_ALL':'C'};execution=runner.run_one(host/'observer',req,output,env)
   result=decode_timing((output/'receipt').read_bytes(),c,execution['nonce'],execution['pid'],lib.stat(),helper)
   self.assertEqual(len(result['timing_segments']),6);self.assertEqual(result['timing_segments'][5]['callback_count'],8192);self.assertGreater(result['timing_segments'][5]['callback_cpu_total_ns'],0);self.assertFalse(result['production_qualified'])
if __name__=='__main__':unittest.main()

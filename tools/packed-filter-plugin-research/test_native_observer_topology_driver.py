"""Real inert shared-library A-D history through public request-v3 supervisor.
The synth is never loaded. Fixture replacements are scoped and count-checked.
"""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,unittest
import test_native_observer_driver as driver_base
from test_process_lifetime_transaction import NATIVE_EXPECTED
ROOT=pathlib.Path(__file__).parent
class TopologyDriverTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  sdk=os.environ.get('CS80_OBSERVER_SDK')
  if not sdk:raise unittest.SkipTest('explicit pinned SDK required')
  for manifest in ('native_observer_sdk_provenance.json','native_observer_render_provenance.json'):
   for name,wanted in json.loads((ROOT/manifest).read_text())['headers'].items():
    p=pathlib.Path(sdk)/name
    if p.is_symlink() or hashlib.sha256(p.read_bytes()).hexdigest()!=wanted:raise RuntimeError('SDK drift '+name)
  cls.temp=tempfile.TemporaryDirectory();cls.root=pathlib.Path(cls.temp.name);cls.exe=cls.root/'driver';cls.lib=cls.root/'fixture.so';cls.expected=cls.root/'expected'
  text=(ROOT/'native_observer_module_history_library.cpp').read_text()
  def replace(old,new):
   nonlocal text
   if text.count(old)!=1:raise RuntimeError('fixture replacement drift: '+old)
   text=text.replace(old,new)
  replace('static std::vector<ParamID> fullIds;', 'static std::vector<ParamID> fullIds;static int disabledRequests=0;')
  replace('(media==kAudio && direction==kOutput) || (media==kEvent && direction==kInput)?1:0;', 'media==kAudio || (media==kEvent && direction==kInput)?1:0;')
  replace("b.channelCount=2;b.name[0]='O';return kResultOk;", "b.channelCount=2;b.name[0]='O';if(m==kAudio && d==kInput){b.channelCount=1;b.busType=kMain;b.flags=0;const char16_t name[]=u\"External Modulation\";for(std::size_t i=0;i<sizeof(name)/sizeof(name[0]);++i)b.name[i]=name[i];}return kResultOk;")
  replace('activateBus(MediaType media,BusDirection,int32,TBool active) override {', '''activateBus(MediaType media,BusDirection direction,int32,TBool active) override {
        if(media==kAudio && direction==kInput){require(!active && phase==ObserverPhase::Setup,"only inactive external input at Setup");if(std::string(mode())=="topology-disable-fail")return kResultFalse;++disabledRequests;return kResultOk;}''')
  replace('setBusArrangements(SpeakerArrangement*,int32 inputs,SpeakerArrangement* output,int32 outputs)', 'setBusArrangements(SpeakerArrangement* input,int32 inputs,SpeakerArrangement* output,int32 outputs)')
  replace('require(inputs==0 && outputs==1 && output && *output==SpeakerArr::kStereo,"exact stereo arrangement");', 'require(inputs==1 && input && *input==SpeakerArr::kMono && outputs==1 && output && *output==SpeakerArr::kStereo,"exact mono/stereo arrangement");')
  replace('getBusArrangement(BusDirection,int32,SpeakerArrangement& a) override {a=3;', 'getBusArrangement(BusDirection direction,int32,SpeakerArrangement& a) override {a=direction==kInput?SpeakerArr::kMono:SpeakerArr::kStereo;')
  replace('require(std::this_thread::get_id()!=uiThread,"process worker");if(data.numSamples)', '''require(std::this_thread::get_id()!=uiThread,"process worker");
    if(data.numSamples){require(disabledRequests>0 && data.numInputs==1 && data.inputs && data.inputs[0].numChannels==1 && data.inputs[0].channelBuffers32 && data.inputs[0].channelBuffers32[0]==nullptr && data.inputs[0].silenceFlags==0,"actual inactive mono process descriptor");if(std::string(mode())=="topology-mutate"){std::fputs("INERT_INPUT_MUTATION\\n",stderr);data.inputs[0].channelBuffers32[0]=data.outputs[0].channelBuffers32[0];}}
    else require(data.numInputs==0 && !data.inputs,"parameter-only flush has no buffers");
    if(data.numSamples)''')
  replace('if(data.numSamples)for(int i=0;i<data.numSamples;++i){data.outputs[0].channelBuffers32[0][i]=.25f;data.outputs[0].channelBuffers32[1][i]=-.25f;}return kResultOk;', 'if(data.numSamples){for(int i=0;i<data.numSamples;++i){data.outputs[0].channelBuffers32[0][i]=.25f;data.outputs[0].channelBuffers32[1][i]=-.25f;}}return kResultOk;')
  replace('extern "C" bool ModuleExit(){', 'extern "C" bool ModuleExit(){require(disabledRequests==3,"three explicit inactive cycles before exit");')
  (cls.root/'library.cpp').write_text(text)
  expected=NATIVE_EXPECTED.replace('const auto plan=workload_detail::planFromRoster(c,roster);','auto plan=workload_detail::planFromRoster(c,roster);plan.render.topology=AudioInputTopology::DisabledMonoExternalInput;')
  if expected==NATIVE_EXPECTED:raise RuntimeError('expected probe seam drift')
  (cls.root/'expected.cpp').write_text(expected)
  flags=[shutil.which('g++'),'-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-pthread','-ffp-contract=off','-I',sdk,'-I',str(ROOT),'-I',str(ROOT.parents[1]/'src')]
  for cmd in ([*flags,str(ROOT/'native_observer_driver.cpp'),'-ldl','-lcrypto','-o',str(cls.exe)],[*flags,'-fno-gnu-unique','-shared','-fPIC',str(cls.root/'library.cpp'),'-o',str(cls.lib)],[*flags,str(cls.root/'expected.cpp'),'-lcrypto','-o',str(cls.expected)]):
   p=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
   if p.returncode:raise RuntimeError(p.stderr)
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 setUp=driver_base.DriverTests.setUp
 def encoded(self,**kw):return driver_base.DriverTests.encoded(self,**{'schema':'cs80-process-lifetime-request-v3','topology':2,**kw})
 def contract(self):
  c,commands=driver_base.DriverTests.public_contract(self);c['audio_input_topology']=2;return c,commands
 def test_public_complete_A_D(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.contract();result=process_lifetime_arm(c,commands)
  self.assertTrue(result['process_completed']);self.assertEqual(result['audio_input_topology'],2);self.assertFalse(result['production_qualified']);self.assertFalse(result['E_transfer_authority']);self.assertEqual(result['actual_pid'],commands.ledger[0]['pid'])
  payload=pathlib.Path(result['full_payload_path']).read_bytes();self.assertEqual(hashlib.sha256(payload).hexdigest(),result['payload_sha256']);self.assertIn(b'CS80RPRT\x02\x00\x00\x00\x02\x00\x00\x00',payload);self.assertEqual(len(result['render_sha256']),4)
 def test_prelaunch_topology_mismatch(self):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.contract();c['audio_input_topology']=1
  with self.assertRaisesRegex(ValueError,'prelaunch audio input topology mismatch'):process_lifetime_arm(c,commands)
  self.assertFalse(any('argv' in row for row in commands.ledger))
 def failure(self,mode,expected):
  from process_lifetime_transaction import process_lifetime_arm
  c,commands=self.contract();c['environment']['CS80_JOIN_MODE']=mode
  with self.assertRaises(Exception):process_lifetime_arm(c,commands)
  self.assertTrue(commands.terminal_failure);self.assertFalse((pathlib.Path(c['output'])/'process-result.json').exists())
  logs=b''.join(p.read_bytes() for p in (self.work/'logs').rglob('*') if p.is_file());self.assertIn(expected.encode(),logs);return logs
 def test_native_input_mutation_rejected(self):
  logs=self.failure('topology-mutate','processing worker terminal failure');self.assertIn(b'INERT_INPUT_MUTATION',logs)
 def test_native_inactive_request_failure(self):self.failure('topology-disable-fail','external input inactive request failed')
if __name__=='__main__':unittest.main()

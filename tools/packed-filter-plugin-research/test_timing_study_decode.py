"""Isolated pure timing/PCM/protocol controls; no plugin runs."""
import copy,math,os,pathlib,shutil,struct,subprocess,tempfile,unittest
import timing_study_decode as d
from source_parameter_roles import GENERATED
from source_parameter_proof import PANEL
S=lambda s:d.V(len(s))+s.encode()
def case():
 b=S('packed-filter-workload-draft-v1')+S('eb5b73f144f83ff386865b0abb726f767eb8f4a0b651662442c4d03ccf8a2ac2')+S('dcc9bc824420ceb801d5047660ff4857a9ab4adda9b4762d0bd8df09aba08981')+b''.join(d.V(x) for x in (0,48000,0,16,64,4096))+d.V(192)
 for name,id in GENERATED:b+=S(name)+d.V(id)+struct.pack('<d',1. if id in PANEL else .25)
 b+=d.V(32)
 for on in (True,False):
  for n in range(16):b+=struct.pack('<IIIIII f',(17 if on else 3072)+n,int(on),0,0,48+n,48+n,.75 if on else 0.)
 return b
def render(protocol,frames,*,bad=None):
 callbacks=[(i*64,64,0,0,1000) for i in range(frames//64)]
 if bad in ('start','count','result','silence','cpu-zero','cpu-large'):
  q=list(callbacks[0]);idx,value={'start':(0,1),'count':(1,63),'result':(2,1),'silence':(3,4),'cpu-zero':(4,0),'cpu-large':(4,d.CPU_CAP+1)}[bad];q[idx]=value;callbacks[0]=tuple(q)
 b=b'CS80TIME'+d.V(1)+d.B(protocol)+d.U(len(callbacks))+b''.join(struct.pack('<QIIQQ',*c) for c in callbacks)
 b+=d.U(frames*2)+struct.pack('<f',float('nan') if bad=='nan' else 0. if bad=='zero' else .125)*(frames*2)
 b+=d.U(0)+d.U(128)+d.U(0 if bad=='baseline-zero' else 20)*128
 return b+(b'x' if bad=='trailing' else b'')
class TimingTests(unittest.TestCase):
 def setUp(self):self.case=case();self.p=d.source_protocols(self.case)
 def test_valid(self):
  r=d.decode_render(render(self.p[0],4096),self.p[0],4096);self.assertEqual(r['callback_cpu_total_ns'],64000);self.assertEqual(r['pcm_peak'],.125)
 def test_all_six(self):
  c={'protocol_sha256':['unused','unused',d.digest(self.p[0])],'warmup_protocol_sha256':d.digest(self.p[4]),'measured_protocol_sha256':d.digest(self.p[5])}
  result=d.decode_segments([render(p,f) for p,f in zip(self.p,[4096]*4+[65536,524288])],self.case,c)
  self.assertEqual(result['measured_cpu_ns_per_frame'],1000/64);self.assertFalse(result['clock_subtracted'])
 def test_full_cpu_budget(self):
  blob=render(self.p[0],4096);offset=12+8+len(self.p[0])+8+24
  blob=blob[:offset]+d.U(d.CPU_CAP)+blob[offset+8:]
  with self.assertRaises(ValueError):d.decode_render(blob,self.p[0],4096)
 def test_wrong_schema(self):
  with self.assertRaises(ValueError):d.decode_render(b'CS80ROBS'+render(self.p[0],4096)[8:],self.p[0],4096)
 def test_exact_pcm_pairing(self):
  x={'timing_segments':[{'frames':4096,'protocol_sha256':'a'*64,'pcm_sha256':'b'*64,'pcm_bytes':32768} for _ in range(6)]}
  y=copy.deepcopy(x);self.assertTrue(d.matched_pcm_histories(x,y)['exact_pcm_all_six']);y['timing_segments'][5]['pcm_sha256']='c'*64;self.assertFalse(d.matched_pcm_histories(x,y)['exact_pcm_all_six'])
 def test_measured_silent_callback(self):
  blob=render(self.p[5],524288);pcm_at=12+8+len(self.p[5])+8+(524288//64)*32+8
  blob=blob[:pcm_at]+bytes(128*4)+blob[pcm_at+128*4:]
  with self.assertRaisesRegex(ValueError,'silent measured callback'):d.decode_render(blob,self.p[5],524288)
 def test_protocol_mismatch(self):
  with self.assertRaises(ValueError):d.decode_render(render(self.p[0],4096),self.p[1]+b'x',4096)
 def test_case_events(self):
  with self.assertRaises(ValueError):d.source_protocols(self.case[:-1]+b'\x01')
 def test_case_trailing(self):
  with self.assertRaises(ValueError):d.source_protocols(self.case+b'x')
 def test_source_cpp_protocol_agreement(self):
  sdk=os.getenv('CS80_OBSERVER_SDK')
  if not sdk:self.skipTest('explicit SDK for pure C++ source protocol comparison')
  root=pathlib.Path(__file__).parent
  code=r'''#include "workload_bridge.hpp"
#include <iostream>
int main(){using namespace cs80_observer;packed_filter_workload::Case c{packed_filter_workload::Policy::legacy,48000,packed_filter_workload::Mode::realtime,16,64};ConnectedParameterRoster roster;for(auto q:packed_filter_workload::controls(c))roster.ids.push_back(q.id);roster.declaredCount=192;auto p=workload_detail::planFromRoster(c,roster).render;p.topology=AudioInputTopology::DisabledMonoExternalInput;for(int k=0;k<3;++k){if(k==1){p.frames=65536;p.events.resize(16);for(auto& q:p.events)q.value.noteOn.noteId+=48;}if(k==2){p.frames=524288;p.events.clear();}auto b=encodeRenderProtocol(p);std::cout.write((char*)b.data.data(),b.data.size());}}'''
  with tempfile.TemporaryDirectory() as tmp:
   src=pathlib.Path(tmp)/'p.cpp';exe=pathlib.Path(tmp)/'p';src.write_text(code)
   subprocess.run([shutil.which('g++'),'-std=c++20','-I',sdk,'-I',str(root),'-I',str(root.parents[1]/'src'),str(src),'-o',str(exe)],capture_output=True,check=True,timeout=40)
   self.assertEqual(subprocess.run([str(exe)],capture_output=True,check=True,timeout=5).stdout,self.p[0]+self.p[4]+self.p[5])
for bad in ('start','count','result','silence','cpu-zero','cpu-large','nan','zero','baseline-zero','trailing'):
 def test(self,bad=bad):
  with self.assertRaises(ValueError):d.decode_render(render(self.p[0],4096,bad=bad),self.p[0],4096)
 setattr(TimingTests,'test_reject_'+bad.replace('-','_'),test)
class StatisticsTests(unittest.TestCase):
 def block(self,candidate=99,overhead=.0001):return {'cpu_per_frame':[100,candidate,candidate,100],'overhead_fraction':[overhead]*4}
 def test_one_percent_gain_allowed(self):
  r=d.paired_summary([self.block()]*6,[self.block(100)]*2);self.assertTrue(r['gain_claim_supported']);self.assertAlmostEqual(r['gain_fraction'],.01)
 def test_deterministic(self):self.assertEqual(d.paired_summary([self.block()]*6,[self.block(100)]*2),d.paired_summary([self.block()]*6,[self.block(100)]*2))
 def test_ci_contains_one(self):
  r=d.paired_summary([self.block(x) for x in (98,102,98,102,98,102)],[self.block(100)]*2);self.assertFalse(r['gain_claim_supported'])
 def test_lower_ci_must_exceed_floor(self):
  r=d.paired_summary([self.block(x,.002) for x in (99.9,99.9,99.9,98,98,98)],[self.block(100)]*2)
  self.assertGreater(r['gain_fraction'],r['claim_floor_fraction']);self.assertFalse(r['gain_claim_supported'])
 def test_clock_floor(self):self.assertFalse(d.paired_summary([self.block(overhead=.01)]*6,[self.block(100)]*2)['gain_claim_supported'])
 def test_noise_floor(self):self.assertFalse(d.paired_summary([self.block()]*6,[self.block(95)]*2)['gain_claim_supported'])
 def test_missing_block(self):
  with self.assertRaises(ValueError):d.paired_summary([self.block()]*5,[self.block(100)]*2)
 def test_nonfinite(self):
  with self.assertRaises(ValueError):d.paired_summary([self.block(float('nan'))]*6,[self.block(100)]*2)
if __name__=='__main__':unittest.main()

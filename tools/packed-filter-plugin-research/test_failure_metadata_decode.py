import hashlib,struct,unittest
from failure_metadata_decode import connected_metadata,decode_failure,classify_roster,MAX_METADATA
U=lambda n:struct.pack('<Q',n)
V=lambda n:struct.pack('<I',n)
B=lambda b:U(len(b))+b
T=lambda s:B(s.encode())
W=lambda s:U(len(s.encode('utf-16-le'))//2)+s.encode('utf-16-le')
H=lambda b:hashlib.sha256(b).hexdigest()
def metadata(parameters=None):
 b=T('native-observer-connected-v1')+T('native-observer-created-identity-v1')+b'\x05'*16+V(1)+b'\x07'*16
 for media,direction in ((0,0),(0,1),(1,0),(1,1)):
  b+=V(media)+V(direction)+V(1 if (media,direction)==(0,1) else 0)
  if (media,direction)==(0,1):b+=V(media)+V(direction)+V(2)+W('Output')+V(0)+V(1)+U(3)
 ps=parameters or [(123,'Gain',1,0,.5,.25)];b+=V(len(ps))
 for id,title,flags,steps,default,current in ps:b+=V(id)+W(title)+W('' if title.startswith('MIDI CC ') else title)+W('')+V(steps)+struct.pack('<d',default)+V(0)+V(flags)+struct.pack('<d',current)
 return b
class DecodeTests(unittest.TestCase):
 def setUp(self):self.expected={'nonce':'a'*64,'arm':'stock','manifest_digest':'b'*64,'evidence_digest':'c'*64,'module_sha256':'d'*64}
 def frame(self,md=None,scope='preload-identities-not-postfailure-verification',pid=123):
  md=metadata() if md is None else md;e=self.expected
  b=T('cs80-process-lifetime-failure-metadata-v1')+T('rejected')+V(24)+T(e['nonce'])+U(pid)+T(e['arm'])+b''.join(T(e[k]) for k in ('manifest_digest','evidence_digest','module_sha256'))+T(scope)+T('run')+T('history')+T('initial-flush')+T('roster differs')+T(H(md))+B(md)
  return U(len(b))+b+T(H(b))
 def decode(self,data=None,exit=24,pid=123):return decode_failure(self.frame() if data is None else data,expected=self.expected,observed_exit=exit,observed_pid=pid)
 def test_complete(self):
  d=self.decode();self.assertFalse(d['admitted']);self.assertFalse(d['postfailure_identity_verified']);p=d['connected']['parameters'][0];self.assertEqual(p['title']['text'],'Gain');self.assertEqual(p['current_normalized'],.25);self.assertEqual(p['default_normalized'],.5);self.assertEqual(p['flags_raw'],1)
 def test_wrong_exit(self):
  with self.assertRaises(ValueError):self.decode(exit=0)
 def test_wrong_pid(self):
  with self.assertRaises(ValueError):self.decode(pid=124)
 def test_wrong_nonce(self):
  data=self.frame();self.expected['nonce']='e'*64
  with self.assertRaises(ValueError):self.decode(data)
 def test_wrong_preload_identity(self):
  data=self.frame();self.expected['module_sha256']='e'*64
  with self.assertRaises(ValueError):self.decode(data)
 def test_identity_scope(self):
  with self.assertRaises(ValueError):self.decode(self.frame(scope='verified-after-failure'))
 def test_hash(self):
  data=bytearray(self.frame());data[-1]^=1
  with self.assertRaises(ValueError):self.decode(bytes(data))
 def test_trailing(self):
  with self.assertRaises(ValueError):self.decode(self.frame()+b'x')
 def test_truncated(self):
  with self.assertRaises(ValueError):self.decode(self.frame()[:-1])
 def test_oversize(self):
  with self.assertRaises(ValueError):self.decode(U(2**64-1))
 def test_metadata_tail(self):
  with self.assertRaises(ValueError):self.decode(self.frame(metadata()+b'x'))
 def test_duplicate_id(self):
  p=(123,'Gain',1,0,.5,.25)
  with self.assertRaises(ValueError):connected_metadata(metadata([p,p]))
 def test_sentinel(self):
  with self.assertRaises(ValueError):connected_metadata(metadata([(0xffffffff,'Bad',0,0,0,0)]))
 def test_nonfinite(self):
  with self.assertRaises(ValueError):connected_metadata(metadata([(1,'Bad',0,0,float('nan'),0)]))
 def test_range(self):
  with self.assertRaises(ValueError):connected_metadata(metadata([(1,'Bad',0,0,0,1.1)]))
 def test_utf16(self):
  p=connected_metadata(metadata([(1,'🎹',0,0,0,0)]))['parameters'][0];self.assertEqual(len(p['title']['code_units']),2);self.assertEqual(p['title']['text'],'🎹')
 def test_signed_preserved(self):
  p=connected_metadata(metadata([(1,'A',0,0xffffffff,0,0)]))['parameters'][0];self.assertEqual(p['step_count_raw'],0xffffffff);self.assertEqual(p['step_count'],-1)
 def test_classification(self):
  ps=[(120,'x',1,0,0,0),(0x62797073,'Bypass',65537,1,0,0),(0x70727374,'Program',32769,522,0,0),(0x6d636d00+129,'MIDI CC 0|129',0,0,0,0),(999,'Unknown',0,0,0,0)]
  c=classify_roster(connected_metadata(metadata(ps)),[{'name':'x','id':120}]);self.assertEqual(c['role_counts']['generated_workload_control'],1);self.assertEqual(c['role_counts']['pinned_juce_midi_emulation_pattern'],1);self.assertEqual(c['role_counts']['unclassified'],1);self.assertFalse(c['write_permission']);self.assertEqual(c['missing_generated'],[])
 def test_midi_name_not_guessed(self):
  c=classify_roster(connected_metadata(metadata([(0x6d636d00,'Other',0,0,0,0)])),[{'name':'x','id':120}]);self.assertEqual(c['role_counts'],{'unclassified':1});self.assertEqual(len(c['missing_generated']),1)
 def test_memory_source_map(self):
  name='memory_1_hpf';id=0
  for char in name:id=(id*31+ord(char))&0xffffffff
  id&=0x7fffffff
  c=classify_roster(connected_metadata(metadata([(id,'Memory 1 HPF',1,0,.5,.2)])),[{'name':'x','id':120}],[{'name':name,'id':id}]);self.assertEqual(c['role_counts'],{'source_memory_control':1});self.assertEqual(c['missing_source_memory'],[])
 def test_wrapper_flags_not_guessed(self):
  c=classify_roster(connected_metadata(metadata([(0x62797073,'Bypass',65536,1,0,0)])),[{'name':'x','id':120}]);self.assertEqual(c['role_counts'],{'unclassified':1})
 def test_program_count_not_guessed(self):
  c=classify_roster(connected_metadata(metadata([(0x70727374,'Program',32769,10,0,0)])),[{'name':'x','id':120}]);self.assertEqual(c['role_counts'],{'unclassified':1})
 def test_current_value_preserved_not_assumed_default(self):
  m=connected_metadata(metadata([(0x6d636d00,'MIDI CC 0|0',0,0,0,.25)]));c=classify_roster(m,[{'name':'x','id':120}]);self.assertEqual(c['role_counts'],{'pinned_juce_midi_emulation_pattern':1});self.assertEqual(m['parameters'][0]['current_normalized'],.25)
 def test_generated_hash(self):
  with self.assertRaises(ValueError):classify_roster(connected_metadata(metadata()),[{'name':'x','id':999}])
if __name__=='__main__':unittest.main()

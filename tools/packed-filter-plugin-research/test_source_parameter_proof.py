import struct,unittest
from source_parameter_proof import prepare,readback,IDS,PANEL,ALTER
from source_parameter_roles import GENERATED,expected_metadata
U=lambda n:struct.pack('<Q',n)
V=lambda n:struct.pack('<I',n&0xffffffff)
B=lambda b:U(len(b))+b
T=lambda s:B(s.encode())
W=lambda s:U(len(s.encode('utf-16-le'))//2)+s.encode('utf-16-le')
S=lambda s:V(len(s))+s.encode()
ONE=struct.pack('<d',1.)
def make_metadata(mutate=None):
 rows=list(expected_metadata());b=T('native-observer-connected-v1')+T('native-observer-created-identity-v1')+b'\x05'*16+V(0)
 for media,direction in ((0,0),(0,1),(1,0),(1,1)):b+=V(media)+V(direction)+V(0)
 b+=V(len(rows))
 for index,row in enumerate(rows):
  id,role,title,short,units,step,default,unit,flags=row;current=bytes.fromhex(default)
  data={'id':id,'role':role,'title':title,'short':short,'units':units,'step':step,'default':bytes.fromhex(default),'unit':unit,'flags':flags,'current':current}
  if mutate:mutate(index,data)
  b+=V(data['id'])+W(data['title'])+W(data['short'])+W(data['units'])+V(data['step'])+data['default']+V(data['unit'])+V(data['flags'])+data['current']
 return b

def fixtures():
 controls=[(id,ONE if id in PANEL else struct.pack('<d',.25)) for _,id in GENERATED]
 case=S('packed-filter-workload-draft-v1')+S('eb5b73f144f83ff386865b0abb726f767eb8f4a0b651662442c4d03ccf8a2ac2')+S('dcc9bc824420ceb801d5047660ff4857a9ab4adda9b4762d0bd8df09aba08981')+b''.join(V(x) for x in (0,48000,0,8,64,4096))+V(192)
 for (name,id),(_,bits) in zip(GENERATED,controls):case+=S(name)+V(id)+bits
 initial=T('cs80-control-input-v1')+V(0)+U(192)+b''.join(V(id)+bits for id,bits in controls)
 alter=T('cs80-control-input-v1')+V(0)+U(2)+b''.join(V(id)+U(bits) for id,bits in ALTER)
 return case,initial,alter

def encode_readback(stage,residual):return T('cs80-observe-only-readback-v1')+V(stage)+U(2188)+b''.join(V(id)+bits for id,bits in residual+[(PANEL[0],ONE),(PANEL[1],ONE)])
class SourceProofTests(unittest.TestCase):
 def setUp(self):self.case,self.initial,self.alter=fixtures()
 def prepare(self,metadata=None,initial=None,alter=None,case=None):return prepare(make_metadata() if metadata is None else metadata,self.initial if initial is None else initial,self.alter if alter is None else alter,self.case if case is None else case)
 def test_full_source_metadata_partition(self):
  residual,proof=self.prepare();self.assertEqual(len(residual),2186);self.assertEqual(proof['initial_write_count'],192);self.assertEqual(proof['alter_write_count'],2)
  for stage in range(4):self.assertEqual(len(readback(encode_readback(stage,residual),stage,residual)),64)
 def bad_metadata(self,key,value,index=0):
  def mutate(i,row):
   if i==index:row[key]=value
  with self.assertRaises(ValueError):self.prepare(metadata=make_metadata(mutate))
 def test_title(self):self.bad_metadata('title','wrong')
 def test_short(self):self.bad_metadata('short','wrong')
 def test_units(self):self.bad_metadata('units','wrong')
 def test_step(self):self.bad_metadata('step',123)
 def test_default(self):self.bad_metadata('default',struct.pack('<d',.123))
 def test_unit(self):self.bad_metadata('unit',123)
 def test_flags(self):self.bad_metadata('flags',0)
 def test_unknown_id(self):self.bad_metadata('id',123)
 def test_residual_baseline(self):self.bad_metadata('current',struct.pack('<d',.123),index=192)
 def test_generated_current_may_differ(self):
  def mutate(i,row):
   if row['role']==0:row['current']=struct.pack('<d',.987)
  self.prepare(metadata=make_metadata(mutate))
 def test_initial_extra_write(self):
  with self.assertRaises(ValueError):self.prepare(initial=self.initial+V(123)+ONE)
 def test_initial_value_drift(self):
  b=bytearray(self.initial);b[-1]^=1
  with self.assertRaises(ValueError):self.prepare(initial=bytes(b))
 def test_alter_value_drift(self):
  b=bytearray(self.alter);b[-1]^=1
  with self.assertRaises(ValueError):self.prepare(alter=bytes(b))
 def test_readback_stage(self):
  r,_=self.prepare()
  with self.assertRaises(ValueError):readback(encode_readback(1,r),0,r)
 def test_readback_panel(self):
  r,_=self.prepare();b=bytearray(encode_readback(0,r));b[-1]^=1
  with self.assertRaises(ValueError):readback(bytes(b),0,r)
 def test_readback_residual(self):
  r,_=self.prepare();changed=list(r);changed[0]=(changed[0][0],struct.pack('<d',.123))
  with self.assertRaises(ValueError):readback(encode_readback(0,changed),0,r)
 def test_readback_order(self):
  r,_=self.prepare();changed=list(r);changed[0],changed[1]=changed[1],changed[0]
  with self.assertRaises(ValueError):readback(encode_readback(0,changed),0,r)
 def test_readback_tail(self):
  r,_=self.prepare()
  with self.assertRaises(ValueError):readback(encode_readback(0,r)+b'x',0,r)
if __name__=='__main__':unittest.main()

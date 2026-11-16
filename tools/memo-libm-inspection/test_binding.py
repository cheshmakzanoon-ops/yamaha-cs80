import importlib.util,pathlib,unittest,json,tempfile
from unittest.mock import patch
p=pathlib.Path(__file__).with_name('inspect.py');spec=importlib.util.spec_from_file_location('memo_inspection',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Binding(unittest.TestCase):
 text='BIND\t0x403fd0\t0x7f000100\t0x7f000000\t0x100\t/lib/libm.so.6\nRESULT\t2097168\t0\t1\n'
 rel='000000403fd0 0000 R_X86_64_GLOB_DAT 0000 powf@GLIBC_2.27 + 0\n'
 asm='000000401200 <raw_pow>:\n 401200: e9 aa jmp 401060 <powf@plt>\n\n000000401060 <powf@plt>:\n 401060: ff 25 jmp QWORD PTR [rip+0x2f6a] # 403fd0 <powf@GLIBC_2.27>\n\n'
 def test_valid(self):self.assertEqual(m.check_binding(self.text,self.rel,self.asm)['implementation_offset'],'0x100')
 def test_wrong_slot(self):
  with self.assertRaises(AssertionError):m.check_binding(self.text,self.rel,self.asm.replace('# 403fd0','# 403fd8'))
 def test_wrong_call(self):
  with self.assertRaises(AssertionError):m.check_binding(self.text,self.rel,self.asm.replace('<powf@plt>','<exp2f@plt>'))
 def test_unstable_or_failed(self):
  for x in ['RESULT\t2097168\t1\t1','RESULT\t2097168\t0\t0']:
   with self.assertRaises(AssertionError):m.check_binding(self.text.replace('RESULT\t2097168\t0\t1',x),self.rel,self.asm)
 def test_interposition_rejected(self):
  with tempfile.TemporaryDirectory() as t,patch.dict(m.os.environ,{'LD_PRELOAD':'unapproved.so'}):
   with self.assertRaises(AssertionError):m.inspect(pathlib.Path(t))
 def test_request(self):self.assertEqual(json.loads(p.with_name('request.json').read_text()),m.REQUEST)
if __name__=='__main__':unittest.main()

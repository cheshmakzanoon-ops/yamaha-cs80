import unittest
import binding
class Receipt(unittest.TestCase):
 def setUp(self):
  self.rel='0000000000003ff8 000 R_X86_64_GLOB_DAT 0 powf@GLIBC_2.27 + 0'
  self.approved={'implementation_offset':'0x200','symbol_version':'GLIBC_2.27'}
  self.log=''.join('MEMO_BIND\t%s\t0x103ff8\t0x200200\t0x200000\t0x200\t/lib/libm.so.6\t0x100000\n'%p for p in ['begin','end'])
  self.asm='00001000 <powf@plt>:\n 1000: ff 25 00 00 00 00 jmp QWORD PTR [rip+0x20] # 3ff8 <powf@GLIBC_2.27>\n00001100 <_Engine_processInternalSampleImpl>:\n 1100: call 1000 <powf@plt>\n 1105: call 1200 <PitchPowMemo2hz>\n00001200 <PitchPowMemo2hz>:\n 1200: jmp 1000 <powf@plt>\n'
 def test_runtime_receipt(self):self.assertEqual(binding.verify(self.log,self.rel,self.approved)['relative_got'],'0x3ff8')
 def test_runtime_failures(self):
  for log,rel in [(self.log.splitlines()[0],self.rel),(self.log.replace('0x103ff8','0x103ff0',1),self.rel),(self.log,self.rel.replace('3ff8','3ff0'))]:
   with self.assertRaises((AssertionError,ValueError)):binding.verify(log,rel,self.approved)
 def test_codegen_all_arms(self):
  for arm in 'ABC':self.assertEqual(binding.codegen(self.asm,self.rel,arm)['relative_got'],'0x3ff8')
 def test_codegen_not_driver_relocation_only(self):
  for asm,rel in [(self.asm.replace('_Engine_processInternalSampleImpl','driver'),self.rel),(self.asm.replace('call 1200 <PitchPowMemo2hz>','nop'),self.rel),(self.asm,self.rel.replace('GLOB_DAT','JUMP_SLOT')),(self.asm,self.rel.replace('3ff8','3ff0'))]:
   with self.assertRaises(AssertionError):binding.codegen(asm,rel,'C')
if __name__=='__main__':unittest.main()

"""Compile tiny inert C fixtures; never load or execute generated code."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from elf_proof import Elf, PROCESS, verify_process, constant_access_width

class ElfProofTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();p=Path(cls.tmp.name);cls.path=p
        compiler=shutil.which('gcc')
        if compiler is None:
            raise RuntimeError('GNU gcc required for actual-link proof tests; not skipped')
        source='''__attribute__((noinline,visibility("hidden"))) double helper(double x) { return x+x; }
extern double process(double) __asm__("SYMBOL");
__attribute__((visibility("hidden"),noinline)) double process(double x) { return helper(x)*1.25; }
'''.replace('SYMBOL',PROCESS)
        (p/'fixture.c').write_text(source)
        subprocess.run([compiler,'-O3','-fPIC','-c',str(p/'fixture.c'),'-o',str(p/'fixture.o')],check=True,capture_output=True)
        subprocess.run([compiler,'-shared',str(p/'fixture.o'),'-o',str(p/'fixture.so')],check=True,capture_output=True)
        cls.obj=(p/'fixture.o').read_bytes();cls.final=(p/'fixture.so').read_bytes()
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def offset(self,symbol):
        elf=Elf(self.final);s=elf.symbol(symbol);sec=elf.sections[s['section']]
        return sec['offset']+s['value']-sec['addr']
    def test_actual_link(self):self.assertFalse(verify_process(self.obj,self.final)['production_qualified'])
    def test_wrong_machine(self):
        b=bytearray(self.final);struct.pack_into('<H',b,18,183)
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_changed_instruction(self):
        b=bytearray(self.final);b[self.offset(PROCESS)]^=1
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_changed_helper(self):
        b=bytearray(self.final);b[self.offset('helper')]^=1
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_changed_constant(self):
        b=bytearray(self.final);needle=struct.pack('<d',1.25);i=b.find(needle)
        self.assertGreaterEqual(i,0);b[i]^=1
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_relocation_target_corrupt(self):
        elf=Elf(self.obj);sym=elf.symbol(PROCESS);sec=next(s for s in elf.sections if s['type']==4 and s['info']==sym['section'])
        relative=struct.unpack_from('<Q',self.obj,sec['offset'])[0]-sym['value']
        b=bytearray(self.final);b[self.offset(PROCESS)+relative]^=8
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_shifted_addend_and_displacement_rejected(self):
        elf=Elf(self.obj);symbol=elf.symbol(PROCESS)
        section=next(s for s in elf.sections if s['type']==4 and s['info']==symbol['section'])
        obj=bytearray(self.obj);final=bytearray(self.final)
        for pos in range(section['offset'],section['offset']+section['size'],24):
            roff,info,addend=struct.unpack_from('<QQq',obj,pos)
            target=elf.tables[section['link']][info>>32]
            if target['info']&15==2:continue
            struct.pack_into('<q',obj,pos+16,addend+8)
            loc=self.offset(PROCESS)+roff-symbol['value']
            struct.pack_into('<i',final,loc,struct.unpack_from('<i',final,loc)[0]+8)
            with self.assertRaisesRegex(ValueError,'addend'):verify_process(obj,final)
            return
        self.fail('constant relocation fixture missing')
    def test_helper_internal_relocation_rejected(self):
        elf=Elf(self.obj);symbol=elf.symbol(PROCESS);helper=elf.symbol('helper')
        section=next(s for s in elf.sections if s['type']==4 and s['info']==symbol['section'])
        obj=bytearray(self.obj)
        # Keep process helper call; relocate the constant's metadata into helper.
        for pos in range(section['offset'],section['offset']+section['size'],24):
            roff,info,addend=struct.unpack_from('<QQq',obj,pos)
            target=elf.tables[section['link']][info>>32]
            if target['info']&15==2:continue
            struct.pack_into('<Q',obj,pos,helper['value'])
            with self.assertRaisesRegex(ValueError,'helper relocation'):verify_process(obj,self.final)
            return
        self.fail('constant relocation fixture missing')
    def test_immediate_addend_asymmetry(self):
        for encoded,relative in [('c5b5c20d0000000011',4),('c4e37d4b050000000010',5)]:
            code=bytes.fromhex(encoded)
            self.assertEqual(constant_access_width(code,relative,-5),32)
            with self.assertRaises(ValueError):constant_access_width(code,relative,-4)
    def test_constant_entry_too_small(self):
        elf=Elf(self.obj);entry=next(i for i,s in enumerate(elf.sections) if s['name']=='.rodata.cst8')
        section_offset=struct.unpack_from('<Q',self.obj,40)[0]
        obj=bytearray(self.obj);struct.pack_into('<Q',obj,section_offset+entry*64+56,4)
        with self.assertRaisesRegex(ValueError,'fully covered'):verify_process(obj,self.final)
    def test_dynamic_rewrite_rejected(self):
        elf=Elf(self.final);section=next(s for s in elf.sections if s['type']==4 and s['size']>=24)
        b=bytearray(self.final);struct.pack_into('<Q',b,section['offset'],elf.symbol(PROCESS)['value'])
        with self.assertRaises(ValueError):verify_process(self.obj,b)
    def test_truncated(self):
        with self.assertRaises(ValueError):verify_process(self.obj,self.final[:63])
if __name__=='__main__':unittest.main()

import unittest
from link_binding import contributions,resolve,MEMBER
class MapTests(unittest.TestCase):
    def test_live_not_discarded(self):
        text=f'Discarded input sections\n .text 0x0 0x20 wrong.a({MEMBER})\nLinker script and memory map\n .text 0x100 0x20 core.a({MEMBER})\nCross Reference Table\n'
        self.assertEqual(contributions(text,'/build'),[{'section':'.text','address':256,'size':32,'archive':'/build/core.a','member':MEMBER}])
    def test_split(self):
        x=contributions(f'Linker script and memory map\n .text.long\n  0x100 0x20 core.a({MEMBER})','/build')
        self.assertEqual(x[0]['section'],'.text.long')
    def test_direct_owner_not_hidden(self):
        x=contributions(f'Linker script and memory map\n .text 0x100 0x20 {MEMBER}','/build')
        self.assertIsNone(x[0]['archive'])
    def test_marker_absent(self):
        with self.assertRaises(ValueError):contributions('','/build')
    def test_marker_duplicate(self):
        with self.assertRaises(ValueError):contributions('Linker script and memory map\nLinker script and memory map','/build')
    def test_owner_traversal(self):
        with self.assertRaises(ValueError):resolve('../core.a','/build')
if __name__=='__main__':unittest.main()

class ActualMapBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import test_elf_proof,subprocess,shutil
        from pathlib import Path
        cls.fixture=test_elf_proof.ElfProofTests;cls.fixture.setUpClass();cls.p=cls.fixture.path
        cls.o=cls.p/MEMBER;shutil.copy2(cls.p/'fixture.o',cls.o)
        cls.archive=cls.p/'core.a';cls.map=cls.p/'link.map';cls.final=cls.p/'linked.so'
        subprocess.run(['ar','rcs',str(cls.archive),str(cls.o)],check=True,capture_output=True)
        subprocess.run(['gcc','-shared','-Wl,--whole-archive',str(cls.archive),'-Wl,--no-whole-archive',
                        '-Wl,-Map,'+str(cls.map)+',--cref','-o',str(cls.final)],check=True,capture_output=True)
        cls.args=dict(object_bytes=cls.o.read_bytes(),member_bytes=subprocess.check_output(['ar','p',str(cls.archive),MEMBER]),
                      member_names=subprocess.check_output(['ar','t',str(cls.archive)],text=True).splitlines(),
                      archive_path=str(cls.archive),map_text=cls.map.read_text(),cwd=str(cls.p),final_bytes=cls.final.read_bytes())
    @classmethod
    def tearDownClass(cls):cls.fixture.tearDownClass()
    def test_actual_archive_link(self):
        from link_binding import verify_binding
        r=verify_binding(**self.args);self.assertTrue(r['archive_member_bound']);self.assertFalse(r['production_qualified'])
    def test_wrong_member_bytes(self):
        from link_binding import verify_binding
        args={**self.args,'member_bytes':b'wrong'}
        with self.assertRaises(ValueError):verify_binding(**args)
    def test_duplicate_member(self):
        from link_binding import verify_binding
        args={**self.args,'member_names':[MEMBER,MEMBER]}
        with self.assertRaises(ValueError):verify_binding(**args)
    def test_wrong_map_provider(self):
        from link_binding import verify_binding
        args={**self.args,'map_text':self.args['map_text'].replace(str(self.archive),str(self.p/'wrong.a'))}
        with self.assertRaises(ValueError):verify_binding(**args)

import copy,json,tempfile,unittest
from pathlib import Path
from capture_roster import frozen,bind_contract,capture_case,compare_case
class CaptureRosterTests(unittest.TestCase):
    def setUp(self):self.c=frozen();self.repo=Path(__file__).resolve().parents[2]
    def commands(self):return {n:[x.replace('${BUILD}','/build').replace('${SOURCE}',str(self.repo)) for x in c['argv']] for n,c in self.c['cases'].items()}
    def bind(self,commands=None):return bind_contract(repo=self.repo,raw_commands=commands or self.commands(),source_root=self.repo,build_root='/build')
    def test_full_source_bound_roster(self):
        r=self.bind();self.assertEqual((r['native_cases'],r['drivers'],r['exposed_capture_cases']),(294,49,276));self.assertFalse(r['production_qualified'])
    def test_missing_case(self):
        c=self.commands();del c[next(iter(c))]
        with self.assertRaises(ValueError):self.bind(c)
    def test_changed_argv(self):
        c=self.commands();c[next(iter(c))].append('--compat-only')
        with self.assertRaises(ValueError):self.bind(c)
    def test_source_binding(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(ValueError):bind_contract(repo=t,raw_commands=self.commands(),source_root=self.repo,build_root='/build')
    def test_hashed_source_and_command_root_must_match(self):
        with self.assertRaises(ValueError):bind_contract(repo=self.repo,raw_commands=self.commands(),source_root='/other/repo',build_root='/build')
    def test_noncanonical_build_root(self):
        with self.assertRaises(ValueError):bind_contract(repo=self.repo,raw_commands=self.commands(),source_root=self.repo,build_root='/build/../other')
    def test_symlink_source_root(self):
        with tempfile.TemporaryDirectory() as t:
            link=Path(t)/'source';link.symlink_to(self.repo,target_is_directory=True)
            with self.assertRaises(ValueError):bind_contract(repo=link,raw_commands=self.commands(),source_root=link,build_root='/build')
    def test_no_capture_not_exemption(self):
        r=capture_case('/absent','cs80_vst3_cc1_fixture_44100_realtime_tests')
        self.assertFalse(r['captures_exposed']);self.assertTrue(r['full_original_assertions_required'])
    def make_case(self,root):
        name='cs80_vst3_glissando_automation_44100_realtime_tests';case=self.c['cases'][name]
        directory=root/case['capture_directory'].replace('${BUILD}/','');directory.mkdir(parents=True)
        for n,s in self.c['drivers'][case['driver']]['captures'].items():(directory/n).write_bytes(bytes(s))
        return name,directory
    def test_capture_and_exact_compare(self):
        with tempfile.TemporaryDirectory() as a,tempfile.TemporaryDirectory() as b:
            n,d=self.make_case(Path(a));self.make_case(Path(b));r=compare_case(a,b,n);self.assertEqual(r['files'],18);self.assertTrue(r['independent_sealed_arm_identity_required'])
            (next((Path(b)/d.name).iterdir())).write_bytes(b'\0'*32764+b'\0\0\x80?')
            with self.assertRaises(ValueError):compare_case(a,b,n)
    def test_same_root_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            name,_=self.make_case(Path(t))
            with self.assertRaises(ValueError):compare_case(t,t,name)
    def test_lexical_alias_root_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);name,_=self.make_case(root);(root/'child').mkdir()
            with self.assertRaises(ValueError):compare_case(root,root/'child'/'..',name)
    def test_symlink_alias_root_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);stock=root/'stock';stock.mkdir();name,_=self.make_case(stock)
            alias=root/'alias';alias.symlink_to(stock,target_is_directory=True)
            with self.assertRaises(ValueError):compare_case(stock,alias,name)
    def test_nested_roots_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);name,_=self.make_case(root);candidate=root/'candidate';candidate.mkdir()
            self.make_case(candidate)
            with self.assertRaises(ValueError):compare_case(root,candidate,name)
    def test_no_capture_case_still_rejects_self_comparison(self):
        with tempfile.TemporaryDirectory() as t:
            with self.assertRaises(ValueError):compare_case(t,t,'cs80_vst3_cc1_fixture_44100_realtime_tests')
    def test_missing_extra_wrong_size_nonfinite_symlink(self):
        for kind in ('missing','extra','size','nan','symlink'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as t:
                name,d=self.make_case(Path(t));p=next(d.iterdir())
                if kind=='missing':p.unlink()
                elif kind=='extra':(d/'extra').write_bytes(b'')
                elif kind=='size':p.write_bytes(b'')
                elif kind=='nan':p.write_bytes(b'\0\0\xc0\x7f'+bytes(32764))
                else:p.unlink();p.symlink_to('/tmp/untrusted')
                with self.assertRaises(ValueError):capture_case(t,name)
    def test_random_lfo_extended_length(self):
        d=self.c['drivers']['cs80_vst3_lfo_native_random_tests'];self.assertEqual(len(d['captures']),32);self.assertEqual(set(d['captures'].values()),{131072})
    def test_sub_depth_distinct_roster(self):
        d=self.c['drivers']['cs80_vst3_sub_depth_automation_tests']['captures'];self.assertEqual(len(d),41);self.assertIn('old22-7.pcm',d);self.assertNotIn('old20-7.pcm',d)
if __name__=='__main__':unittest.main()

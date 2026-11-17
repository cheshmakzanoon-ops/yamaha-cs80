import pathlib,tempfile,unittest
from capture_roster import frozen,capture_case
from capture_seal import seal_case,clear_case,directory_for
class CaptureSealTests(unittest.TestCase):
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.t.name)
        self.build=self.root/'build';self.sealed=self.root/'sealed';self.build.mkdir();self.sealed.mkdir()
        self.case='cs80_vst3_glissando_automation_44100_realtime_tests'
        self.source=directory_for(self.build,self.case);self.source.mkdir()
        contract=frozen();driver=contract['drivers'][contract['cases'][self.case]['driver']]
        for n,size in driver['captures'].items():(self.source/n).write_bytes(bytes(size))
    def tearDown(self):self.t.cleanup()
    def seal(self):return seal_case(build=self.build,sealed=self.sealed,case=self.case)
    def test_seal_and_recoverable_clear(self):
        receipt=self.seal();result=clear_case(build=self.build,sealed=self.sealed,case=self.case,expected=receipt)
        self.assertFalse(self.source.exists());self.assertEqual(capture_case(self.sealed,self.case),receipt);self.assertFalse(result['production_qualified'])
    def test_missing_seal_keeps_original(self):
        receipt=capture_case(self.build,self.case)
        with self.assertRaises(ValueError):clear_case(build=self.build,sealed=self.sealed,case=self.case,expected=receipt)
        self.assertTrue(self.source.exists())
    def test_corrupt_seal_keeps_original(self):
        receipt=self.seal();next(directory_for(self.sealed,self.case).iterdir()).write_bytes(b'bad')
        with self.assertRaises(ValueError):clear_case(build=self.build,sealed=self.sealed,case=self.case,expected=receipt)
        self.assertEqual(len(list(self.source.iterdir())),18)
    def test_duplicate_seal(self):
        self.seal()
        with self.assertRaises(ValueError):self.seal()
    def test_self_seal(self):
        with self.assertRaises(ValueError):seal_case(build=self.build,sealed=self.build,case=self.case)
    def test_stale_extra_capture(self):
        (self.source/'extra').write_bytes(b'x')
        with self.assertRaises(ValueError):self.seal()
    def test_assertion_only_not_a_capture_seal(self):
        with self.assertRaises(ValueError):seal_case(build=self.build,sealed=self.sealed,case='cs80_vst3_cc1_fixture_44100_realtime_tests')
if __name__=='__main__':unittest.main()

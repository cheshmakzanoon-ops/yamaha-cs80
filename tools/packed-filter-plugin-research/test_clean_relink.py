from pathlib import Path
import tempfile
import unittest
from clean_relink import relink_once
from identity import capture_files

class ToyCommands:
    def __init__(self,build,fail=False):self.build=build;self.calls=[];self.fail=fail;self.ledger=[];self.terminal_failure=False
    def persist(self):self.persisted=True
    def remaining(self):return 60
    def run(self,label,argv,cwd):
        if self.calls:raise ValueError('single attempt')
        self.calls.append(argv);self.ledger.append({'label':label,'argv':argv,'cwd':str(cwd),'exit':0})
        assert not (self.build/'module').exists()
        assert cwd==self.build
        if self.fail:raise RuntimeError('link failed')
        (self.build/'module').write_bytes(b'candidate')
        (self.build/'stdout').write_text('linked');(self.build/'stderr').write_text('')
        return self.build/'stdout',self.build/'stderr'

class RelinkTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.build=self.root/'build';self.sealed=self.root/'sealed'
        self.build.mkdir();self.sealed.mkdir()
        for p in (self.build,self.sealed):(p/'module').write_bytes(b'stock')
        self.hashes=capture_files(self.build,['module']);self.commands=ToyCommands(self.build)
    def tearDown(self):self.temp.cleanup()
    def call(self,**changes):
        args=dict(build=self.build,sealed=self.sealed,expected_outputs=['module'],expected_stock_hashes=self.hashes,
                  targets=['CS80Reference_VST3'],commands=self.commands,label='candidate-link')
        args.update(changes);return relink_once(**args)
    def test_clean_link(self):
        result=self.call();self.assertNotEqual(result['before'],result['after']);self.assertFalse(result['production_qualified'])
        self.assertEqual((self.sealed/'module').read_bytes(),b'stock')
        self.assertIn('--verbose',self.commands.calls[0])
    def test_changed_seal(self):
        (self.sealed/'module').write_bytes(b'bad')
        with self.assertRaises(ValueError):self.call()
        self.assertTrue((self.build/'module').exists());self.assertFalse(self.commands.calls)
    def test_changed_canonical(self):
        (self.build/'module').write_bytes(b'bad')
        with self.assertRaises(ValueError):self.call()
        self.assertTrue((self.build/'module').exists())
    def test_failed_link_keeps_original(self):
        self.commands.fail=True
        with self.assertRaises(RuntimeError):self.call()
        self.assertEqual((self.sealed/'module').read_bytes(),b'stock')
    def test_missing_new_output_terminal(self):
        original=self.commands.run
        def run(*a,**kw):
            result=original(*a,**kw);(self.build/'module').unlink();return result
        self.commands.run=run
        with self.assertRaises(ValueError):self.call()
        self.assertTrue(self.commands.terminal_failure);self.assertTrue(self.commands.persisted)
        with self.assertRaises(ValueError):self.call(label='retry')
        self.assertEqual(len(self.commands.calls),1)
    def test_postrun_seal_drift_terminal(self):
        original=self.commands.run
        def run(*a,**kw):
            result=original(*a,**kw);(self.sealed/'module').write_bytes(b'bad');return result
        self.commands.run=run
        with self.assertRaises(ValueError):self.call()
        self.assertTrue(self.commands.terminal_failure);self.assertTrue(self.commands.persisted)
        with self.assertRaises(ValueError):self.call(label='retry')
        self.assertEqual(len(self.commands.calls),1)
    def test_postrun_bad_ledger_terminal(self):
        original=self.commands.run
        def run(*a,**kw):
            result=original(*a,**kw);self.commands.ledger[-1]['exit']=1;return result
        self.commands.run=run
        with self.assertRaises(ValueError):self.call()
        self.assertTrue(self.commands.terminal_failure);self.assertTrue(self.commands.persisted)
    def test_same_roots(self):
        with self.assertRaises(ValueError):self.call(sealed=self.build)
    def test_missing_output_roster(self):
        with self.assertRaises(ValueError):self.call(expected_outputs=[])
    def test_symlink(self):
        (self.build/'module').unlink();(self.build/'module').symlink_to(self.sealed/'module')
        with self.assertRaises(ValueError):self.call()
        self.assertEqual((self.sealed/'module').read_bytes(),b'stock')
    def test_terminal_no_delete(self):
        self.commands.terminal_failure=True
        with self.assertRaises(ValueError):self.call()
        self.assertTrue((self.build/'module').exists())
    def test_repeated_label_no_delete(self):
        self.commands.ledger=[{'label':'candidate-link'}]
        with self.assertRaises(ValueError):self.call()
        self.assertTrue((self.build/'module').exists())
    def test_bad_target(self):
        with self.assertRaises(ValueError):self.call(targets=['--clean-first'])
if __name__=='__main__':unittest.main()

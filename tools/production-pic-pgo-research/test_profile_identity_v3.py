import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('profilev2',Path(__file__).with_name('profile_identity_v3.py'))
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

def contract(root, absolute_output=False):
    cwd='/fixed/build'; obj='/fixed/build/CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o'
    output=obj if absolute_output else 'CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o'
    rel='fixed/build/CMakeFiles/cs80_core.dir/src/core/Engine.cpp.gcda' if absolute_output else '#fixed#build#CMakeFiles#cs80_core.dir#src#core#Engine.cpp.gcda'
    embedded=str(root)+'/'+('/' if absolute_output else '')+rel
    data=b'\x7fELF\x02\x01\0'+p.VERSION+b'\0'+embedded.encode()+b'\0'
    argv=['/usr/bin/c++','-fprofile-generate='+str(root),'-o',output,'-c','/fixed/source/src/core/Engine.cpp']
    return p.make_contract(argv,cwd,'/fixed/source/src/core/Engine.cpp',obj,str(root),data,p.sha(data))

def write(root,c, data=None):
    path=root/c['relative_gcda'];path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(data if data is not None else b'adcg'+p.VERSION+b'0'*24)
    return path

class Tests(unittest.TestCase):
    def test_relative_and_absolute_spelling_are_distinct_exact_paths(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles';root.mkdir()
            a,b=contract(root),contract(root,True)
            self.assertEqual(a['relative_gcda'],'#fixed#build#CMakeFiles#cs80_core.dir#src#core#Engine.cpp.gcda')
            self.assertEqual(b['relative_gcda'],'fixed/build/CMakeFiles/cs80_core.dir/src/core/Engine.cpp.gcda')
            self.assertNotEqual(a['expected_gcda'],b['expected_gcda'])

    def test_raw_absolute_double_slash_not_normalized_before_comparison(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles'
            c=contract(root,True)
            self.assertEqual(c['embedded_gcda'],str(root)+'//fixed/build/CMakeFiles/cs80_core.dir/src/core/Engine.cpp.gcda')
            self.assertEqual(c['expected_gcda'],str(root)+'/fixed/build/CMakeFiles/cs80_core.dir/src/core/Engine.cpp.gcda')
            self.assertEqual(str(Path(c['embedded_gcda'])),c['expected_gcda'])
            for raw in (c['expected_gcda'],c['embedded_gcda'].replace('//fixed','///fixed'),
                        c['embedded_gcda'].replace('/build/','/build/./'),
                        c['embedded_gcda'].replace('/build/','/build/other/../'),
                        c['embedded_gcda'].replace('/fixed/','/wrong/')):
                data=b'\x7fELF\x02\x01\0'+p.VERSION+b'\0'+raw.encode()+b'\0'
                with self.assertRaises(ValueError):
                    p.make_contract(c['argv'],c['cwd'],c['source'],c['engine_object'],str(root),data,p.sha(data))

    def test_relative_raw_string_remains_exact(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles';c=contract(root)
            self.assertEqual(c['embedded_gcda'],c['expected_gcda'])
            raw=c['embedded_gcda'].replace('/#fixed','//#fixed')
            data=b'\x7fELF\x02\x01\0'+p.VERSION+b'\0'+raw.encode()+b'\0'
            with self.assertRaises(ValueError):
                p.make_contract(c['argv'],c['cwd'],c['source'],c['engine_object'],str(root),data,p.sha(data))

    def test_both_exact_forms_validate_and_copy(self):
        for absolute in (False,True):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root,absolute);file=write(root,c)
                evidence=Path(t)/'evidence';manifest=p.capture_and_validate(root,evidence,c)
                self.assertEqual(manifest[c['relative_gcda']]['sha256'],p.sha(file.read_bytes()))
                self.assertEqual((evidence/'raw'/c['relative_gcda']).read_bytes(),file.read_bytes())
                self.assertTrue(json.loads((evidence/'inventory.json').read_text())['accepted'])

    def test_wrong_suffix_name_retained_not_accepted(self):
        for name in ('Engine.cpp.gcda','garbageEngine.cpp.gcda','#wrong#build#Engine.cpp.gcda'):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root)
                (root/name).write_bytes(b'adcg'+p.VERSION+b'x'*24)
                evidence=Path(t)/'evidence'
                with self.assertRaises(ValueError):p.capture_and_validate(root,evidence,c)
                self.assertTrue((evidence/'raw'/name).is_file())
                self.assertFalse(json.loads((evidence/'inventory.json').read_text())['accepted'])

    def test_extra_file_directory_and_missing_rejected(self):
        for kind in ('extra-file','extra-dir','missing'):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root)
                if kind!='missing':write(root,c)
                if kind=='extra-file':(root/'other').write_bytes(b'x')
                if kind=='extra-dir':(root/'unexpected').mkdir()
                evidence=Path(t)/'evidence'
                with self.assertRaises(ValueError):p.capture_and_validate(root,evidence,c)
                self.assertTrue((evidence/'inventory.json').is_file())

    def test_bad_header_version_size_retains_bytes(self):
        for data in (b'',b'adcg'+p.VERSION,b'bad!'+p.VERSION+b'x'*24,b'adcg*24B'+b'x'*24):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root);write(root,c,data)
                evidence=Path(t)/'evidence'
                with self.assertRaises(ValueError):p.capture_and_validate(root,evidence,c)
                self.assertEqual((evidence/'raw'/c['relative_gcda']).read_bytes(),data)

    def test_symlink_hardlink_and_root_alias_never_accepted(self):
        import os
        for kind in ('symlink','hardlink','root'):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root)
                external=Path(t)/'external';external.write_bytes(b'private sentinel')
                if kind=='symlink':(root/c['relative_gcda']).symlink_to(external)
                elif kind=='hardlink':os.link(external,root/c['relative_gcda'])
                else:
                    root.rmdir();root.symlink_to(Path(t),target_is_directory=True)
                evidence=Path(t)/'evidence'
                with self.assertRaises(ValueError):p.capture_and_validate(root,evidence,c)
                self.assertFalse(any(x.is_file() for x in (evidence/'raw').rglob('*')))
                self.assertEqual(external.read_bytes(),b'private sentinel')

    def test_contract_embedded_path_cwd_output_and_hash(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles';c=contract(root)
            raw=b'\x7fELF\x02\x01\0'+p.VERSION+b'\0'+c['expected_gcda'].encode()+b'\0'
            args=[c['argv'],c['cwd'],c['source'],c['engine_object'],str(root),raw,p.sha(raw)]
            for index,value in ((1,'/wrong/build'),(3,'/wrong/Engine.cpp.o'),(6,'0'*64)):
                altered=list(args);altered[index]=value
                with self.assertRaises(ValueError):p.make_contract(*altered)
            altered=list(args);altered[0]=list(c['argv']);altered[0][3]=c['engine_object']
            with self.assertRaises(ValueError):p.make_contract(*altered) # Same resolved output, different spelling semantics.
            altered=list(args);altered[5]=raw+raw;altered[6]=p.sha(altered[5])
            with self.assertRaises(ValueError):p.make_contract(*altered)

    def test_training_or_csv_failure_keeps_unvalidated_bytes_no_retry(self):
        for failure in ('training','rows'):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t)/'profiles';root.mkdir();c=contract(root);evidence=Path(t)/'evidence';calls=[]
                def train():
                    calls.append('train');write(root,c)
                    if failure=='training':raise RuntimeError('training failed')
                def rows():
                    calls.append('rows')
                    self.assertTrue((evidence/'raw'/c['relative_gcda']).is_file())
                    raise RuntimeError('CSV failed')
                with self.assertRaises(RuntimeError):p.complete_training_attempt(root,evidence,c,train,rows)
                report=json.loads((evidence/'inventory.json').read_text())
                self.assertEqual(report['validation'],'unvalidated');self.assertFalse(report['accepted'])
                self.assertEqual(calls,['train'] if failure=='training' else ['train','rows'])
                self.assertTrue((evidence/'raw'/c['relative_gcda']).is_file())

    def test_live_recheck_is_not_only_retained_copy(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles';root.mkdir();c=contract(root);file=write(root,c)
            evidence=Path(t)/'evidence';frozen=p.capture_and_validate(root,evidence,c)
            self.assertEqual(p.live_profile_manifest(root,c),frozen)
            file.write_bytes(b'adcg'+p.VERSION+b'changed'+b'x'*24)
            self.assertNotEqual(p.live_profile_manifest(root,c),frozen)
            self.assertEqual(p.validate_evidence(evidence,c),frozen)
            (evidence/'raw'/c['relative_gcda']).write_bytes(b'bad')
            with self.assertRaises(ValueError):p.validate_evidence(evidence,c)
            self.assertFalse(json.loads((evidence/'inventory.json').read_text())['accepted'])

    def test_direct_fifo_read_is_nonblocking_and_rejected(self):
        import os
        with tempfile.TemporaryDirectory() as t:
            path=Path(t)/'fifo';os.mkfifo(path)
            with self.assertRaises(ValueError):p._regular_bytes(path,1024)

    def test_bounded_oversize_records_failure_without_copy(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)/'profiles';root.mkdir();c=contract(root)
            file=root/c['relative_gcda']
            with file.open('wb') as stream:stream.truncate(p.MAX_FILE+1)
            evidence=Path(t)/'evidence'
            with self.assertRaises(ValueError):p.capture_and_validate(root,evidence,c)
            report=json.loads((evidence/'inventory.json').read_text())
            self.assertFalse(report['complete']);self.assertFalse(report['entries'][0]['copied'])
            self.assertFalse(any(x.is_file() for x in (evidence/'raw').rglob('*')))

if __name__=='__main__':unittest.main()

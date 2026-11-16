import hashlib, importlib.util, json, os, pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock
spec=importlib.util.spec_from_file_location('trial',pathlib.Path(__file__).with_name('run.py'))
r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
CTEST=os.environ.get('CTEST_FOR_TESTS') or shutil.which('ctest')
if not CTEST:raise RuntimeError('Real CTest is required for scheduler tests')

class Tests(unittest.TestCase):
    def fixture(self,root,fail=False):
        build=root/'build';build.mkdir();out=root/'out';out.mkdir()
        lines=[]
        for name in ('alpha','bravo','charlie','delta'):
            code=("import sys; print('selected failure'); sys.exit(7)" if fail and name=='alpha'
                  else "import pathlib; pathlib.Path("+repr(str(build/(name+'.ran')))+").write_text('ran'); print('ok')")
            args=' '.join(r.bracket(x) for x in (sys.executable,'-c',code))
            lines.append(f'add_test({r.bracket(name)} {args})\nset_tests_properties({r.bracket(name)} PROPERTIES TIMEOUT 5)\n')
        (build/'CTestTestfile.cmake').write_text(''.join(lines))
        return build,out,r.read_inventory(CTEST,build)
    def test_real_ctest_full_partition_and_restoration(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t));original=(b/'CTestTestfile.cmake').read_bytes()
            result=r.trial(CTEST,b,o,i,['alpha','bravo'],4)
            self.assertTrue(result['accepted']);self.assertTrue(result['generated_file_restored'])
            self.assertEqual((b/'CTestTestfile.cmake').read_bytes(),original)
            self.assertEqual([g['junit']['count'] for g in result['groups']],[2,2])
            for group in ('selected','remaining'):
                for f in ('LastTest.log','junit.xml','ctest.log','console.log'):self.assertTrue((o/group/f).stat().st_size)
            for name in ('alpha','bravo','charlie','delta'):self.assertTrue((b/(name+'.ran')).exists())
    def test_failure_still_runs_remainder(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t),True);result=r.trial(CTEST,b,o,i,['alpha','bravo'],4)
            self.assertFalse(result['accepted']);self.assertNotEqual(result['groups'][0]['exit'],0)
            self.assertTrue(result['groups'][1]['accepted']);self.assertTrue((b/'charlie.ran').exists());self.assertTrue(result['generated_file_restored'])
    def test_reporting_exception_still_runs_remainder(self):
        original=r.run_group
        def broken(ctest,b,o,name,*args):
            if name=='selected':raise OSError('report storage failure')
            return original(ctest,b,o,name,*args)
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t))
            with mock.patch.object(r,'run_group',side_effect=broken):result=r.trial(CTEST,b,o,i,['alpha','bravo'],4)
            self.assertFalse(result['accepted']);self.assertTrue(result['groups'][1]['accepted']);self.assertTrue((b/'delta.ran').exists())
    def test_missing_report_fails_but_continues(self):
        original=r.junit_report
        def missing(path,*args):
            if path.parent.name=='selected':raise FileNotFoundError('missing XML')
            return original(path,*args)
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t))
            with mock.patch.object(r,'junit_report',side_effect=missing):result=r.trial(CTEST,b,o,i,['alpha','bravo'],4)
            self.assertFalse(result['accepted']);self.assertTrue(result['groups'][1]['accepted'])
    def test_bad_partition(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t))
            for names,count in ((['missing'],4),(['alpha','alpha'],4),(['alpha'],5),([],4)):
                with self.assertRaises(ValueError):r.partition(i,names,count)
            duplicate=json.loads(json.dumps(i));duplicate['tests'].append(duplicate['tests'][0])
            with self.assertRaises(ValueError):r.partition(duplicate,['alpha'],5)
    def test_same_count_roster_substitution(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t))
            expected=hashlib.sha256(('\n'.join(sorted(r.roster(i)))+'\n').encode()).hexdigest()
            self.assertEqual(r.verify_roster(i,expected),expected)
            i['tests'][0]['name']='substituted'
            with self.assertRaises(ValueError):r.verify_roster(i,expected)
    def test_unhandled_dependencies(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t));i['tests'][0]['properties'].append({'name':'DEPENDS','value':['bravo']})
            with self.assertRaises(ValueError):r.partition(i,['alpha'],4)
    def test_property_and_command_drift(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t));other=json.loads(json.dumps(i));other['tests'][0]['command'].append('extra')
            with self.assertRaises(ValueError):r.verify_environment(i,other,{})
            other=json.loads(json.dumps(i));other['tests'][0]['properties'].append({'name':'ENVIRONMENT','value':['UNEXPECTED=1']})
            with self.assertRaises(ValueError):r.verify_environment(i,other,{})
    def test_junit_missing_duplicate_and_skip(self):
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t);p=root/'j.xml';test={'alpha':{'properties':[]}}
            for xml in ('<testsuite/>','<testsuite><testcase name="alpha"/><testcase name="alpha"/></testsuite>',
                        '<testsuite><testcase name="alpha"><skipped/></testcase></testsuite>'):
                p.write_text(xml)
                with self.assertRaises(ValueError):r.junit_report(p,{'alpha'},test)
            name='cs80_filter_batch_cap_tests';test={name:{'properties':[{'name':'SKIP_RETURN_CODE','value':77}]}}
            for body in ('<skipped/>','<skipped message="wrong reason"/>',''):
                p.write_text(f'<testsuite><testcase name="{name}" status="notrun">{body}</testcase></testsuite>')
                with self.assertRaises(ValueError):r.junit_report(p,{name},test)
            p.write_text(f'<testsuite><testcase name="{name}" status="notrun"><skipped message="SKIP_RETURN_CODE=77"/></testcase></testsuite>')
            self.assertEqual(r.junit_report(p,{name},test)['skipped_by_existing_policy'],[name])
    def test_failed_restoration_is_not_overwritten(self):
        original=r.run_group
        def changed(ctest,b,o,name,*args):
            value=original(ctest,b,o,name,*args)
            if name=='remaining':(b/'CTestTestfile.cmake').write_text('# unexpected change\n')
            return value
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t))
            with mock.patch.object(r,'run_group',side_effect=changed):result=r.trial(CTEST,b,o,i,['alpha','bravo'],4)
            self.assertFalse(result['accepted']);self.assertFalse(result['generated_file_restored']);self.assertEqual((b/'CTestTestfile.cmake').read_text(),'# unexpected change\n')
    def test_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            b,o,i=self.fixture(pathlib.Path(t));f=b/'CTestTestfile.cmake';f.rename(b/'original');f.symlink_to(b/'original')
            with self.assertRaises(ValueError):r.trial(CTEST,b,o,i,['alpha','bravo'],4)

if __name__=='__main__':unittest.main()

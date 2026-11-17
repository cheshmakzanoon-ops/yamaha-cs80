import copy,os,pathlib,subprocess,tempfile,unittest,json,xml.etree.ElementTree as ET,sys
from split_ctest import schedule
class ScheduleTests(unittest.TestCase):
    def setUp(self):self.d={'tests':[{'name':'a.+ [x]','command':['/build/test'],'properties':[{'name':'WORKING_DIRECTORY','value':'/build'}]}]}
    def call(self,**kw):
        args=dict(document=self.d,build='/build',output='/evidence',ctest='/tools/ctest',arm='stock',environment={},definition_texts={'CTestTestfile.cmake':'add_test([=[a.+ [x]]=] "/build/test")'})
        args.update(kw);return schedule(**args)
    def test_exact_full_schedule(self):
        r=self.call();self.assertEqual(r['full_test_count'],1);self.assertIn('--no-tests=error',r['cases'][0]['argv']);self.assertFalse(r['production_qualified'])
    def test_unsupported_properties(self):
        for name in ['DEPENDS','FIXTURES_REQUIRED','RESOURCE_LOCK','RUN_SERIAL','PROCESSORS','ENVIRONMENT','WILL_FAIL']:
            self.d['tests'][0]['properties'].append({'name':name,'value':'x'})
            with self.subTest(name=name),self.assertRaises(ValueError):self.call()
            self.d['tests'][0]['properties'].pop()
    def test_inherited_ctest(self):
        with self.assertRaises(ValueError):self.call(environment={'CTEST_PARALLEL_LEVEL':'2'})
    def test_include_hook(self):
        with self.assertRaises(ValueError):self.call(definition_texts={'a':'include("hooks.cmake")'})
    def test_changed_cwd(self):
        self.d['tests'][0]['properties'][0]['value']='/elsewhere'
        with self.assertRaises(ValueError):self.call()
    def test_regex_followup(self):
        self.d['tests'][0]['name']='cs80_teardown_probe';self.d['tests'][0]['properties'].append({'name':'PASS_REGULAR_EXPRESSION','value':['JUCE shut down',' static destructors run']})
        self.assertTrue(self.call()['cases'][0]['same_attempt_dashboard_exit_required'])

class RuntimeAuditTests(unittest.TestCase):
    def setUp(self):
        from identity import capture_files
        self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
        self.tool=self.root/'ctest';self.tool.write_bytes(b'tool');self.defs=self.root/'CTestTestfile.cmake';self.defs.write_text('# frozen')
        self.hashes=capture_files(self.root,['CTestTestfile.cmake']);self.toolhash=capture_files(self.root,['ctest'])['ctest']
    def tearDown(self):self.tmp.cleanup()
    def call(self,env=None):
        from split_ctest import audit_runtime
        return audit_runtime(build=self.root,ctest=self.tool,expected_ctest_sha256=self.toolhash,definition_hashes=self.hashes,environment=env or {})
    def test_exact_runtime(self):self.assertTrue(self.call()['custom_hooks_absent'])
    def test_custom_config(self):
        for name in ['CTestCustom.cmake','CTestCustom.ctest','CTestConfiguration.ini','DartConfiguration.tcl','CTestConfig.cmake']:
            p=self.root/name;p.write_text('# hook')
            with self.subTest(name=name),self.assertRaises(ValueError):self.call()
            p.unlink()
    def test_runtime_changed(self):
        self.tool.write_bytes(b'changed')
        with self.assertRaises(ValueError):self.call()
    def test_definitions_changed(self):
        self.defs.write_text('changed')
        with self.assertRaises(ValueError):self.call()
    def test_environment(self):
        with self.assertRaises(ValueError):self.call({'CTEST_CUSTOM_PRE_TEST':'bad'})

@unittest.skipUnless(os.environ.get('CS80_CTEST'),'explicit exact CTest tool required for inert equivalence fixture')
class CTestEquivalenceTests(unittest.TestCase):
    def test_monolithic_split_and_regex_exit(self):
        tool=os.environ['CS80_CTEST'];self.assertIn('3.31.6',subprocess.check_output([tool,'--version'],text=True))
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t);script=root/'case.py'
            script.write_text('import os,sys,time\nprint("cwd="+os.getcwd(),flush=True)\nprint("env="+os.environ["CS80_TOY_ENV"],flush=True)\nprint("JUCE shut down; static destructors run",flush=True)\nif sys.argv[1]=="timeout":time.sleep(1)\nsys.exit(int(sys.argv[1]) if sys.argv[1].isdigit() else 0)\n')
            names=['normal space.+','skip','regex-nonzero','timeout']
            lines=[]
            for name,arg in zip(names,['0','77','42','timeout']):
                lines.append(f'add_test([=[{name}]=] "{sys.executable}" "{script}" "{arg}")')
                lines.append(f'set_tests_properties([=[{name}]=] PROPERTIES WORKING_DIRECTORY "{root}")')
            lines+=['set_tests_properties(skip PROPERTIES SKIP_RETURN_CODE 77)','set_tests_properties(regex-nonzero PROPERTIES PASS_REGULAR_EXPRESSION "JUCE shut down")','set_tests_properties(timeout PROPERTIES TIMEOUT 0.05)']
            (root/'CTestTestfile.cmake').write_text('\n'.join(lines)+'\n')
            env={k:v for k,v in os.environ.items() if not k.startswith('CTEST_')};env['CS80_TOY_ENV']='frozen'
            def run(extra,path):
                r=subprocess.run([tool,'--test-dir',str(root),'--output-on-failure','--no-tests=error','-T','Test','--output-junit',str(path),*extra],env=env,capture_output=True,text=True)
                cases=list(ET.parse(path).getroot().iter('testcase'))
                return r,{c.get('name'):('skip' if c.find('skipped') is not None else 'fail' if c.find('failure') is not None else 'pass') for c in cases}
            mono,outcomes=run([],root/'mono.xml');self.assertNotEqual(mono.returncode,0);self.assertEqual(outcomes,dict(zip(names,['pass','skip','pass','fail'])))
            import re
            seen={}
            for i,name in enumerate(names):
                result,one=run(['-R','^'+re.escape(name)+'$'],root/f'split{i}.xml');self.assertEqual(set(one),{name});seen.update(one)
                tag=(root/'Testing/TAG').read_text().splitlines()[0]
                text=(root/'Testing/Temporary'/('LastTest_'+tag+'.log')).read_text();self.assertIn('cwd='+str(root),text);self.assertIn('env=frozen',text)
                (root/f'LastTest-{i}.log').write_text(text)
                tag=(root/'Testing/TAG').read_text().splitlines()[0]
                dashboard=(root/'Testing'/tag/'Test.xml').read_text()
                (root/f'Test-{i}.xml').write_text(dashboard)
                from dashboard_exit import require_exit_zero
                actual=[sys.executable,str(script),['0','77','42','timeout'][i]]
                if name==names[0]:self.assertEqual(require_exit_zero(dashboard,name=name,expected_argv=actual)['raw_exit'],0)
                else:
                    with self.assertRaises(ValueError):require_exit_zero(dashboard,name=name,expected_argv=actual)

            self.assertEqual(seen,outcomes)
            raw=subprocess.run([sys.executable,str(script),'42'],cwd=root,env=env,capture_output=True);self.assertEqual(raw.returncode,42)
            none=subprocess.run([tool,'--test-dir',str(root),'--no-tests=error','-R','^absent$'],capture_output=True);self.assertNotEqual(none.returncode,0)
if __name__=='__main__':unittest.main()

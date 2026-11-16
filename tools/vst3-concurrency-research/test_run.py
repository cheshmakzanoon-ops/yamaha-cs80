import argparse, importlib.util, json, pathlib, subprocess, tempfile, unittest
from unittest import mock
spec = importlib.util.spec_from_file_location('research', pathlib.Path(__file__).with_name('run.py'))
r = importlib.util.module_from_spec(spec); spec.loader.exec_module(r)

class Tests(unittest.TestCase):
    def inventory(self):
        repo, build = pathlib.Path('/repo'), pathlib.Path('/build')
        tests = []
        for c in r.cases():
            command = [str(build / ('cs80_vst3_automation_tests' if c['family']=='generic' else 'cs80_vst3_pwm_automation_tests')),
                       str(build / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'),
                       '/repo/tests/parity/tolerances-v2/host-automation.json']
            if c['family']=='pwm':
                command += ['/repo/tests/parity/tolerances-v2/host-automation-pwm-v1.json',f'/build/pwm-automation-capture-{c["rate"]}-{c["mode"]}',str(c['rate']),c['mode']]
            elif c['name']!='cs80_vst3_automation_tests': command += [str(c['rate']),c['mode']]
            tests.append(dict(name=c['name'],command=command,properties=[dict(name='TIMEOUT',value=180.0),dict(name='WORKING_DIRECTORY',value='/build')]))
        return dict(tests=tests)
    def validate(self, data): return r.validate_inventory(data,pathlib.Path('/repo'),pathlib.Path('/build'))
    def test_exact_roster(self):
        selected=self.validate(self.inventory());self.assertEqual(len(selected),12)
        self.assertEqual(len({c['name'] for c in selected}),12)
        self.assertEqual(len(next(c for c in selected if c['name']=='cs80_vst3_automation_tests')['command']),3)
    def test_missing_duplicate(self):
        for change in (lambda d:d['tests'].pop(),lambda d:d['tests'].append(d['tests'][0])):
            d=self.inventory();change(d)
            with self.assertRaises(ValueError):self.validate(d)
    def test_command_drift(self):
        for suffix in ('--compat-only','--skip'):
            d=self.inventory();d['tests'][-1]['command'].append(suffix)
            with self.assertRaises(ValueError):self.validate(d)
    def test_timeout_and_unhandled_properties(self):
        for prop in ({'name':'TIMEOUT','value':181},{'name':'ENVIRONMENT','value':['HOME=/other']},{'name':'WORKING_DIRECTORY','value':'/other'}):
            d=self.inventory();d['tests'][0]['properties'].append(prop)
            with self.assertRaises(ValueError):self.validate(d)
    def test_capture_collision(self):
        d=self.inventory();d['tests'][-1]['command'][4]=d['tests'][-2]['command'][4]
        with self.assertRaises(ValueError):self.validate(d)
    def test_valid_gate(self):
        record=dict(exit=0,home_entries=[],pwm_full_gate=True,family='pwm',captures={name:dict(size=32768,sha256='x') for name in r.expected_pcm_names()})
        self.assertTrue(r.valid(record))
        for key,value in [('exit',1),('home_entries',['state']),('pwm_full_gate',False),('captures',{})]:
            self.assertFalse(r.valid({**record,key:value}))
    def test_timeout_receipt_and_nonzero(self):
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t); case=dict(name='fixture',family='generic',command=['fake'],capture=None)
            with mock.patch.object(r.subprocess,'run',side_effect=subprocess.TimeoutExpired(['fake'],180)):
                result=r.execute(case,'serial',root,root)
            self.assertEqual(result['exit'],124);self.assertFalse(r.valid(result))
            self.assertTrue((root/'serial/fixture/result.json').exists())
    def test_refuse_existing_home(self):
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t);home=root/'homes/fixture';home.mkdir(parents=True);(home/'state').write_text('keep')
            with self.assertRaises(ValueError):r.execute(dict(name='fixture',family='generic',command=['fake'],capture=None),'serial',root,root)
            self.assertEqual((home/'state').read_text(),'keep')
    def test_refuse_existing_capture(self):
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t);capture=root/'keep';capture.mkdir()
            with self.assertRaises(ValueError):r.execute(dict(name='fixture',family='pwm',command=['fake'],capture=str(capture)),'serial',root,root)
            self.assertTrue(capture.exists())

    def main_flow(self, failure=None):
        with tempfile.TemporaryDirectory() as t:
            root=pathlib.Path(t); repo=root/'repo';build=root/'build';repo.mkdir();build.mkdir();out=root/'out'
            inventory=json.loads(json.dumps(self.inventory()).replace('/repo',str(repo)).replace('/build',str(build)))
            selected=r.validate_inventory(inventory,repo,build)
            for name in {c['command'][0] for c in selected}|{c['command'][1] for c in selected}:
                p=pathlib.Path(name);p.parent.mkdir(parents=True,exist_ok=True);p.write_text('binary')
            def output(command,**kwargs):
                if command[0]=='ctest':return json.dumps(inventory)
                return '' if 'status' in command else r.BASELINE
            def execute(case,phase,out,build):
                captures={name:dict(size=32768,sha256='same') for name in r.expected_pcm_names()} if case['family']=='pwm' else {}
                record={**case,'exit':0,'home_entries':[],'pwm_full_gate':True,'captures':captures,'log_sha256':'same'}
                if phase=='parallel' and failure=='mismatch':record['log_sha256']='different'
                if phase=='parallel' and failure=='exit':record['exit']=7
                if phase=='serial' and failure=='serial_exit':record['exit']=7
                if failure=='binary':pathlib.Path(case['command'][0]).write_text('changed')
                return record
            with mock.patch.object(r.argparse.ArgumentParser,'parse_args',return_value=argparse.Namespace(repo=repo,build=build,out=out)),mock.patch.object(r.subprocess,'run'),mock.patch.object(r.subprocess,'check_output',side_effect=output),mock.patch.object(r,'execute',side_effect=execute):
                code=r.main()
            result=json.loads((out/'results.json').read_text())
            return code,result
    def test_main_acceptance(self):
        code,result=self.main_flow();self.assertEqual(code,0);self.assertTrue(result['accepted']);self.assertEqual(len(result['parallel']),12)
    def test_main_rejects_mismatch_exit_and_binary_drift(self):
        for failure in ('mismatch','exit','serial_exit','binary'):
            with self.subTest(failure=failure):
                code,result=self.main_flow(failure);self.assertEqual(code,1);self.assertFalse(result['accepted'])
                if failure=='serial_exit':self.assertEqual(result['parallel'],[])
    def test_wrong_pcm_names(self):
        record=dict(exit=0,home_entries=[],pwm_full_gate=True,family='pwm',captures={str(i):dict(size=32768,sha256='x') for i in range(72)})
        self.assertFalse(r.valid(record))

if __name__=='__main__':unittest.main()

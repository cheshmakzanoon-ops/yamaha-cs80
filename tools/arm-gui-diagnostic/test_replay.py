import importlib.util, json, pathlib, tempfile, unittest, subprocess
from unittest.mock import patch
s = importlib.util.spec_from_file_location('replay', pathlib.Path(__file__).with_name('replay.py'))
r = importlib.util.module_from_spec(s); s.loader.exec_module(r)

def record(code=0, success=True, **kw):
    return dict(exit=code, success_marker=success, original_assertion=False, timed_out=False, **kw)

class Tests(unittest.TestCase):
    def test_clean(self):
        self.assertEqual(r.classify(record()), 'clean_nonreproduction')
        self.assertEqual(r.classify(record(), {'state':'exited','exit_status':0},True),'clean_nonreproduction')
    def test_stops(self):
        for state in ('stopped','crashed','suspended'):
            self.assertEqual(r.classify(record(),{'state':state},True),'captured_stop')
    def test_inferior_failed(self):
        self.assertEqual(r.classify(record(),{'state':'exited','exit_status':9},True),'inferior_failure')
    def test_debugger_failed(self):
        self.assertEqual(r.classify(record(1),{'state':'exited','exit_status':0},True),'debugger_failure')
    def test_missing_evidence(self):
        self.assertEqual(r.classify(record(),None,True),'missing_inferior_evidence')
        self.assertEqual(r.classify(record(),{'state':'launching'},True),'inferior_launch_or_state_failure')
    def test_fail_closed(self):
        self.assertEqual(r.classify(record(success=False)),'missing_success')
        x=record(); x['original_assertion']=True
        self.assertEqual(r.classify(x),'assertion_observed')
        x['timed_out']=True
        self.assertEqual(r.classify(x),'timeout')
        self.assertEqual(r.classify(record(launch_error='denied')),'launch_failure')
    def run_pair(self, native_code=0, mutate=False, debug_state='exited', debug_exit=0):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d); validator=root/'validator'; validator.write_bytes(b'v')
            component=root/'p.component'; binary=component/'Contents/MacOS/CS-80 Reference'
            binary.parent.mkdir(parents=True); binary.write_bytes(b'au')
            calls=[]
            def execute(command, folder, timeout):
                calls.append(command)
                self.assertEqual(timeout,300)
                self.assertIn('0x41dd1ce',command)
                self.assertIn('--strictness-level',command)
                self.assertNotIn('--skip-gui-tests',command)
                if len(calls)==1:
                    self.assertEqual(command[0],str(validator))
                    if mutate: binary.write_bytes(b'changed')
                    return record(native_code)
                self.assertIn('-k', command)
                (root/'out/inferior.json').write_text(json.dumps({'state':debug_state,'exit_status':debug_exit}))
                return record()
            with patch.object(r,'execute',side_effect=execute):
                result=r.replay(root/'lldb',validator,component,root/'out')
            receipt=json.loads((root/'out/receipt.json').read_text())
            return result,calls,receipt
    def test_pair_clean(self):
        code,calls,receipt=self.run_pair(); self.assertEqual(code,0); self.assertEqual(len(calls),2)
    def test_native_failure_continues(self):
        code,calls,receipt=self.run_pair(native_code=9); self.assertEqual(code,1); self.assertEqual(len(calls),2)
    def test_mutation_blocks(self):
        code,calls,receipt=self.run_pair(mutate=True); self.assertEqual(code,1); self.assertEqual(len(calls),1)
        self.assertIn('comparison_blocked',receipt)
    def test_debugger_stop_never_pass(self):
        code,calls,receipt=self.run_pair(debug_state='stopped'); self.assertEqual(code,1)
    def test_conditional_inspector(self):
        script=r.inspector('/tmp/result.json'); compile(script,'inspector','exec')
        self.assertIn('if state in (lldb.eStateStopped',script)
    def test_execute_launch_failure(self):
        with tempfile.TemporaryDirectory() as d, patch.object(r.subprocess,'Popen',side_effect=OSError('denied')):
            self.assertIn('launch_error',r.execute(['lldb'],pathlib.Path(d)/'run',1))
    def test_execute_timeout_kills_group(self):
        class Proc:
            pid=1234
            calls=0
            def wait(self,timeout=None):
                self.calls+=1
                if self.calls==1: raise subprocess.TimeoutExpired('lldb',timeout)
                return -9
        with tempfile.TemporaryDirectory() as d, patch.object(r.subprocess,'Popen',return_value=Proc()), patch.object(r.os,'killpg') as kill:
            result=r.execute(['lldb'],pathlib.Path(d)/'run',1)
            self.assertTrue(result['timed_out']); kill.assert_called_once_with(1234,r.signal.SIGKILL)

if __name__=='__main__': unittest.main()

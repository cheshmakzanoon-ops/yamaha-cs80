import importlib.util, pathlib, tempfile, unittest
from unittest.mock import patch
import subprocess
s=importlib.util.spec_from_file_location('replay',pathlib.Path(__file__).with_name('replay.py'))
r=importlib.util.module_from_spec(s);s.loader.exec_module(r)
class Tests(unittest.TestCase):
    def check(self,text,code=0,timeout=False):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d);v=root/'pluginval';v.write_bytes(b'validator')
            c=root/'a.component';b=c/'Contents/MacOS/CS-80 Reference';b.parent.mkdir(parents=True);b.write_bytes(b'au')
            class Proc:
                pid=12345
                n=0
                def wait(self,timeout=None):
                    self.n+=1
                    if timeout is not None and self.n==1 and timed:
                        raise subprocess.TimeoutExpired('lldb',timeout)
                    return code
            timed=timeout
            def launch(command,**kwargs):
                self.assertIn('0x41dd1ce',command)
                self.assertNotIn('--skip-gui-tests',command)
                self.assertIn('thread backtrace all',command)
                self.assertEqual(command[command.index('--strictness-level')+1],'10')
                self.assertTrue(kwargs['start_new_session'])
                kwargs['stdout'].write(text.encode());kwargs['stdout'].flush()
                return Proc()
            with patch.object(r.subprocess,'Popen',side_effect=launch),patch.object(r.os,'killpg') as kill:
                result=r.replay(root/'lldb',v,c,root/'out',1)
                self.assertEqual(kill.called,timeout)
                return result
    def test_success_is_only_nonreproduction(self): self.assertEqual(self.check('SUCCESS\n'),0)
    def test_debugger_zero_is_not_pass(self): self.assertEqual(self.check('stopped'),1)
    def test_abort_never_pass(self): self.assertEqual(self.check('SUCCESS\nstop reason = breakpoint 1.1 abort'),1)
    def test_assertion_never_pass(self): self.assertEqual(self.check('SUCCESS\nAppleParavirtCommandBuffer _commandCurrentStorage.currentOffset'),1)
    def test_launch_error(self): self.assertEqual(self.check('error: launch denied',1),1)
    def test_timeout(self): self.assertEqual(self.check('SUCCESS\n',0,True),1)
if __name__=='__main__':unittest.main()

"""Bounded synthetic process tests, never project binaries or CPU measurements."""
import json,os,signal,subprocess,sys,tempfile,time,unittest
from pathlib import Path
from unittest.mock import patch
from orchestrate import execute_owned
CHILD="""import json,os,signal,subprocess,sys,time
p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'])
open(sys.argv[1],'w').write(json.dumps({'leader':os.getpid(),'child':p.pid,'alarm_blocked':signal.SIGALRM in signal.pthread_sigmask(signal.SIG_BLOCK,[])}))
if sys.argv[2]=='nonzero': sys.exit(7)
time.sleep(30)
"""
class Cleanup(unittest.TestCase):
    def scenario(self,mode):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);pidfile=root/'pids.json';record={};real_popen=subprocess.Popen
            def launch(*args,**kwargs):
                proc=real_popen(*args,**kwargs)
                if mode=='launch_alarm':
                    end=time.monotonic()+3
                    while not pidfile.exists() and time.monotonic()<end:time.sleep(.01)
                    if not pidfile.exists():
                        os.killpg(proc.pid,signal.SIGKILL);proc.wait();raise AssertionError('synthetic child did not start')
                    os.kill(os.getpid(),signal.SIGALRM) # queued while spawn assignment blocked
                return proc
            def alarm(*_):raise TimeoutError('synthetic deadline during launch')
            previous=signal.signal(signal.SIGALRM,alarm);originalmask=signal.pthread_sigmask(signal.SIG_BLOCK,[])
            try:
                with (root/'stdout').open('wb') as stdout,(root/'stderr').open('wb') as stderr,patch('orchestrate.subprocess.Popen',side_effect=launch):
                    args=dict(cwd=root,stdin=None,stdout=stdout,stderr=stderr,timeout=.5 if mode=='timeout' else 4,record=record)
                    argv=[sys.executable,'-c',CHILD,str(pidfile),mode]
                    if mode=='timeout':
                        with self.assertRaises(subprocess.TimeoutExpired):execute_owned(argv,**args)
                    elif mode=='launch_alarm':
                        with self.assertRaises(TimeoutError):execute_owned(argv,**args)
                    else:execute_owned(argv,**args);self.assertEqual(record['exit'],7)
                self.assertEqual(signal.pthread_sigmask(signal.SIG_BLOCK,[]),originalmask)
                data=json.loads(pidfile.read_text());self.assertFalse(data['alarm_blocked'])
                for pid in [data['leader'],data['child']]:
                    end=time.monotonic()+2
                    while time.monotonic()<end:
                        p=Path(f'/proc/{pid}/stat')
                        if not p.exists() or p.read_text().split()[2]=='Z':break
                        time.sleep(.01)
                    else:self.fail(f'synthetic descendant {pid} survived cleanup')
            finally:signal.signal(signal.SIGALRM,previous)
    def test_timeout_kills_group(self):self.scenario('timeout')
    def test_nonzero_leader_kills_descendant(self):self.scenario('nonzero')
    def test_alarm_during_spawn_assignment(self):self.scenario('launch_alarm')
if __name__=='__main__':unittest.main()

#!/usr/bin/env python3
"""Mock-only collector checks: no subprocess, DSP or real clock execution."""
from pathlib import Path
import contextlib,io,json,tempfile,types,sys
import collector
from schedule import make_schedule

def run_case(mode):
 with tempfile.TemporaryDirectory(prefix='native-average-mock-') as t:
  root=Path(t);original_root=collector.ROOT;collector.ROOT=root
  plan={'schedule':make_schedule()};(root/'plan.json').write_text(json.dumps(plan));(root/'freeze.json').write_text('{}')
  binary=root/'fake';binary.write_bytes(b'not executable; never launched')
  freeze={'loader_environment':{'PATH':'/usr/bin:/bin','LANG':'C','LC_ALL':'C','LD_LIBRARY_PATH':'/never-loaded'},'roles':{k:{'path':str(binary),'sha256':collector.sha(binary)} for k in ['host','stock','candidate']}}
  auth={'action':collector.ACTION,'plan_sha256':collector.sha(root/'plan.json'),'freeze_sha256':collector.sha(root/'freeze.json'),'exclusive_approved':True}
  if mode=='wrong_auth':auth['exclusive_approved']=False
  approval=root/'approval.json';approval.write_text(json.dumps(auth));n={'spawn':0,'tick':0}
  def monotonic():n['tick']+=1;return n['tick']*1000000
  def usage(who):return types.SimpleNamespace(ru_utime=0.,ru_stime=0.)
  class FakeProcess:
   def __init__(self,*args,**kw):self.pid=1000+n['spawn'];self.returncode=0;self.slot=n['spawn'];n['spawn']+=1
   def communicate(self,timeout=None):
    if mode=='interrupt' and self.slot==7 and timeout is not None:raise InterruptedError('synthetic interruption')
    return json.dumps({'slot':self.slot,'cpu_ns_sum':0}),''
  def validate(o,slot,f,**kw):
   if mode=='integrity' and slot['slot']==3:raise ValueError('synthetic identity mismatch')
   assert o['slot']==slot['slot']
  fake_contract=types.SimpleNamespace(command=lambda f,s,o,a:['never-launch',str(s['slot'])])
  saved=(collector.verify_freeze,collector.time.monotonic_ns,collector.resource.getrusage,collector.subprocess.Popen,collector.signal.signal,collector.os.killpg,collector.validate_output,sys.modules.get('host_contract'))
  collector.verify_freeze=lambda:freeze;collector.time.monotonic_ns=monotonic;collector.resource.getrusage=usage;collector.subprocess.Popen=FakeProcess;collector.signal.signal=lambda *a:None;collector.os.killpg=lambda *a:None;collector.validate_output=validate;sys.modules['host_contract']=fake_contract
  try:
   with contextlib.redirect_stdout(io.StringIO()):
    if mode=='wrong_auth':
     try:collector.collection(approval)
     except ValueError:pass
     else:raise AssertionError('bad authorization accepted')
     assert n['spawn']==0 and not (root/'trial-consumed.json').exists();return {'mode':mode,'passed':True,'mock_launches':0}
    result=collector.collection(approval)
   terminal=json.loads((root/'campaign/terminal.json').read_text());receipts=list((root/'campaign').glob('slot-*.json'))
   expected={'success':96,'integrity':4,'interrupt':8}[mode];assert n['spawn']==expected and len(receipts)==expected
   assert terminal['status']==('complete' if mode=='success' else 'failed')
   assert terminal['completed_slots']==(96 if mode=='success' else expected-1)
   before=n['spawn']
   try:collector.collection(approval)
   except (ValueError,FileExistsError):pass
   else:raise AssertionError('resume accepted')
   assert n['spawn']==before
   return {'mode':mode,'passed':True,'mock_launches':n['spawn'],'attempt_receipts':len(receipts),'resume_rejected':True}
  finally:
   collector.verify_freeze,collector.time.monotonic_ns,collector.resource.getrusage,collector.subprocess.Popen,collector.signal.signal,collector.os.killpg,collector.validate_output,old=saved
   if old is None:sys.modules.pop('host_contract',None)
   else:sys.modules['host_contract']=old
   collector.ROOT=original_root
if __name__=='__main__':print(json.dumps({'passed':True,'real_clock_calls':0,'real_subprocesses':0,'tests':[run_case(x) for x in ['success','integrity','interrupt','wrong_auth']]},indent=2))

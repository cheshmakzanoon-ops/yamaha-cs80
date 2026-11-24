#!/usr/bin/env python3
"""Inspect actual Intel Mac archives only after all original evidence is acquired.

Runs local byte/log verifiers, never downloaded products. All reports/logs use
exclusive creation; a failed inspection must be preserved before any deliberate
rerun. Original API authenticity is established by the separately recorded
GitHub connector acquisition and successful producer/upload gates.
"""
import hashlib,json,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[5]
DOWNLOADS=REPO.parent/'ci-c76-macos-x86_64-downloads'
SOURCE='c76ad288e02fbeca48f39652bf4fcdd99837f793'
RUN='45351731801'
TARGET='macos-x86_64'
VERIFIERS=REPO/'docs/preset-evidence/2026.11.5/verification'
PARSER=REPO/'docs/preset-evidence/2026.11.4/ci/source-078c098/parse-native-evidence.py'
F1=ROOT.parent/'release-gate-audit/bind-parser-archives.py'
PINS={VERIFIERS/'a41-verify-install-artifact.py':'803ff4378746937039400e777ff61bfce8ebaeb2ab1bba6699fe76076f520305',VERIFIERS/'a37-verify-macos-binding.py':'214a15276244a6ebfd57f66c66ccbef9678f2435ae2b9c9ca43aadbc92a8eda2',PARSER:'f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58',F1:'cada3812da3285e91116b0273b8bda0f4782bcc05f968e450dfb4ed2f540b682'}
def main():
 for p,h in PINS.items():
  assert hashlib.sha256(p.read_bytes()).hexdigest()==h, str(p)
 for name in ('run-attempt-api.json','jobs-api.json','artifacts-api.json','producer-job.log','install-api.json','pluginval-api.json','test-evidence-api.json'):
  assert (ROOT/name).is_file(), name
 for name in ('install.zip','pluginval.zip','test-evidence.zip'):
  assert (DOWNLOADS/name).is_file(), name
 config={'provenance':{'source':SOURCE,'tree':'416c1f6bfb4cd9f7dd5dbd9f5cbc211edda2bf00','run_id':int(RUN),'attempt':1,'factory_revision':'2026.11.5'},'native':[{'target':TARGET,'archive':str(DOWNLOADS/'test-evidence.zip')}],'validators':[{'target':TARGET,'archive':str(DOWNLOADS/'pluginval.zip')}]}
 with (ROOT/'parser-inputs.json').open('x') as f:json.dump(config,f,indent=2);f.write('\n')
 python=sys.executable
 commands=[
 ('install-check',[python,VERIFIERS/'a41-verify-install-artifact.py','--repo',REPO,'--source',SOURCE,'--revision','2026.11.5','--run-id',RUN,'--attempt','1','--run-api',ROOT/'run-attempt-api.json','--platform','macos','--architecture','x86_64','--archive',DOWNLOADS/'install.zip','--artifact-api',ROOT/'install-api.json','--out',ROOT/'install-acceptance.json']),
 ('payload-check',[python,VERIFIERS/'a37-verify-macos-binding.py','--repo',REPO,'--expected-source',SOURCE,'--expected-run',RUN,'--expected-attempt','1','--expected-architecture','x86_64','--expected-revision','2026.11.5','--install-zip',DOWNLOADS/'install.zip','--install-api',ROOT/'install-api.json','--pluginval-zip',DOWNLOADS/'pluginval.zip','--pluginval-api',ROOT/'pluginval-api.json','--out',ROOT/'payload-binding.json']),
 ('native-parser',[python,PARSER,'--inputs',ROOT/'parser-inputs.json','--out',ROOT/'native-validator-observations.json']),
 ('parser-archive-check',[python,F1,'--repo',REPO,'--source',SOURCE,'--run-id',RUN,'--attempt','1','--target',TARGET,'--observations',ROOT/'native-validator-observations.json','--native-zip',DOWNLOADS/'test-evidence.zip','--native-api',ROOT/'test-evidence-api.json','--validator-zip',DOWNLOADS/'pluginval.zip','--validator-api',ROOT/'pluginval-api.json','--out',ROOT/'parser-archive-binding.json']),
 ('original-execution-check',[python,ROOT/'qualify-macos-x86_64.py','--downloads',DOWNLOADS])]
 with (ROOT/'verification-commands.json').open('x') as f:json.dump([{'name':n,'argv':list(map(str,c))} for n,c in commands],f,indent=2);f.write('\n')
 for name,command in commands:
  with (ROOT/(name+'.log')).open('xb') as log:
   result=subprocess.run(list(map(str,command)),cwd=REPO,stdout=log,stderr=subprocess.STDOUT)
  with (ROOT/(name+'-exit.json')).open('x') as f:json.dump({'argv':list(map(str,command)),'exit_code':result.returncode},f,indent=2);f.write('\n')
  print(json.dumps({'check':name,'exit_code':result.returncode}),flush=True)
  if result.returncode:return result.returncode
 return 0
if __name__=='__main__':raise SystemExit(main())

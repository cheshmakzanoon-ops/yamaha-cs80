#!/usr/bin/env python3
"""Explicit, non-overwriting bounded qualification on an existing115 module."""
from pathlib import Path
import hashlib,json,subprocess,sys,os,struct,math
R=Path(__file__).resolve().parents[1];REC=R.parent
OUT=R/'docs/experiments/fx-modern-mix-automation';BUILD=R/'build-fx-modern-mix'
MODULE=REC/'yamaha-cs80-fx-mix-automation/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
RUNNER=BUILD/'fx-modern-mix'
BASE='2305421d50aab146cb348256234eaf71a2d0538e'
SDK=REC/'yamaha-cs80/build/_deps/juce-src/modules/juce_audio_processors_headless/format_types/VST3_SDK'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
freeze=json.loads((R/'tests/fx-modern-mix-source-freeze.json').read_text())
def verify():
 for p,h in freeze['sources'].items():assert sha(R/p)==h,('source changed',p)
 assert sha(MODULE)==freeze['module_sha256'],'fixed module changed'
 changed=subprocess.check_output(['git','diff','--name-only',BASE],cwd=R).decode().splitlines()
 old=set(subprocess.check_output(['git','ls-tree','-r','--name-only',BASE],cwd=R).decode().splitlines())
 assert not old.intersection(changed),'historical tracked files changed'
 for m in OUT.glob('*/complete.json'):
  d=json.loads(m.read_text())
  assert d['freeze_sha256']==sha(R/'tests/fx-modern-mix-source-freeze.json')
  for p,h in d['hashes'].items():assert sha(Path(p))==h,('completed file changed',p)
def command(args,log):
 env=os.environ.copy()
 raw=subprocess.check_output(['bash','-c','source "$1"; env -0','bash',str(REC/'yamaha-cs80-tuning-ref-automation/build/recovery-env.sh')])
 env.update(dict(x.decode().split('=',1) for x in raw.split(b'\0') if x))
 with log.open('x') as f:
  f.write('COMMAND '+repr([str(x) for x in args])+'\n');f.flush()
  subprocess.run([str(x) for x in args],cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=1800)
a=sys.argv[1:];assert a and a[0] in ('build','run','collect')
if a[0]=='run':assert len(a)==3 and a[1] in ('44100','48000','96000') and a[2] in ('realtime','offline')
else:assert len(a)==1
verify();name='-'.join(a);d=OUT/name;assert not d.exists(),('do not overwrite',d)
if a[0]=='run':assert (OUT/'build/complete.json').is_file()
if a[0]=='collect':assert all((OUT/('run-'+rate+'-'+m)/'complete.json').is_file() for rate in ('44100','48000','96000') for m in ('realtime','offline'))
d.mkdir(parents=True);result={'stage':name,'freeze_sha256':sha(R/'tests/fx-modern-mix-source-freeze.json')};files=[MODULE]
if a[0]=='build':
 BUILD.mkdir(exist_ok=False)
 command(['/usr/bin/c++','-std=c++20','-O3','-DNDEBUG','-I'+str(R/'src'),'-I'+str(SDK),R/'tests/Vst3FxModernMixTests.cpp',SDK/'pluginterfaces/base/coreiids.cpp',SDK/'pluginterfaces/base/funknown.cpp',SDK/'public.sdk/source/vst/vstinitiids.cpp','-ldl','-o',RUNNER],d/'build.log');files+=[RUNNER]
elif a[0]=='run':
 captures=BUILD/(a[1]+'-'+a[2])
 command([RUNNER,MODULE,captures,R/'tests/parity/tolerances-v2/host-automation.json',R/'tests/parity/tolerances-v2/host-automation-fx-modern-mix-v1.json',a[1],a[2]],d/'run.log')
 pcm=sorted(captures.glob('*.pcm'));assert len(pcm)==59
 files += [RUNNER,*pcm];result['captures']=59
else:
 metrics=[];captures={}
 host=json.loads((R/'tests/parity/tolerances-v2/host-automation.json').read_text())['host_automation']
 limits=json.loads((R/'tests/parity/tolerances-v2/host-automation-fx-modern-mix-v1.json').read_text())['host_automation_fx_modern_mix']
 def read(p):
  raw=p.read_bytes();assert len(raw)==131072
  v=struct.unpack('<32768f',raw);assert all(map(math.isfinite,v));captures[str(p)]=sha(p);return raw,v
 def delta(x,y):return max(abs(a-b) for a,b in zip(x,y))
 for rate in ('44100','48000','96000'):
  for mode in ('realtime','offline'):
   for s in range(4):
    suffix=['noqueue','null','preceding112','legacy','legacy-flat','pure','pure-fixed','scalar','constant',*[f'block{x}' for x in (7,64,256,1024)]]
    if s==3:suffix += [f'reverse{x}' for x in (7,64,256,1024)]+['held-lane0','held-lane1','held-lane2']
    data={x:read(BUILD/(rate+'-'+mode)/f'fx-case{s}-{x}.pcm') for x in suffix}
    for x,y in [('noqueue','null'),('legacy','legacy-flat'),('pure','pure-fixed')]:assert data[x][0]==data[y][0]
    v=data['scalar'][1];fixed=data['constant'][1]
    err=max(delta(v,data['block'+str(x)][1]) for x in (7,64,256,1024));assert err<=host['audio_max_abs_error']
    assert data['scalar'][0][:8192]==data['constant'][0][:8192]
    effect=delta(v[2048:28672],fixed[2048:28672]);assert effect>limits['minimum_held_effect']
    lanes=[]
    if s==3:
     for x in (7,64,256,1024):assert data['block'+str(x)][0]==data['reverse'+str(x)][0]
     lanes=[delta(v[2048:28672],data['held-lane'+str(i)][1][2048:28672]) for i in range(3)];assert min(lanes)>limits['minimum_held_effect']
    metrics.append(dict(rate=rate,mode=mode,scenario=s,max_error=err,held_effect=effect,per_lane_effects=lanes))
 assert len(captures)==354
 save(d/'captures.json',captures);result.update(captures=354,scalar_comparisons=96,queue_order_checks=24,initial_holds=24,metrics=metrics)
 files += [RUNNER,*map(Path,captures)]
verify();files+=list(d.glob('*'));result['hashes']={str(p):sha(p) for p in files};save(d/'complete.json',result);print('PASS',name)

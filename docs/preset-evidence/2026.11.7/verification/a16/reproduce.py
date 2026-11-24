#!/usr/bin/env python3
"""Reproduce A16: builds diagnostic cores only in /tmp; uses supplied production libraries.
Run from any directory, with --work /tmp/cs80-a16-repeat. Requires NumPy, c++, ar, git.
"""
import argparse,concurrent.futures,subprocess,pathlib,io,tarfile,shutil,json,hashlib
HERE=pathlib.Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,default=HERE.parents[4]);p.add_argument('--work',type=pathlib.Path,required=True);p.add_argument('--current-lib',type=pathlib.Path,default=pathlib.Path('/tmp/cs80-build-1cee5a/libcs80_core.a'));p.add_argument('--baseline-lib',type=pathlib.Path,default=pathlib.Path('/tmp/cs80-reference-1cee5a/libcs80_core.a'));a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True)
def run(command,**kw):subprocess.run(list(map(str,command)),check=True,**kw)
old=a.work/'old';old.mkdir(exist_ok=True)
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','archive','18711f93','src'],cwd=a.repo))) as t:t.extractall(old,filter='data')
def build(kind):
 if kind in ('frozen','noair'):
  base=a.work/kind;shutil.copytree(a.repo/'src',base/'src',dirs_exist_ok=True);h=base/'src/core/InstrumentBody.h';s=h.read_text()
  if kind=='frozen':before='return (model >= bowed && model <= brass) || vocal()\n            || (model >= clav && model <= bass)';after='return (model >= bowed && model < brass) || vocal()\n            || (model >= clav && model <= bass)'
  else:before='case brass: airAmount = 0.0009 + 0.0004 * strike;';after='case brass: airAmount = 0.0;'
  assert s.count(before)==1;h.write_text(s.replace(before,after));objects=[]
  for name in ('Engine','EngineWscHeld','FactoryPresets','Ig00156Batch'):
   obj=base/(name+'.o');flags=['-DCS80_ENABLE_BORROWED_HELD=1'] if name in ('Engine','EngineWscHeld') else ['-ffp-contract=off'] if name=='Ig00156Batch' else []
   run(['c++','-std=gnu++20','-O3','-DNDEBUG','-fPIC','-I',base/'src',*flags,'-c',base/'src/core'/f'{name}.cpp','-o',obj]);objects.append(obj)
  lib=base/'libcs80_core.a';run(['ar','rcs',lib,*objects]);inc=base/'src';suite='frozen' if kind=='frozen' else 'baseline'
 else:lib=a.baseline_lib if kind=='before' else a.current_lib;inc=old/'src' if kind=='before' else a.repo/'src';suite='baseline' if kind=='before' else 'current'
 exe=a.work/f'probe_{kind}';run(['c++','-std=c++20','-O2','-I',inc,HERE/'brass_probe.cpp',lib,'-o',exe])
 with (a.work/f'{kind}-render.log').open('w') as log:run([exe,a.work/kind/'pcm',suite],stdout=log)
 if kind in ('after','noair'):
  dry='dry-current' if kind=='after' else 'dry-noair'
  with (a.work/f'{dry}-render.log').open('w') as log:run([exe,a.work/dry/'pcm','dry'],stdout=log)
 return {'kind':kind,'library_sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'header_sha256':hashlib.sha256((inc/'core/InstrumentBody.h').read_bytes()).hexdigest()}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:binding=list(pool.map(build,['before','after','frozen','noair']))
(a.work/'binding.json').write_text(json.dumps(binding,indent=2)+'\n')
run(['python3',HERE/'analyze.py',a.work])

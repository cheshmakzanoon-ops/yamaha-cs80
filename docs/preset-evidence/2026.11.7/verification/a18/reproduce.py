#!/usr/bin/env python3
"""A18 strings review. Matching archived baseline headers are essential."""
import concurrent.futures,hashlib,io,json,pathlib,subprocess,tarfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[4]
TMP=pathlib.Path('/tmp/cs80-a18')
TMP.mkdir(exist_ok=True)
baseline='18711f93a39aa49f3d6f9a41784be456224facbe'
archive=subprocess.check_output(['git','archive',baseline,'src'],cwd=ROOT)
(TMP/'baseline-src').mkdir(exist_ok=True)
with tarfile.open(fileobj=io.BytesIO(archive)) as a:a.extractall(TMP/'baseline-src',filter='data')
bindings={}
def side_run(side):
 build=pathlib.Path('/tmp/cs80-build-1cee5a' if side=='current' else '/tmp/cs80-reference-1cee5a')
 headers=ROOT/'src' if side=='current' else TMP/'baseline-src/src'
 exe=TMP/f'probe-{side}'
 subprocess.run(['c++','-std=c++20','-O2','-I',str(headers),str(HERE/'strings_probe.cpp'),str(build/'libcs80_core.a'),'-o',str(exe)],check=True)
 subprocess.run([str(build/'cs80_preset_capture'),'--library','all','--inventory',str(TMP/f'{side}-inventory.json')],check=True)
 cmd=[str(build/'cs80_preset_capture'),'--library','all','--out',str(TMP/side/'capture'),'--wav','--rate','48000','--note','60','--velocity','.85','--hold','2','--tail','1']
 for name in ('Solo Violin','Solo Cello','Cellos','PWM Strings'):cmd+=['--preset',name]
 with (HERE/f'{side}-capture.log').open('w') as f:subprocess.run(cmd,stdout=f,check=True)
 with (HERE/f'{side}-probe.csv').open('w') as f:
  f.write('name,quality,scene,note,factor,bend_range,body1_size\n');f.flush()
  subprocess.run([str(exe),str(TMP/side/'gestures')],stdout=f,check=True)
 bindings[side]={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (build/'cs80_preset_capture',build/'libcs80_core.a',headers/'core/Engine.h',headers/'core/InstrumentBody.h',exe)}
 print(side,'complete',flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(side_run,('current','baseline')))
(HERE/'binding.json').write_text(json.dumps({'baseline':baseline,'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'bindings':bindings},indent=2)+'\n')
subprocess.run(['python3',str(HERE/'analyze.py')],check=True)

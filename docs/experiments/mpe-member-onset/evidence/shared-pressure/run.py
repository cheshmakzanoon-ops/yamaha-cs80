import json,pathlib,subprocess,hashlib
r=pathlib.Path.cwd(); out=r/'mpe-onset-evidence/shared-pressure'; records=[]
for profile,flags in [('generic',[]),('native',['-march=native'])]:
 exe=out/f'probe-{profile}'
 argv=['g++','-std=c++20','-O3','-DNDEBUG','-Wall','-Wextra','-Werror','-DCS80_DISABLE_AVX2_FILTER_BATCH=1',*flags,'-I'+str(r/'next-product-step/src'),str(out/'SharedPressureOnsetProbe.cpp'),str(r/f'build-mpe-onset-gcc-{profile}/libcs80_core.a'),'-o',str(exe)]
 for stage,command in [('build',argv),('run',[str(exe)])]:
  logfile=out/f'{profile}-{stage}.log'
  with logfile.open('w') as f: p=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
  records.append(dict(profile=profile,stage=stage,argv=command,status=p.returncode,log=logfile.name,sha256=hashlib.sha256(logfile.read_bytes()).hexdigest()))
  (out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
  if p.returncode: print(logfile.read_text()); raise SystemExit(p.returncode)
 print((out/f'{profile}-run.log').read_text(),end='')

import json,shlex,subprocess
from pathlib import Path
p=Path(__file__).resolve().parent;r=p.parent;b=r/'hybrid-baseline-automation-build'
e=next(x for x in json.load(open(b/'compile_commands.json')) if x['file'].endswith('/tests/Vst3AutomationTests.cpp'))
a=shlex.split(e['command']);a[a.index('-o')+1]=str(p/'test.o');a[-1]=str(p/'Vst3AutomationTests.cpp');a.insert(1,'-I'+str(r/'audio-root-recovery/tests'))
with open(p/'build-commands.txt','w') as f:f.write(shlex.join(a)+'\n')
with open(p/'compile.log','w') as f:s=subprocess.run(a,cwd=e['directory'],stdout=f,stderr=subprocess.STDOUT).returncode
(p/'compile-exit.txt').write_text(str(s));assert s==0
link=shlex.split((b/'CMakeFiles/cs80_vst3_automation_tests.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(p/'test')
link=[str(p/'test.o') if x.endswith('/tests/Vst3AutomationTests.cpp.o') else x for x in link]
with open(p/'build-commands.txt','a') as f:f.write(shlex.join(link)+'\n')
with open(p/'link.log','w') as f:s=subprocess.run(link,cwd=b,stdout=f,stderr=subprocess.STDOUT).returncode
(p/'link-exit.txt').write_text(str(s));assert s==0

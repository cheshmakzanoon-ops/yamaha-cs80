import json,shlex,subprocess,os
from pathlib import Path
p=Path(__file__).resolve().parent;r=p.parent;b=r/'hybrid-full-build'
for file,target in [('ModernStateNotificationTests.cpp','cs80_modern_state_notification_tests'),('StudioStateSyncTests.cpp','cs80_studio_state_sync_tests')]:
 e=next(x for x in json.load(open(b/'compile_commands.json')) if x['file'].endswith('/tests/'+file))
 a=shlex.split(e['command']);a[a.index('-o')+1]=str(p/(target+'.o'));a[-1]=str(p/file)
 with open(p/'build-commands.txt','a') as f:f.write(shlex.join(a)+'\n')
 with open(p/(target+'-compile.log'),'w') as f:s=subprocess.run(a,cwd=e['directory'],stdout=f,stderr=subprocess.STDOUT).returncode
 assert s==0,s
 link=shlex.split((b/f'CMakeFiles/{target}.dir/link.txt').read_text());link=[x for x in link if not x.startswith('-Wl,--dependency-file=')];link[link.index('-o')+1]=str(p/target)
 link=[str(p/(target+'.o')) if x.endswith('/tests/'+file+'.o') else x for x in link]
 with open(p/'build-commands.txt','a') as f:f.write(shlex.join(link)+'\n')
 with open(p/(target+'-link.log'),'w') as f:s=subprocess.run(link,cwd=b,stdout=f,stderr=subprocess.STDOUT).returncode
 assert s==0,s

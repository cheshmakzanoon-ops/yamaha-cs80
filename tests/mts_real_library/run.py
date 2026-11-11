#!/usr/bin/env python3
import os,sys,subprocess,select,time,hashlib,json
from pathlib import Path
if sys.flags.optimize:
 raise SystemExit("REFUSE optimized Python: run without -O/PYTHONOPTIMIZE")
E=Path(__file__).resolve().parent
shared=Path('/dev/shm/libMTSShared')
manifest={}
for line in (E/'postbuild.sha256').read_text().splitlines():
 expected,name=line.split('  ',1)
 assert name in {'master.cpp','client.cpp','prepare.py','run.py','build.sh','master','client'},'unexpected manifest member'
 assert name not in manifest,'duplicate manifest member'
 manifest[name]=expected
assert set(manifest)=={'master.cpp','client.cpp','prepare.py','run.py','build.sh','master','client'},'incomplete postbuild manifest'
for name,expected in manifest.items():
 assert hashlib.sha256((E/name).read_bytes()).hexdigest()==expected,'changed compiled artifact or harness: '+name
print('POSTBUILD verified harness sources/scripts and both executable hashes',flush=True)
config=json.loads((E/'config.json').read_text())
assert hashlib.sha256(Path(config['library']).read_bytes()).hexdigest()=='03d23cb8d64ae246ceb416a7323cb230c5748c4b58d52a7c288d2c93ea3b7de5'
for line in (E/'source-hashes.txt').read_text().splitlines():
 expected,path=line.split('  ',1)
 assert hashlib.sha256(Path(path).read_bytes()).hexdigest()==expected,'changed input: '+path
for p in [shared,Path('/usr/local/lib/libMTS.so'),Path('/usr/local/etc/MTS-ESP.conf'),Path('/Library/Application Support/MTS-ESP/libMTS.dylib'),Path('/Library/Application Support/MTS-ESP/MTS-ESP.conf')]:
 if os.path.lexists(p): raise SystemExit('REFUSE pre-existing state/config/library '+str(p))
for name in ['LD_PRELOAD','LD_LIBRARY_PATH','LD_AUDIT']:
 if os.environ.get(name): raise SystemExit('REFUSE loader environment '+name)
print('PRECHECK all shared state, system library, configuration, loader override absent',flush=True)
processes=[]
def read(p):
 if not select.select([p.stdout],[],[],10)[0]: raise RuntimeError('timeout waiting for '+str(p.args))
 line=p.stdout.readline().strip();print(str(p.args)+' > '+line,flush=True)
 if not line: raise RuntimeError('unexpected EOF '+str(p.args))
 return line
def send(p,cmd,expected):
 print(str(p.args)+' < '+cmd,flush=True);p.stdin.write(cmd+'\n');p.stdin.flush();line=read(p)
 if line!=expected: raise RuntimeError('expected '+expected+' got '+line)
def start(name):
 p=subprocess.Popen([str(E/name)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=sys.stderr,text=True,bufsize=1);processes.append(p);print('START '+name+' pid='+str(p.pid),flush=True);return p
try:
 m=start('master');assert read(m)=='READY IPC=1 clients=0'
 # The second executable is allowed to join only the shared state just created by our live master.
 assert shared.exists(),'master reported IPC but shared object missing'
 stat=shared.stat();print('OWNED SHM after master: inode=%s uid=%s size=%s'%(stat.st_ino,stat.st_uid,stat.st_size),flush=True)
 assert stat.st_uid==os.getuid()
 c=start('client');assert read(c)=='READY'
 send(m,'clients','CLIENTS 1');send(c,'initial','PASS initial')
 for cmd in ['update','badglobal','filterbadglobal','badchannel','filterbadchannel','disconnect','reconnect']:
  send(m,cmd,'OK '+cmd);send(c,cmd,'PASS '+cmd)
 send(c,'quit','EXIT');rc=c.wait(timeout=10);print('EXIT client pid='+str(c.pid)+' code='+str(rc),flush=True);assert rc==0
 send(m,'clients','CLIENTS 0');send(m,'quit','EXIT');rc=m.wait(timeout=10);print('EXIT master pid='+str(m.pid)+' code='+str(rc),flush=True);assert rc==0
 assert not os.path.lexists(shared),'shared object retained after orderly exit'
 print('PASS all separate-process qualification stages',flush=True)
finally:
 for p in reversed(processes):
  if p.poll() is None:
   try:p.stdin.close();p.wait(timeout=10)
   except Exception as error:print('BLOCKED orderly exit: '+str(error),flush=True)
 print('CLEANUP shared object absent='+str(not os.path.lexists(shared)),flush=True)
 if os.path.lexists(shared): print('STOP: retained shared object; no manual unlink or reinitialize attempted',flush=True)

#!/usr/bin/env python3
"""Explicit opt-in, bounded stock-only replay. No timing or candidate DSP."""
import argparse,json,os,resource,shutil,signal,subprocess,time
from pathlib import Path
import verify as v
P=Path(__file__).resolve().parent
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 ap.add_argument('--repo',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
 ap.add_argument('--stock-core',type=Path);ap.add_argument('--cxx',default='c++');ap.add_argument('--execute',action='store_true')
 a=ap.parse_args();p,s=v.check_package();repo=a.repo.resolve();retained=v.check_repo(repo,p,s)
 if not a.execute:print('Inputs verified only. Add --execute to compile/render in a fresh output directory.');return
 out=a.out.resolve();out.mkdir(exist_ok=False);src=repo/'src';observed=out/'observed-src';shutil.copytree(src,observed)
 (observed/'core/BbdChorus.h').write_bytes(v.observed_header((src/'core/BbdChorus.h').read_bytes()))
 for name in ['Observer.cpp','driver.cpp']:shutil.copyfile(P/name,out/name)
 (out/'retained-cost.cpp').write_bytes(retained);(out/'fixture').mkdir()
 for name,label in [('source_factory_benchmark.cpp','fixture'),('CallbackCpuMeasurement.h','clock_header')]:shutil.copyfile(repo/p['committed_inputs'][label],out/'fixture'/name)
 art=out/'artifacts';art.mkdir();start=time.monotonic();commands=[]
 def run(label,args):
  remaining=420-(time.monotonic()-start);v.need(remaining>5,'execution budget')
  def cap():
   resource.setrlimit(resource.RLIMIT_CPU,(65,65));resource.setrlimit(resource.RLIMIT_AS,(4*1024**3,4*1024**3));resource.setrlimit(resource.RLIMIT_FSIZE,(100*1024**2,100*1024**2));resource.setrlimit(resource.RLIMIT_CORE,(0,0))
  with (art/(label+'.out')).open('xb')as stdout,(art/(label+'.err')).open('xb')as stderr:
   child=subprocess.Popen(list(map(str,args)),stdout=stdout,stderr=stderr,preexec_fn=cap,start_new_session=True)
   try:code=child.wait(timeout=min(65,remaining))
   except BaseException:os.killpg(child.pid,signal.SIGKILL);child.wait();raise
  commands.append({'label':label,'exit':code});(out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');v.need(code==0,'command failed: '+label)
 flags=p['flags']
 if a.stock_core:
  archive=a.stock_core.resolve();v.need(v.SHA(archive.read_bytes())==p['stock_archive_sha256'],'stock archive')
 else:
  objects=[]
  for stem in ['Engine','FactoryPresets','Ig00156Batch']:
   obj=art/(stem+'.cpp.o');objects.append(obj)
   extra=p['Ig00156Batch_extra_flags']if stem=='Ig00156Batch'else []
   run('stock-'+stem,[a.cxx,*flags,*extra,'-I'+str(src),'-c',src/'core'/(stem+'.cpp'),'-o',obj])
   v.need(v.SHA(obj.read_bytes())==p['stock_objects_sha256'][obj.name],'stock object build mismatch: '+stem)
  archive=art/'libcs80_core.a';run('archive',['ar','rcsD',archive,*objects]);v.need(v.SHA(archive.read_bytes())==p['stock_archive_sha256'],'complete stock archive mismatch')
 run('observed-Engine',[a.cxx,*flags,'-I'+str(observed),'-c',observed/'core/Engine.cpp','-o',art/'ObservedEngine.o'])
 run('driver',[a.cxx,*flags,'-fno-access-control','-I'+str(src),'-c',out/'driver.cpp','-o',art/'driver.o'])
 run('observer',[a.cxx,*flags,'-c',out/'Observer.cpp','-o',art/'Observer.o'])
 run('link-stock',[a.cxx,art/'driver.o',art/'Observer.o',archive,'-o',art/'stock'])
 run('link-observed',[a.cxx,art/'driver.o',art/'Observer.o',art/'ObservedEngine.o',archive,'-o',art/'observed'])
 expected=v.load(P/'counts.json');results={}
 for scene in ['factory-held','factory-moving','panel-moving','pure-held']:
  for arm in ['stock','observed']:run(scene+'-'+arm,[art/arm,scene,art/(scene+'-'+arm+'.bin')])
  b=(art/(scene+'-stock.bin')).read_bytes();c=(art/(scene+'-observed.bin')).read_bytes();v.need(b==c,'STOP: semantic mismatch: '+scene)
  observed_counts=v.load(art/(scene+'-observed.out'));v.need(observed_counts==expected['common_counts_for_every_scene'],'STOP: census changed: '+scene)
  v.need(len(b)==expected['parity'][scene]['bytes_per_arm'],'trace size')
  results[scene]={'exact':True,'bytes_per_arm':len(b),'sha256':v.SHA(b),'counts':observed_counts,'same_as_retained_capture':v.SHA(b)==expected['parity'][scene]['sha256']}
  v.need(results[scene]['same_as_retained_capture'],'STOP: retained capture mismatch: '+scene)
  (out/'result.json').write_text(json.dumps(results,indent=2)+'\n')
 print('PASS: fresh stock-only parity and cursor census; no timing.')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Opt-in independent reconstruction on Linux x86-64; default replay is verify.py."""
from pathlib import Path
import argparse, csv, hashlib, json, os, platform, resource, shutil, subprocess
P=Path(__file__).resolve().parent
H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--run-new-study',action='store_true');a.add_argument('--src',type=Path,required=True,help='frozen shipping src directory');a.add_argument('--out',type=Path,required=True,help='new, nonexisting directory');a.add_argument('--semantic-only',action='store_true');v=a.parse_args()
    assert v.run_new_study and platform.system()=='Linux' and platform.machine()=='x86_64';src=v.src.resolve();out=v.out.resolve();e=json.loads((P/'evidence.json').read_text());assert all(H(src/f)==s for f,s in e['stock_source_files'].items());out.mkdir(exist_ok=False)
    flags=['-O3','-DNDEBUG','-std=gnu++20','-fPIC','-Wall','-Wextra','-Wpedantic'];cxx=os.environ.get('CXX','c++');print(subprocess.check_output([cxx,'--version'],text=True).splitlines()[0],flush=True)
    fixture=P.parent/'lp-cross-lane-reuse/cost/fixture';assert all(H(fixture/f)==s for f,s in e['fixture_sha256'].items());shutil.copytree(fixture,out/'fixture');shutil.copyfile(P/'census.cpp',out/'census.cpp');shutil.copyfile(P/'reset-boundary.cpp',out/'reset-boundary.cpp')
    prefix=(P/'census.cpp').read_bytes()[:e['semantic_prefix_bytes']];assert hashlib.sha256(prefix).hexdigest()==e['semantic_prefix_sha256'];(out/'frozen-observer-prefix.inc').write_bytes(prefix)
    def invoke(args,*,cwd=out,log,inp=None,cpu=100,wall=120):
        def cap():resource.setrlimit(resource.RLIMIT_CPU,(cpu,cpu));resource.setrlimit(resource.RLIMIT_AS,(4*1024**3,4*1024**3));resource.setrlimit(resource.RLIMIT_FSIZE,(128*1024**2,128*1024**2))
        print(log,flush=True)
        with (out/log).open('xb') as f:subprocess.run(list(map(str,args)),cwd=cwd,input=inp,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=wall,preexec_fn=cap)
    def patch(work,filename):invoke(['patch','--fuzz=0','-p1'],cwd=work,log=work.name+'-'+filename+'.log',inp=(P/filename).read_bytes())
    for arm in ['reference','observed']:
        ws=out/(arm+'-src');shutil.copytree(src,ws);patch(ws,'test-friends.patch')
        if arm=='observed':patch(ws,'instrumentation.patch');shutil.copyfile(P/'WscCensusHooks.h',ws/'core/WscCensusHooks.h')
        assert all(H(ws/f)==s for f,s in e['reference_observed_inputs'][arm].items());invoke([cxx,*flags,'-I'+str(ws),'-c',ws/'core/Engine.cpp','-o',arm+'-Engine.o'],log=arm+'-Engine.log')
    for member in ['FactoryPresets','Ig00156Batch']:
        invoke([cxx,*flags,*(['-ffp-contract=off'] if member=='Ig00156Batch' else []),'-I'+str(src),'-c',src/f'core/{member}.cpp','-o',member+'.cpp.o'],log=member+'.log')
    # Same three production members and order as the original sealed stock archive.
    shutil.copyfile(out/'reference-Engine.o',out/'Engine.cpp.o');invoke(['ar','rcs','libcs80_core.a','Engine.cpp.o','FactoryPresets.cpp.o','Ig00156Batch.cpp.o'],log='stock-archive.log')
    identities={s:H(out/f) for s,f in [('archive_sha256','libcs80_core.a'),('reference_engine_object_sha256','reference-Engine.o'),('instrumented_engine_object_sha256','observed-Engine.o')]};(out/'rebuild-identities.json').write_text(json.dumps(identities,indent=2)+'\n');print('Matches recorded toolchain objects:',all(e[k]==s for k,s in identities.items()),flush=True)
    for arm in ['reference','observed']:
        inc='-I'+str(out/(arm+'-src'));link=[arm+'-Engine.o','libcs80_core.a'];invoke([cxx,*flags,inc,'-DSEMANTIC_OBSERVED='+str(int(arm=='observed')),'reset-boundary.cpp',*link,'-o',arm+'-reset'],log=arm+'-reset-build.log');invoke([out/(arm+'-reset'),arm+'-reset.bin'],log=arm+'-reset.log',cpu=20,wall=30)
        if not v.semantic_only:invoke([cxx,*flags,inc,'census.cpp',*link,'-o',arm+'-census'],log=arm+'-census-build.log')
    assert (out/'reference-reset.bin').read_bytes()==(out/'observed-reset.bin').read_bytes();print((out/'observed-reset.log').read_text().strip());assert (out/'reference-reset.bin').stat().st_size==e['real_reset_control']['bytes_per_arm'];assert H(out/'reference-reset.bin')==e['real_reset_control']['sha256'],'different recorded semantic bytes; retain fresh output'
    if v.semantic_only:return
    with (P/'observed-counts.csv').open() as f:expected=list(csv.DictReader(f))
    for sc in e['scenes']:
        for arm in ['reference','observed']:invoke([out/(arm+'-census'),sc,arm+'-'+sc],log=arm+'-'+sc+'.csv',cpu=35,wall=50)
        with (out/('observed-'+sc+'.csv')).open() as f:assert list(csv.DictReader(f))==[r for r in expected if r['scene']==sc]
        for x in (r for r in e['stream_receipts'] if r['scene']==sc):
            a=out/('reference-'+sc+x['stream']);b=out/('observed-'+sc+x['stream']);assert a.read_bytes()==b.read_bytes() and a.stat().st_size==x['bytes'] and H(a)==x['reference_sha256'],'new stream mismatch; retain all prior output'
    invoke([out/'observed-census','factory_moving','perturb','perturb'],log='perturb.csv',cpu=35,wall=50)
    for suf in ('.trace.bin','.wsc.bin'):assert (out/('perturb'+suf)).read_bytes()==(out/('observed-factory_moving'+suf)).read_bytes()
    with (out/'perturb.csv').open() as f:mut=list(csv.DictReader(f));ref=[r for r in expected if r['scene']=='factory_moving'];dif=[(x['window'],x['group'],k,x[k],y[k]) for x,y in zip(ref,mut) for k in x if x[k]!=y[k]];assert len(ref)==len(mut)==8 and dif==[('late','0','terminal_ordinary_nodes_packs','65536','65537')]
    print('New untimed reconstruction agrees with the historical counts and trace receipts.')
if __name__=='__main__':main()
